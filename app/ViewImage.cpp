#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>

#include "ViewImage.h"

static std::vector<std::wstring> focusModes = {L"AF_S", L"AF_A", L"AF_C", L"DMF", L"MF"};
static std::wstring currentFocusMode;
static bool showDropdown = false;

// Shutter speed dropdown additions
static std::vector<std::wstring> shutterSpeeds = {
    L"Bulb", L"30", L"25", L"20", L"15", L"13", L"10", L"8", L"6", L"5", L"4", L"3.2", L"2.5", L"2", L"1.6", L"1.3", L"1", L"0.8", L"0.6", L"0.5", L"0.4", L"1/3", L"1/4", L"1/5", L"1/6", L"1/8", L"1/10", L"1/13", L"1/15", L"1/20", L"1/25", L"1/30", L"1/40", L"1/50", L"1/60", L"1/80", L"1/100", L"1/125", L"1/160", L"1/200", L"1/250", L"1/320", L"1/400", L"1/500", L"1/640", L"1/800", L"1/1000", L"1/1250", L"1/1600", L"1/2000", L"1/2500", L"1/3200", L"1/4000"
};
static std::wstring currentShutterSpeed = shutterSpeeds[0];
static bool showShutterDropdown = false;

// Shutter speed dropdown additions
static std::vector<std::wstring> isoSpeeds = {
    
};
static std::wstring currentIsoSpeed = isoSpeeds[0];
static bool showIsoDropdown = false;

// Scrollable dropdown state
static int shutterDropdownOffset = 0;
static const int maxVisibleShutterItems = 6; // Number of visible items in dropdown

// Scrollable dropdown state
static int isoDropdownOffset = 0;
static const int maxVisibleIsoItems = 6; // Number of visible items in dropdown

void drawControlCentreUI(cv::Mat &settingsWindow, std::atomic<bool> &autoCaptureFlag)
{
    cv::putText(settingsWindow, "Control Centre", {30, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 0, 0}, 2);

    // Focus Mode Dropdown
    cv::rectangle(settingsWindow, cv::Rect(30, 70, 200, 30), cv::Scalar(200, 200, 200), -1);
    std::string currentFocusStr(currentFocusMode.begin(), currentFocusMode.end());
    cv::putText(settingsWindow, currentFocusStr, {35, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);

    if (showDropdown)
    {
        for (size_t i = 0; i < focusModes.size(); ++i)
        {
            int yOffset = 100 + static_cast<int>(i) * 30;
            cv::rectangle(settingsWindow, cv::Rect(30, yOffset, 200, 30), cv::Scalar(220, 220, 220), -1);
            std::string s(focusModes[i].begin(), focusModes[i].end());
            cv::putText(settingsWindow, s, {35, yOffset + 20}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);
        }
    }

    // Shutter Speed Dropdown
    cv::rectangle(settingsWindow, cv::Rect(250, 70, 200, 30), cv::Scalar(200, 200, 200), -1);
    std::string currentShutterStr(currentShutterSpeed.begin(), currentShutterSpeed.end());
    cv::putText(settingsWindow, currentShutterStr, {255, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);

    if (showShutterDropdown)
    {
        int totalItems = static_cast<int>(shutterSpeeds.size());
        int visibleItems = std::min(maxVisibleShutterItems, totalItems);
        int startIdx = shutterDropdownOffset;
        int endIdx = std::min(startIdx + visibleItems, totalItems);

        // Draw up arrow if needed
        if (startIdx > 0)
        {
            cv::rectangle(settingsWindow, cv::Rect(250, 100, 200, 30), cv::Scalar(210, 210, 210), -1);
            cv::putText(settingsWindow, "^", {345, 120}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 0, 0}, 2);
        }

        // Draw visible items
        for (int i = startIdx; i < endIdx; ++i)
        {
            int yOffset = 100 + 30 * (i - startIdx + (startIdx > 0 ? 1 : 0));
            cv::rectangle(settingsWindow, cv::Rect(250, yOffset, 200, 30), cv::Scalar(220, 220, 220), -1);
            std::string s(shutterSpeeds[i].begin(), shutterSpeeds[i].end());
            cv::putText(settingsWindow, s, {255, yOffset + 20}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);
        }

        // Draw down arrow if needed
        if (endIdx < totalItems)
        {
            int yOffset = 100 + 30 * (visibleItems + (startIdx > 0 ? 1 : 0));
            cv::rectangle(settingsWindow, cv::Rect(250, yOffset, 200, 30), cv::Scalar(210, 210, 210), -1);
            cv::putText(settingsWindow, "v", {345, yOffset + 20}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 0, 0}, 2);
        }

        // Draw scrollbar
        int dropdownHeight = visibleItems * 30;
        int scrollbarHeight = std::max(30, dropdownHeight * visibleItems / totalItems);
        int scrollbarY = 100 + ((startIdx * dropdownHeight) / totalItems) + (startIdx > 0 ? 30 : 0);
        cv::rectangle(settingsWindow, cv::Rect(445, scrollbarY, 5, scrollbarHeight), cv::Scalar(150, 150, 150), -1);
    }


    // Buttons
    cv::rectangle(settingsWindow, cv::Rect(30, 270, 160, 40), cv::Scalar(180, 180, 180), -1);
    cv::putText(settingsWindow, "Focus (AF)", {50, 295}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);

    cv::rectangle(settingsWindow, cv::Rect(30, 320, 160, 40), cv::Scalar(180, 180, 180), -1);
    cv::putText(settingsWindow, "Capture (AF)", {50, 345}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);

    cv::rectangle(settingsWindow, cv::Rect(30, 370, 160, 40),
                  autoCaptureFlag.load() ? cv::Scalar(100, 250, 100) : cv::Scalar(180, 180, 180), -1);
    cv::putText(settingsWindow, "Auto Capture", {35, 395}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);

    cv::rectangle(settingsWindow, cv::Rect(220, 370, 160, 40), cv::Scalar(50, 50, 50), -1);
    cv::putText(settingsWindow, "Exit", {250, 395}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 2);
}

void ViewImage::runUI(std::shared_ptr<cli::CameraDevice> camera,
                      std::atomic<bool> &exitFlag,
                      std::atomic<bool> &autoCaptureFlag)
{
    CallbackContext context{camera, &exitFlag, &autoCaptureFlag};

    std::thread autoCaptureThread([&]() {
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
            }
        }
    });

    while (!exitFlag)
    {
        if (!autoCaptureFlag.load())
        {
            camera->get_live_view();
            cv::Mat img = cv::imread("LiveView000000.jpg");
            if (!img.empty())
            {
                cv::resize(img, img, cv::Size(800, 600));
                cv::imshow("Live View", img);
            }
            currentFocusMode = camera->get_focus_mode_output();
            currentShutterSpeed = camera->get_shutter_speed_output();
        }

        cv::Mat settingsWindow(450, 450, CV_8UC3, cv::Scalar(240, 240, 240));
        drawControlCentreUI(settingsWindow, autoCaptureFlag);

        cv::namedWindow("Control Centre");
        cv::setMouseCallback("Control Centre", onMouse, &context);
        cv::imshow("Control Centre", settingsWindow);

        if (cv::waitKey(30) == 27) // ESC to exit
            exitFlag = true;
    }

    if (autoCaptureThread.joinable())
        autoCaptureThread.join();

    cv::destroyAllWindows();
}

void ViewImage::onMouse(int event, int x, int y, int, void *userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN)
        return;

    auto *context = static_cast<CallbackContext *>(userdata);
    auto camera = context->camera;
    auto exitFlag = context->exitFlag;
    auto autoCaptureFlag = context->autoCaptureFlag;

    // Focus Mode Dropdown
    bool clickedFocusDropdownBox = (x >= 30 && x <= 230 && y >= 70 && y <= 100);
    int clickedFocusDropdownItem = -1;
    if (showDropdown)
    {
        for (size_t i = 0; i < focusModes.size(); ++i)
        {
            int itemTop = 100 + static_cast<int>(i) * 30;
            int itemBottom = itemTop + 30;
            if (x >= 30 && x <= 230 && y >= itemTop && y <= itemBottom)
            {
                clickedFocusDropdownItem = static_cast<int>(i);
                break;
            }
        }
    }

    // Shutter Speed Dropdown (scrollable)
    bool clickedShutterDropdownBox = (x >= 250 && x <= 450 && y >= 70 && y <= 100);
    int clickedShutterDropdownItem = -1;
    int totalItems = static_cast<int>(shutterSpeeds.size());
    int visibleItems = std::min(maxVisibleShutterItems, totalItems);
    int startIdx = shutterDropdownOffset;
    int endIdx = std::min(startIdx + visibleItems, totalItems);

    bool clickedUpArrow = false, clickedDownArrow = false;
    int upArrowY = 100, downArrowY = 100 + 30 * (visibleItems + (startIdx > 0 ? 1 : 0));
    int dropdownX1 = 250, dropdownX2 = 450;

    if (showShutterDropdown)
    {
        int yBase = 100;
        int itemOffset = (startIdx > 0 ? 1 : 0);

        // Up arrow
        if (startIdx > 0 && x >= dropdownX1 && x <= dropdownX2 && y >= yBase && y <= yBase + 30)
            clickedUpArrow = true;

        // Down arrow
        if (endIdx < totalItems)
        {
            int arrowY = yBase + 30 * (visibleItems + itemOffset);
            if (x >= dropdownX1 && x <= dropdownX2 && y >= arrowY && y <= arrowY + 30)
                clickedDownArrow = true;
        }

        // Items
        for (int i = startIdx; i < endIdx; ++i)
        {
            int yOffset = yBase + 30 * (i - startIdx + itemOffset);
            if (x >= dropdownX1 && x <= dropdownX2 && y >= yOffset && y <= yOffset + 30)
            {
                clickedShutterDropdownItem = i;
                break;
            }
        }
    }

    if (clickedFocusDropdownBox)
    {
        showDropdown = !showDropdown;
        showShutterDropdown = false;
    }
    else if (clickedFocusDropdownItem != -1)
    {
        currentFocusMode = focusModes[clickedFocusDropdownItem];
        camera->set_focus_mode_new(currentFocusMode, clickedFocusDropdownItem);
        showDropdown = false;
        std::string s(currentFocusMode.begin(), currentFocusMode.end());
        std::cout << "[Focus Mode] Set to: " << s << "\n";
    }
    else if (clickedShutterDropdownBox)
    {
        showShutterDropdown = !showShutterDropdown;
        showDropdown = false;
    }
    else if (showShutterDropdown && clickedUpArrow)
    {
        if (shutterDropdownOffset > 0)
            shutterDropdownOffset--;
    }
    else if (showShutterDropdown && clickedDownArrow)
    {
        if (shutterDropdownOffset + visibleItems < totalItems)
            shutterDropdownOffset++;
    }
    else if (clickedShutterDropdownItem != -1)
    {
        currentShutterSpeed = shutterSpeeds[clickedShutterDropdownItem];
        camera->set_shutter_speed_new(currentShutterSpeed, clickedShutterDropdownItem);
        showShutterDropdown = false;
        std::string s(currentShutterSpeed.begin(), currentShutterSpeed.end());
        std::cout << "[Shutter Speed] Set to: " << s << "\n";
    }
    else if (x >= 30 && x <= 190 && y >= 270 && y <= 310)
    {
        std::cout << "[Focus] clicked\n";
        camera->s1_shooting();
    }
    else if (x >= 30 && x <= 190 && y >= 320 && y <= 360)
    {
        std::cout << "[Capture] clicked\n";
        camera->af_shutter();
    }
    else if (x >= 30 && x <= 190 && y >= 370 && y <= 410)
    {
        *autoCaptureFlag = !(*autoCaptureFlag);
        std::cout << "[Auto Capture] toggled to: " << (*autoCaptureFlag ? "ON" : "OFF") << "\n";
    }
    else if (x >= 220 && x <= 380 && y >= 370 && y <= 410)
    {
        std::cout << "[Exit] clicked\n";
        *exitFlag = true;
    }
}
