#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <algorithm> // max/min

#include "ViewImage.h"

// ------------------- State -------------------
static std::vector<std::wstring> focusModes = {L"AF_S", L"AF_A", L"AF_C", L"DMF", L"MF"};
static std::wstring currentFocusMode;
static bool showDropdown = false;
static int liveViewWidth = 1064;
static int liveViewHeight = 680;

// Interval input state
static std::atomic<int> autoIntervalMs{3000}; // default 3000 ms
static bool editingInterval = false;          // whether the input box is focused
static std::string intervalBuf = "3000";      // editable text buffer shown in the box

// Current read-only values (wstring like your focus getter)
static std::wstring currentExposureMode;
static std::wstring currentShutterSpeed;
static std::wstring currentAperture;
static std::wstring currentISO;

// NEW: bottom (green) button values shown UNDER each button
static std::wstring currentFormat;
static std::wstring currentType;
static std::wstring currentQuality;
static std::wstring currentSize;
static std::wstring currentWBMode;
static std::wstring currentWBValue;
static std::wstring currentFocusArea;

// Utility: convert wstring -> narrow for OpenCV text
static std::string ws2s(const std::wstring& ws) {
    return std::string(ws.begin(), ws.end());
}

// ------------------- Layout -------------------
struct UIRects {
    // Header
    cv::Point titlePos;

    // Dropdown (Focus Mode)
    cv::Rect dropdownBox;
    std::vector<cv::Rect> dropdownItems; // one per focus mode

    // Left column (actions)
    cv::Rect btnFocus;
    cv::Rect btnCapture;
    cv::Rect btnAutoCapture;

    // Right column (setting buttons)
    cv::Rect btnExposureMode;
    cv::Rect btnShutterSpeed;
    cv::Rect btnAperture;
    cv::Rect btnISO;
    cv::Rect btnResetLiveView;

    // Read-only value boxes (to the right of the buttons above)
    cv::Rect valExposureMode;
    cv::Rect valShutterSpeed;
    cv::Rect valAperture;
    cv::Rect valISO;

    // Auto-capture interval input box
    cv::Rect intervalBox;

    // Bottom grid buttons (green)
    cv::Rect btnFormat;
    cv::Rect btnType;
    cv::Rect btnQuality;
    cv::Rect btnSize;
    cv::Rect btnWhiteBalance;
    cv::Rect btnWhiteFocusValue;
    cv::Rect btnFocusArea;

    // NEW: value boxes UNDER each green button
    cv::Rect valFormat;
    cv::Rect valType;
    cv::Rect valQuality;
    cv::Rect valSize;
    cv::Rect valWhiteBalance;
    cv::Rect valWhiteFocusValue;
    cv::Rect valFocusArea;

    // Exit
    cv::Rect btnExit;
};

// Control Centre size
static const int CC_WIDTH  = 1100;   // widened to fit value boxes
static const int CC_HEIGHT = 1100;

static UIRects makeLayout()
{
    UIRects r{};

    // Title
    r.titlePos = {30, 60};

    // Dropdown (Focus Mode current)
    const int ddX = 30, ddY = 90, ddW = 260, ddH = 46;
    r.dropdownBox = {ddX, ddY, ddW, ddH};

    // Dropdown items
    const int itemH = 46;
    const int itemGap = 6;
    int listTop = ddY + ddH + 8;
    r.dropdownItems.clear();
    for (size_t i = 0; i < focusModes.size(); ++i) {
        int y = listTop + static_cast<int>(i) * (itemH + itemGap);
        r.dropdownItems.push_back(cv::Rect(ddX, y, ddW, itemH));
    }

    // Main buttons
    const int btnW = 240, btnH = 60, gapY = 16;

    int leftX = 30;
    int y = r.dropdownItems.empty() ? (ddY + ddH + 20) : (r.dropdownItems.back().y + r.dropdownItems.back().height + 20);
    r.btnFocus       = {leftX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnCapture     = {leftX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnAutoCapture = {leftX, y, btnW, btnH};

    // Right column for setting buttons
    int rightX = leftX + btnW + 30;
    y = ddY + 60;
    r.btnExposureMode  = {rightX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnShutterSpeed  = {rightX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnAperture      = {rightX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnISO           = {rightX, y, btnW, btnH};
    y += btnH + gapY;
    r.btnResetLiveView = {rightX, y, btnW, btnH};

    // Value boxes to the right of the right-column buttons
    int valX = rightX + btnW + 20;
    int valW = 260;
    int vy = ddY + 60;
    r.valExposureMode = {valX, vy, valW, btnH};
    vy += btnH + gapY;
    r.valShutterSpeed = {valX, vy, valW, btnH};
    vy += btnH + gapY;
    r.valAperture     = {valX, vy, valW, btnH};
    vy += btnH + gapY;
    r.valISO          = {valX, vy, valW, btnH};

    // Interval input box (below Reset LiveView, aligned with button column)
    y += btnH + gapY;
    int extra = 15; // tiny visual nudge down
    r.intervalBox = {rightX, y + extra, btnW, btnH};

    // Bottom grid buttons
    int gridTop = std::max(
        r.btnAutoCapture.y + r.btnAutoCapture.height,
        std::max(r.intervalBox.y + r.intervalBox.height, r.valISO.y + r.valISO.height)
    ) + 30;

    int cellW = 220, cellH = 60, gapX = 20, gapY2 = 50;  // vertical space; values will sit under buttons
    int col1 = 30;
    int col2 = col1 + cellW + gapX;
    int col3 = col2 + cellW + gapX;

    // --- Row 1 buttons ---
    r.btnFormat          = {col1, gridTop,            cellW, cellH};
    r.btnType            = {col2, gridTop,            cellW, cellH};
    r.btnQuality         = {col3, gridTop,            cellW, cellH};

    // --- Row 1 values (under buttons) ---
    const int valH = 28;
    const int valGap = 6; // gap between button and its value box
    r.valFormat         = {col1, r.btnFormat.y + r.btnFormat.height + valGap,          cellW, valH};
    r.valType           = {col2, r.btnType.y + r.btnType.height + valGap,              cellW, valH};
    r.valQuality        = {col3, r.btnQuality.y + r.btnQuality.height + valGap,        cellW, valH};

    // --- Row 2 origin is below the tallest of row1 value boxes ---
    int row2Top = std::max({r.valFormat.y + r.valFormat.height,
                            r.valType.y + r.valType.height,
                            r.valQuality.y + r.valQuality.height}) + gapY2;

    // --- Row 2 buttons ---
    r.btnSize            = {col1, row2Top,            cellW, cellH};
    r.btnWhiteBalance    = {col2, row2Top,            cellW, cellH};
    r.btnWhiteFocusValue = {col3, row2Top,            cellW, cellH};

    // --- Row 2 values ---
    r.valSize            = {col1, r.btnSize.y + r.btnSize.height + valGap,             cellW, valH};
    r.valWhiteBalance    = {col2, r.btnWhiteBalance.y + r.btnWhiteBalance.height + valGap, cellW, valH};
    r.valWhiteFocusValue = {col3, r.btnWhiteFocusValue.y + r.btnWhiteFocusValue.height + valGap, cellW, valH};

    // --- Row 3 origin below row2 values ---
    int row3Top = std::max({r.valSize.y + r.valSize.height,
                            r.valWhiteBalance.y + r.valWhiteBalance.height,
                            r.valWhiteFocusValue.y + r.valWhiteFocusValue.height}) + gapY2;

    // --- Row 3 button ---
    r.btnFocusArea       = {col1, row3Top,            cellW, cellH};

    // --- Row 3 value ---
    r.valFocusArea       = {col1, r.btnFocusArea.y + r.btnFocusArea.height + valGap,   cellW, valH};

    // Exit button anchored at bottom-right
    r.btnExit = {CC_WIDTH - 200, CC_HEIGHT - 70, 170, 50};

    return r;
}

// ------------------- Drawing -------------------
static void drawButton(cv::Mat& img, const cv::Rect& rc, const std::string& label,
                       const cv::Scalar& fill, const cv::Scalar& textColor = {0,0,0})
{
    cv::rectangle(img, rc, fill, cv::FILLED, cv::LINE_AA);
    int baseline = 0;
    double fontScale = 0.9;
    int thickness = 2;
    auto size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Point textOrg(rc.x + (rc.width - size.width)/2, rc.y + (rc.height + size.height)/2 - 4);
    cv::putText(img, label, textOrg, cv::FONT_HERSHEY_SIMPLEX, fontScale, textColor, thickness, cv::LINE_AA);
}

static void drawInputBox(cv::Mat& img, const cv::Rect& rc, const std::string& text, bool focused)
{
    // Background
    cv::Scalar bg = {245, 245, 245};
    cv::rectangle(img, rc, bg, cv::FILLED, cv::LINE_AA);

    // Border (red if focused)
    cv::Scalar border = focused ? cv::Scalar(0, 0, 255) : cv::Scalar(150, 150, 150);
    cv::rectangle(img, rc, border, 2, cv::LINE_AA);

    // Text (left aligned)
    double fontScale = 0.9;
    int thickness = 2, baseline = 0;
    auto sz = cv::getTextSize(text.empty() ? "enter ms" : text,
                              cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Scalar tc = text.empty() ? cv::Scalar(120,120,120) : cv::Scalar(0,0,0);
    cv::Point org(rc.x + 10, rc.y + (rc.height + sz.height)/2 - 4);
    cv::putText(img, text.empty() ? "enter ms" : text, org,
                cv::FONT_HERSHEY_SIMPLEX, fontScale, tc, thickness, cv::LINE_AA);

    // Label above
    std::string label = "Auto Capture Interval (ms)";
    cv::putText(img, label, {rc.x, rc.y - 8}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {60,60,60}, 1, cv::LINE_AA);
}

// Read-only compact value box (grey, under a button)
static void drawUnderValueBox(cv::Mat& img, const cv::Rect& rc, const std::string& value)
{
    cv::rectangle(img, rc, {238,238,238}, cv::FILLED, cv::LINE_AA);
    cv::rectangle(img, rc, {160,160,160}, 1, cv::LINE_AA);

    // Value, left aligned
    int baseline = 0;
    double fontScale = 0.7; int thickness = 1;
    auto sz = cv::getTextSize(value, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Point org(rc.x + 8, rc.y + (rc.height + sz.height)/2 - 2);
    cv::putText(img, value, org, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0,0,0}, thickness, cv::LINE_AA);
}

// Read-only value box (to the right)
static void drawValueBoxRight(cv::Mat& img, const cv::Rect& rc, const std::string& label, const std::string& value)
{
    // Label above
    //cv::putText(img, label, {rc.x, rc.y - 8}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {60,60,60}, 1, cv::LINE_AA);

    // Box
    cv::rectangle(img, rc, {235,235,235}, cv::FILLED, cv::LINE_AA);
    cv::rectangle(img, rc, {140,140,140}, 2, cv::LINE_AA);

    // Value (left aligned)
    int baseline = 0;
    double fontScale = 0.9; int thickness = 2;
    auto sz = cv::getTextSize(value, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Point org(rc.x + 10, rc.y + (rc.height + sz.height)/2 - 4);
    cv::putText(img, value, org, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0,0,0}, thickness, cv::LINE_AA);
}

static void drawControlCentreUI(cv::Mat &settingsWindow, std::atomic<bool> &autoCaptureFlag, const UIRects& r)
{
    // Title
    cv::putText(settingsWindow, "Control Centre", r.titlePos, cv::FONT_HERSHEY_SIMPLEX, 1.5, {0,0,0}, 4, cv::LINE_AA);

    // Focus Mode current selection
    cv::rectangle(settingsWindow, r.dropdownBox, {200,200,200}, cv::FILLED, cv::LINE_AA);
    std::string currentStr(currentFocusMode.begin(), currentFocusMode.end());
    int baseline = 0;
    auto sz = cv::getTextSize(currentStr, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
    cv::putText(settingsWindow, currentStr,
                {r.dropdownBox.x + 10, r.dropdownBox.y + (r.dropdownBox.height + sz.height)/2},
                cv::FONT_HERSHEY_SIMPLEX, 1.0, {0,0,0}, 2, cv::LINE_AA);

    // Dropdown items
    if (showDropdown)
    {
        for (size_t i = 0; i < r.dropdownItems.size(); ++i)
        {
            const auto& itemRc = r.dropdownItems[i];
            cv::rectangle(settingsWindow, itemRc, {220,220,220}, cv::FILLED, cv::LINE_AA);
            std::string s(focusModes[i].begin(), focusModes[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height)/2},
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, {0,0,0}, 2, cv::LINE_AA);
        }
    }

    // Left column
    drawButton(settingsWindow, r.btnFocus,       "Focus",        {180,180,180});
    drawButton(settingsWindow, r.btnCapture,     "Capture",      {180,180,180});
    drawButton(settingsWindow, r.btnAutoCapture, "Auto Capture",
               autoCaptureFlag.load() ? cv::Scalar(100,250,100) : cv::Scalar(180,180,180));

    // Right column buttons
    drawButton(settingsWindow, r.btnExposureMode,  "Exposure Mode", {180,180,180});
    drawButton(settingsWindow, r.btnShutterSpeed,  "Shutter Speed", {180,180,180});
    drawButton(settingsWindow, r.btnAperture,      "Aperture",      {180,180,180});
    drawButton(settingsWindow, r.btnISO,           "ISO",           {180,180,180});
    drawButton(settingsWindow, r.btnResetLiveView, "Reset LiveView",{180,180,255}, {0,0,80});

    // Read-only value boxes (to the right)
    drawValueBoxRight(settingsWindow, r.valExposureMode, "Exposure Mode", ws2s(currentExposureMode));
    drawValueBoxRight(settingsWindow, r.valShutterSpeed, "Shutter Speed", ws2s(currentShutterSpeed));
    drawValueBoxRight(settingsWindow, r.valAperture,     "Aperture",      ws2s(currentAperture));
    drawValueBoxRight(settingsWindow, r.valISO,          "ISO",           ws2s(currentISO));

    // Interval input box
    drawInputBox(settingsWindow, r.intervalBox, intervalBuf, editingInterval);

    // Bottom grid buttons (green)
    drawButton(settingsWindow, r.btnFormat,          "Format",      {200,220,200});
    drawButton(settingsWindow, r.btnType,            "Type",        {200,220,200});
    drawButton(settingsWindow, r.btnQuality,         "Quality",     {200,220,200});
    drawButton(settingsWindow, r.btnSize,            "Size",        {200,220,200});
    drawButton(settingsWindow, r.btnWhiteBalance,    "W/B Mode",    {200,220,200});
    drawButton(settingsWindow, r.btnWhiteFocusValue, "W/B Value",   {200,220,200});
    drawButton(settingsWindow, r.btnFocusArea,       "Focus Area",  {200,220,200});

    // NEW: values UNDER each green button
    drawUnderValueBox(settingsWindow, r.valFormat,          ws2s(currentFormat));
    drawUnderValueBox(settingsWindow, r.valType,            ws2s(currentType));
    drawUnderValueBox(settingsWindow, r.valQuality,         ws2s(currentQuality));
    drawUnderValueBox(settingsWindow, r.valSize,            ws2s(currentSize));
    drawUnderValueBox(settingsWindow, r.valWhiteBalance,    ws2s(currentWBMode));
    drawUnderValueBox(settingsWindow, r.valWhiteFocusValue, ws2s(currentWBValue));
    drawUnderValueBox(settingsWindow, r.valFocusArea,       ws2s(currentFocusArea));

    // Exit
    drawButton(settingsWindow, r.btnExit, "Exit", {50,50,50}, {255,255,255});
}

// ------------------- ViewImage methods -------------------

void ViewImage::runUI(std::shared_ptr<cli::CameraDevice> camera,
                      std::atomic<bool> &exitFlag,
                      std::atomic<bool> &autoCaptureFlag)
{
    CallbackContext context{camera, &exitFlag, &autoCaptureFlag};

    std::thread autoCaptureThread([&](){
        while (!exitFlag)
        {
            if (autoCaptureFlag.load())
            {
                std::cout << "[Auto Capture] Taking photo...\n";
                auto start = std::chrono::high_resolution_clock::now();
                camera->capture_image();
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> duration_ms = end - start;
                std::cout << "Execution time: " << duration_ms.count() << " ms\n";

                // Use user-set interval (clamp to >=0)
                int ms = std::max(0, autoIntervalMs.load());
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    cv::namedWindow("Live View", cv::WINDOW_NORMAL);

    // Control Centre window
    cv::namedWindow("Control Centre", cv::WINDOW_NORMAL);
    cv::resizeWindow("Control Centre", CC_WIDTH, CC_HEIGHT);

    while (!exitFlag)
    {
        if (!autoCaptureFlag.load())
        {
            camera->get_live_view();
            cv::Mat img = cv::imread("LiveView000000.jpg");
            if (!img.empty())
            {
                cv::resize(img, img, cv::Size(1064, 680));
                cv::imshow("Live View", img);
            }

            // Refresh current values from camera (wstring outputs)
            currentFocusMode     = camera->get_focus_mode_output();

            // Top-right values
            currentExposureMode  = camera->get_exposure_program_mode_output(); // implement in CameraDevice
            currentShutterSpeed  = camera->get_shutter_speed_output();
            currentAperture      = camera->get_aperture_output();
            currentISO           = camera->get_iso_output();

            // Bottom (green) values UNDER each button
            //currentFormat        = camera->get_format_output();
            //currentType          = camera->get_type_output();
            //currentQuality       = camera->get_quality_output();
            //currentSize          = camera->get_size_output();
            currentWBMode        = camera->get_white_balance_output();
            //currentWBValue       = camera->get_white_balance_value_output();
            currentFocusArea     = camera->get_focus_area_output();
        }

        cv::Mat settingsWindow(CC_HEIGHT, CC_WIDTH, CV_8UC3, cv::Scalar(240,240,240));
        auto layout = makeLayout();
        drawControlCentreUI(settingsWindow, autoCaptureFlag, layout);

        cv::setMouseCallback("Control Centre", onMouse, &context);
        cv::imshow("Control Centre", settingsWindow);

        cv::Rect winRect = cv::getWindowImageRect("Live View");
        if (winRect.width > 0 && winRect.height > 0)
        {
            liveViewWidth  = winRect.width;
            liveViewHeight = winRect.height;
        }

        // Keyboard handling (interval input + exit)
        int key = cv::waitKey(30);
        if (editingInterval) {
            int k = key & 0xFF;
            if (k >= '0' && k <= '9') {
                if (intervalBuf.size() < 7) intervalBuf.push_back(static_cast<char>(k));
            } else if (k == 8 || k == 127) { // Backspace/Delete
                if (!intervalBuf.empty()) intervalBuf.pop_back();
            } else if (k == 13 || k == 10) { // Enter
                int v = 0;
                try { v = intervalBuf.empty() ? 0 : std::stoi(intervalBuf); } catch (...) { v = 0; }
                v = std::max(0, std::min(v, 600000)); // 0 .. 10 minutes
                autoIntervalMs.store(v);
                editingInterval = false;
                std::cout << "[Auto Capture] Interval set to " << v << " ms\n";
            } else if (k == 27) { // ESC cancels editing
                editingInterval = false;
            }
        } else {
            if ((key & 0xFF) == 27) { // ESC exits when not editing
                exitFlag = true;
            }
        }
    }

    if (autoCaptureThread.joinable())
        autoCaptureThread.join();

    cv::destroyAllWindows();
}

static bool pointIn(const cv::Point& p, const cv::Rect& r) { return r.contains(p); }

void ViewImage::onMouse(int event, int x, int y, int, void *userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN)
        return;

    auto *context = static_cast<CallbackContext *>(userdata);
    auto camera = context->camera;
    auto exitFlag = context->exitFlag;
    auto autoCaptureFlag = context->autoCaptureFlag;

    const auto r = makeLayout(); // same geometry as draw
    const cv::Point pt{x,y};

    // Focus interval input if clicked
    if (pointIn(pt, r.intervalBox)) {
        editingInterval = true;
        return;
    } else {
        editingInterval = false;
    }

    // Dropdown toggle
    if (pointIn(pt, r.dropdownBox)) {
        showDropdown = !showDropdown;
        return;
    }

    // Dropdown item clicks
    if (showDropdown) {
        for (size_t i = 0; i < r.dropdownItems.size(); ++i) {
            if (pointIn(pt, r.dropdownItems[i])) {
                currentFocusMode = focusModes[i];
                camera->set_focus_mode_new(currentFocusMode, static_cast<int>(i));
                showDropdown = false;
                std::string s(currentFocusMode.begin(), currentFocusMode.end());
                std::cout << "[Focus Mode] Set to: " << s << "\n";
                return;
            }
        }
    }

    // Left column
    if (pointIn(pt, r.btnFocus)) {
        std::cout << "[Focus] clicked\n";
        std::cout << "\nSent START\n";
        camera->s1_shooting();
        std::cout << "\nSent STOP\n";
        return;
    }
    if (pointIn(pt, r.btnCapture)) {
        std::cout << "[Capture] clicked\n";
        std::cout << "\nSent START\n";
        camera->capture_image();
        std::cout << "\nSent STOP\n";
        return;
    }
    if (pointIn(pt, r.btnAutoCapture)) {
        *autoCaptureFlag = !(*autoCaptureFlag);
        std::cout << "[Auto Capture] toggled to: " << (*autoCaptureFlag ? "ON" : "OFF") << "\n";
        return;
    }

    // Right column buttons
    if (pointIn(pt, r.btnExposureMode)) {
        std::cout << "[Exposure Mode] clicked\n";
        camera->get_exposure_program_mode();
        camera->set_exposure_program_mode();
        return;
    }
    if (pointIn(pt, r.btnShutterSpeed)) {
        std::cout << "[Shutter Speed] clicked\n";
        camera->get_shutter_speed();
        camera->set_shutter_speed();
        return;
    }
    if (pointIn(pt, r.btnAperture)) {
        std::cout << "[Aperture] clicked\n";
        camera->get_aperture();
        camera->set_aperture();
        return;
    }
    if (pointIn(pt, r.btnISO)) {
        std::cout << "[ISO] clicked\n";
        camera->get_iso();
        camera->set_iso();
        return;
    }
    if (pointIn(pt, r.btnResetLiveView)) {
        std::cout << "[Reset LiveView] clicked\n";
        liveViewWidth = 1064;
        liveViewHeight = 680;
        cv::resizeWindow("Live View", liveViewWidth, liveViewHeight);
        return;
    }

    // Bottom grid (buttons only; values are display-only)
    if (pointIn(pt, r.btnFormat)) {
        std::cout << "[Format] clicked\n";
        // camera->set_format();
        return;
    }
    if (pointIn(pt, r.btnType)) {
        std::cout << "[Type] clicked\n";
        // camera->set_type();
        return;
    }
    if (pointIn(pt, r.btnQuality)) {
        std::cout << "[Quality] clicked\n";
        // camera->set_quality();
        return;
    }
    if (pointIn(pt, r.btnSize)) {
        std::cout << "[Size] clicked\n";
        // camera->set_size();
        return;
    }
    if (pointIn(pt, r.btnWhiteBalance)) {
        std::cout << "[W/B Mode] clicked\n";
        camera->set_white_balance();
        return;
    }
    if (pointIn(pt, r.btnWhiteFocusValue)) {
        std::cout << "[W/B Value] clicked\n";
        // camera->set_white_focus_value();
        return;
    }
    if (pointIn(pt, r.btnFocusArea)) {
        std::cout << "[Focus Area] clicked\n";
        // camera->set_focus_area();
        return;
    }

    // Exit
    if (pointIn(pt, r.btnExit)) {
        std::cout << "[Exit] clicked\n";
        *exitFlag = true;
        return;
    }
}
