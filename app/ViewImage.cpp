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

void drawControlCentreUI(cv::Mat &settingsWindow, std::atomic<bool> &autoCaptureFlag)
{
    cv::putText(settingsWindow, "Control Centre", {30, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 0, 0}, 2);

    // Dropdown current selection
    cv::rectangle(settingsWindow, cv::Rect(30, 70, 200, 30), cv::Scalar(200, 200, 200), -1);
    std::string currentStr(currentFocusMode.begin(), currentFocusMode.end());
    cv::putText(settingsWindow, currentStr, {35, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);

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

    // Buttons
    cv::rectangle(settingsWindow, cv::Rect(30, 270, 160, 40), cv::Scalar(180, 180, 180), -1);
    cv::putText(settingsWindow, "Focus", {50, 295}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);

    cv::rectangle(settingsWindow, cv::Rect(30, 320, 160, 40), cv::Scalar(180, 180, 180), -1);
    cv::putText(settingsWindow, "Capture", {50, 345}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);

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
                //camera->capture_image();
                camera->continuous_shooting();
                auto end = std::chrono::high_resolution_clock::now();
                //std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::chrono::duration<double, std::milli> duration_ms = end - start;
                std::cout << "Execution time: " << duration_ms.count() << " ms\n";
            }
            else
            {
                //std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    while (!exitFlag)
    {
        if (!autoCaptureFlag.load())
        {
            // Only run live view if auto capture is off
            camera->get_live_view();
            cv::Mat img = cv::imread("LiveView000000.jpg");
            if (!img.empty())
            {
                cv::resize(img, img, cv::Size(800, 600));
                cv::imshow("Live View", img);
            }
            currentFocusMode = camera->get_focus_mode_output();
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

    bool clickedDropdownBox = (x >= 30 && x <= 230 && y >= 70 && y <= 100);
    int clickedDropdownItem = -1;

    if (showDropdown)
    {
        for (size_t i = 0; i < focusModes.size(); ++i)
        {
            int itemTop = 100 + static_cast<int>(i) * 30;
            int itemBottom = itemTop + 30;
            if (x >= 30 && x <= 230 && y >= itemTop && y <= itemBottom)
            {
                clickedDropdownItem = static_cast<int>(i);
                break;
            }
        }
    }

    if (clickedDropdownBox)
    {
        showDropdown = !showDropdown;
    }
    else if (clickedDropdownItem != -1)
    {
        currentFocusMode = focusModes[clickedDropdownItem];
        camera->set_focus_mode_new(currentFocusMode, clickedDropdownItem);
        showDropdown = false;
        std::string s(currentFocusMode.begin(), currentFocusMode.end());
        std::cout << "[Focus Mode] Set to: " << s << "\n";
    }
    else if (x >= 30 && x <= 190 && y >= 270 && y <= 310)
    {
        std::cout << "[Focus] clicked\n";
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        camera->s1_shooting();
    }
    else if (x >= 30 && x <= 190 && y >= 320 && y <= 360)
    {
        std::cout << "[Capture] clicked\n";
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
