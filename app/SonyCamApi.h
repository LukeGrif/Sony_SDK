#ifndef SONYCAMAPI_H
#define SONYCAMAPI_H

// Plain C interface to CameraDevice, built as the SonyCam shared library
// (SonyCam.dll / libSonyCam.so) so a host program such as the Python GUI can
// drive the camera in-process (e.g. via ctypes) instead of launching
// RemoteCli.exe and scraping its console output.
//
// Rules:
//   * All strings are UTF-8, NUL terminated.
//   * Functions return SONYCAM_OK (0) on success or a negative SONYCAM_ERR_*.
//   * Calls are serialised internally, so they may come from any thread.
//   * Callbacks run on SDK/worker threads. Keep them short and do not call
//     back into this API from inside them.

#if defined(_WIN32) || defined(_WIN64)
#define SONYCAM_API __declspec(dllexport)
#else
#define SONYCAM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SONYCAM_API_VERSION 1

enum
{
    SONYCAM_OK = 0,
    SONYCAM_ERR_NOT_INIT = -1,      // sonycam_init() not called / failed
    SONYCAM_ERR_NO_CAMERA = -2,     // no camera found at that index
    SONYCAM_ERR_CONNECT = -3,       // connect failed or timed out
    SONYCAM_ERR_NOT_CONNECTED = -4, // call needs a connected camera
    SONYCAM_ERR_BUFFER = -5,        // caller's buffer too small
    SONYCAM_ERR_FAILED = -6,        // SDK call failed
    SONYCAM_ERR_ARG = -7            // bad argument
};

// Settings for sonycam_get_setting / sonycam_set_setting.
enum
{
    SONYCAM_SETTING_ISO = 0,
    SONYCAM_SETTING_SHUTTER_SPEED = 1,
    SONYCAM_SETTING_APERTURE = 2,
    SONYCAM_SETTING_EXPOSURE_MODE = 3,
    SONYCAM_SETTING_FOCUS_MODE = 4
};

typedef void (*sonycam_text_callback)(const char *utf8);

SONYCAM_API int sonycam_api_version(void);

// Receive every line the camera code logs (what RemoteCli printed to the
// console). Pass NULL to send logging back to stdout.
SONYCAM_API void sonycam_set_log_callback(sonycam_text_callback cb);

// Receive the full path of each image the camera transfers to the PC.
// Images only reach the PC when the store destination is PC or PC+card.
SONYCAM_API void sonycam_set_download_callback(sonycam_text_callback cb);

// Initialise / release the Sony SDK. Call once per process.
SONYCAM_API int sonycam_init(void);
SONYCAM_API void sonycam_shutdown(void);

// Enumerate cameras, connect to the given index (0 = first) in Remote mode
// and wait up to timeout_ms for the connection to come up.
SONYCAM_API int sonycam_connect(int camera_index, int timeout_ms);
SONYCAM_API void sonycam_disconnect(void);
SONYCAM_API int sonycam_is_connected(void);

// Model name of the connected camera. Returns the length needed (excluding
// the NUL); the copy is truncated if buf_len is too small.
SONYCAM_API int sonycam_get_model(char *buf, int buf_len);

// Folder the PC copies of images are written to (created if missing), the
// file name prefix (may be "") and starting number (-1 = camera numbering).
SONYCAM_API int sonycam_set_save_info(const char *dir, const char *prefix, int start_no);

// Still image store destination: 0 = PC, 1 = memory card, 2 = PC + card.
SONYCAM_API int sonycam_set_store_destination(int index);
// Returns 0..2, or a negative error if unknown.
SONYCAM_API int sonycam_get_store_destination(void);

// Press and release the shutter.
SONYCAM_API int sonycam_capture(void);

// Copy the latest live view frame (JPEG) into buf. Returns the number of
// bytes written, 0 if no new frame is ready yet, or a negative error
// (SONYCAM_ERR_BUFFER if buf_len is too small).
SONYCAM_API int sonycam_get_live_view(unsigned char *buf, int buf_len);

// Current value of a setting and its selectable values, '\n'-separated.
// *writable is set to 1 if the setting can be changed right now.
SONYCAM_API int sonycam_get_setting(int setting, char *current, int current_len,
                                    char *options, int options_len, int *writable);
// Select the option at 'index' in the list returned by sonycam_get_setting.
SONYCAM_API int sonycam_set_setting(int setting, int index);

#ifdef __cplusplus
}
#endif

#endif // SONYCAMAPI_H
