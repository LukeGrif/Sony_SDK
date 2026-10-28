#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "SonyCamApi.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include "CRSDK/CameraRemote_SDK.h"
#include "CameraDevice.h"
#include "Text.h"

namespace SDK = SCRSDK;
using namespace std::chrono_literals;

namespace
{
    // ---------- UTF-8 <-> cli::text ----------
    std::string to_utf8(const std::string &s) { return s; }

#if defined(_WIN32) || defined(_WIN64)
    std::string to_utf8(const std::wstring &s)
    {
        if (s.empty())
            return std::string();
        int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n, nullptr, nullptr);
        return out;
    }
#endif

    cli::text from_utf8(const char *s)
    {
        if (s == nullptr)
            return cli::text();
#if defined(_MSC_VER) && (defined(UNICODE) || defined(_UNICODE))
        int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        if (n <= 1)
            return cli::text();
        std::wstring out(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], n);
        return out;
#else
        return cli::text(s);
#endif
    }

    // Copy a string into a caller buffer (always NUL terminated, truncated
    // if needed). Returns the full length so the caller can retry.
    int copy_out(const std::string &s, char *buf, int buf_len)
    {
        if (buf != nullptr && buf_len > 0)
        {
            size_t n = std::min(s.size(), (size_t)buf_len - 1);
            std::memcpy(buf, s.data(), n);
            buf[n] = '\0';
        }
        return (int)s.size();
    }

    // ---------- Log redirection ----------
    // Replaces cli::tout's buffer so everything CameraDevice prints is handed
    // to the host one line at a time instead of going to a console.
    class LineCallbackBuf : public std::basic_streambuf<cli::text_char>
    {
    public:
        void set_callback(sonycam_text_callback cb)
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_cb = cb;
        }

    protected:
        int_type overflow(int_type ch) override
        {
            if (traits_type::eq_int_type(ch, traits_type::eof()))
                return traits_type::not_eof(ch);
            cli::text_char c = traits_type::to_char_type(ch);
            put(&c, 1);
            return ch;
        }

        std::streamsize xsputn(const cli::text_char *s, std::streamsize n) override
        {
            put(s, n);
            return n;
        }

    private:
        void put(const cli::text_char *s, std::streamsize n)
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            for (std::streamsize i = 0; i < n; ++i)
            {
                if (s[i] == '\n')
                {
                    if (m_cb && !m_line.empty())
                        m_cb(to_utf8(m_line).c_str());
                    m_line.clear();
                }
                else if (s[i] != '\r')
                {
                    m_line.push_back(s[i]);
                }
            }
        }

        std::mutex m_mtx;
        sonycam_text_callback m_cb = nullptr;
        cli::text m_line;
    };

    LineCallbackBuf g_log_buf;
    std::basic_streambuf<cli::text_char> *g_original_tout_buf = nullptr;

    // ---------- Camera state ----------
    std::mutex g_mtx; // serialises every API call
    bool g_sdk_ready = false;
    std::shared_ptr<cli::CameraDevice> g_camera;

    std::mutex g_download_cb_mtx;
    sonycam_text_callback g_download_cb = nullptr;
    sonycam_event_callback g_capture_cb = nullptr;

    void notify_download(const cli::text &path)
    {
        std::lock_guard<std::mutex> lk(g_download_cb_mtx);
        if (g_download_cb)
            g_download_cb(to_utf8(path).c_str());
    }

    void notify_capture()
    {
        std::lock_guard<std::mutex> lk(g_download_cb_mtx);
        if (g_capture_cb)
            g_capture_cb();
    }

    void log_line(const std::string &line)
    {
        cli::tout << from_utf8(line.c_str()) << '\n';
    }

    bool connected()
    {
        return g_camera && g_camera->is_connected();
    }

    void disconnect_locked()
    {
        if (!g_camera)
            return;
        g_camera->set_download_listener(nullptr);
        g_camera->set_capture_listener(nullptr);
        if (g_camera->is_connected())
        {
            g_camera->disconnect();
            for (int i = 0; i < 30 && g_camera->is_connected(); ++i)
                std::this_thread::sleep_for(100ms);
        }
        g_camera->release();
        g_camera.reset();
    }

    bool to_setting(int setting, cli::CameraDevice::Setting &out)
    {
        switch (setting)
        {
        case SONYCAM_SETTING_ISO: out = cli::CameraDevice::Setting::Iso; return true;
        case SONYCAM_SETTING_SHUTTER_SPEED: out = cli::CameraDevice::Setting::ShutterSpeed; return true;
        case SONYCAM_SETTING_APERTURE: out = cli::CameraDevice::Setting::Aperture; return true;
        case SONYCAM_SETTING_EXPOSURE_MODE: out = cli::CameraDevice::Setting::ExposureMode; return true;
        case SONYCAM_SETTING_FOCUS_MODE: out = cli::CameraDevice::Setting::FocusMode; return true;
        default: return false;
        }
    }
} // namespace

extern "C"
{
    SONYCAM_API int sonycam_api_version(void)
    {
        return SONYCAM_API_VERSION;
    }

    SONYCAM_API void sonycam_set_log_callback(sonycam_text_callback cb)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_log_buf.set_callback(cb);
        if (cb)
        {
            if (g_original_tout_buf == nullptr)
                g_original_tout_buf = cli::tout.rdbuf(&g_log_buf);
            cli::tout.clear();
        }
        else if (g_original_tout_buf != nullptr)
        {
            cli::tout.rdbuf(g_original_tout_buf);
            g_original_tout_buf = nullptr;
        }
    }

    SONYCAM_API void sonycam_set_download_callback(sonycam_text_callback cb)
    {
        std::lock_guard<std::mutex> lk(g_download_cb_mtx);
        g_download_cb = cb;
    }

    SONYCAM_API void sonycam_set_capture_callback(sonycam_event_callback cb)
    {
        std::lock_guard<std::mutex> lk(g_download_cb_mtx);
        g_capture_cb = cb;
    }

    SONYCAM_API int sonycam_init(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_sdk_ready)
            return SONYCAM_OK;
        if (!SDK::Init())
        {
            log_line("SDK init failed.");
            return SONYCAM_ERR_NOT_INIT;
        }
        g_sdk_ready = true;
        return SONYCAM_OK;
    }

    SONYCAM_API void sonycam_shutdown(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        disconnect_locked();
        if (g_sdk_ready)
        {
            SDK::Release();
            g_sdk_ready = false;
        }
    }

    SONYCAM_API int sonycam_connect(int camera_index, int timeout_ms)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!g_sdk_ready)
            return SONYCAM_ERR_NOT_INIT;
        if (camera_index < 0)
            return SONYCAM_ERR_ARG;
        disconnect_locked();

        log_line("Connecting to camera...");
        SDK::ICrEnumCameraObjectInfo *camera_list = nullptr;
        if (CR_FAILED(SDK::EnumCameraObjects(&camera_list)) || camera_list == nullptr)
        {
            log_line("No cameras found. Check the camera is connected, powered on and not in use by another application.");
            return SONYCAM_ERR_NO_CAMERA;
        }

        CrInt32u count = camera_list->GetCount();
        for (CrInt32u i = 0; i < count; ++i)
        {
            auto info = camera_list->GetCameraObjectInfo(i);
            cli::tout << "Found camera [" << i + 1 << "] " << info->GetModel() << '\n';
        }
        if ((CrInt32u)camera_index >= count)
        {
            camera_list->Release();
            return SONYCAM_ERR_NO_CAMERA;
        }

        auto camera = std::make_shared<cli::CameraDevice>(camera_index + 1, camera_list->GetCameraObjectInfo(camera_index));
        camera_list->Release();

        camera->set_download_listener(notify_download);
        camera->set_capture_listener(notify_capture);
        if (!camera->connect(SDK::CrSdkControlMode_Remote, SDK::CrReconnecting_ON))
        {
            camera->set_download_listener(nullptr);
            camera->set_capture_listener(nullptr);
            return SONYCAM_ERR_CONNECT;
        }

        // OnConnected arrives asynchronously.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (!camera->is_connected() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(50ms);

        g_camera = camera;
        if (!g_camera->is_connected())
        {
            log_line("Timed out waiting for the camera to connect.");
            disconnect_locked();
            return SONYCAM_ERR_CONNECT;
        }
        return SONYCAM_OK;
    }

    SONYCAM_API void sonycam_disconnect(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        disconnect_locked();
    }

    SONYCAM_API int sonycam_is_connected(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        return connected() ? 1 : 0;
    }

    SONYCAM_API int sonycam_get_model(char *buf, int buf_len)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!g_camera)
            return SONYCAM_ERR_NOT_CONNECTED;
        return copy_out(to_utf8(g_camera->get_model()), buf, buf_len);
    }

    SONYCAM_API int sonycam_set_save_info(const char *dir, const char *prefix, int start_no)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        return g_camera->set_save_info(from_utf8(dir), from_utf8(prefix), start_no) ? SONYCAM_OK : SONYCAM_ERR_FAILED;
    }

    SONYCAM_API int sonycam_set_store_destination(int index)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        if (index < 0 || index > 2)
            return SONYCAM_ERR_ARG;
        g_camera->set_still_image_store_destination(index);
        return SONYCAM_OK;
    }

    SONYCAM_API int sonycam_get_store_destination(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        int index = g_camera->get_still_image_store_destination();
        return index < 0 ? SONYCAM_ERR_FAILED : index;
    }

    SONYCAM_API int sonycam_capture(void)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        g_camera->capture_image();
        return SONYCAM_OK;
    }

    SONYCAM_API int sonycam_get_live_view(unsigned char *buf, int buf_len)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        if (buf == nullptr || buf_len <= 0)
            return SONYCAM_ERR_ARG;

        std::vector<std::uint8_t> jpeg;
        auto err = g_camera->get_live_view_jpeg(jpeg);
        if (err == SDK::CrWarning_Frame_NotUpdated)
            return 0;
        if (CR_FAILED(err))
            return SONYCAM_ERR_FAILED;
        if (jpeg.size() > (size_t)buf_len)
            return SONYCAM_ERR_BUFFER;
        std::memcpy(buf, jpeg.data(), jpeg.size());
        return (int)jpeg.size();
    }

    SONYCAM_API int sonycam_get_setting(int setting, char *current, int current_len,
                                        char *options, int options_len, int *writable)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        cli::CameraDevice::Setting which;
        if (!to_setting(setting, which))
            return SONYCAM_ERR_ARG;

        cli::text cur;
        std::vector<cli::text> opts;
        bool can_write = false;
        if (!g_camera->get_setting_options(which, cur, opts, can_write))
            return SONYCAM_ERR_FAILED;

        std::string joined;
        for (size_t i = 0; i < opts.size(); ++i)
        {
            if (i)
                joined += '\n';
            joined += to_utf8(opts[i]);
        }
        if (writable)
            *writable = can_write ? 1 : 0;
        int need_cur = copy_out(to_utf8(cur), current, current_len);
        int need_opts = copy_out(joined, options, options_len);
        if (need_cur >= current_len || need_opts >= options_len)
            return SONYCAM_ERR_BUFFER;
        return SONYCAM_OK;
    }

    SONYCAM_API int sonycam_set_setting(int setting, int index)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!connected())
            return SONYCAM_ERR_NOT_CONNECTED;
        cli::CameraDevice::Setting which;
        if (!to_setting(setting, which) || index < 0)
            return SONYCAM_ERR_ARG;
        return g_camera->set_setting(which, index) ? SONYCAM_OK : SONYCAM_ERR_FAILED;
    }
} // extern "C"
