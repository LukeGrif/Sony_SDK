#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

#include "ViewImage.h"

void ViewImage::displayImage(const std::string& path, std::shared_ptr<cli::CameraDevice> camera, std::atomic<bool>& exitFlag) {
    CallbackContext context{camera, &exitFlag};

    cv::Mat img = cv::imread(path);
    if (img.empty()) {
        std::cerr << "Failed to open image: " << path << std::endl;
        return;
    }

    cv::resize(img, img, cv::Size(800, 600));

    cv::rectangle(img, cv::Rect(10, 10, 180, 40), cv::Scalar(200, 200, 200), -1);
    cv::rectangle(img, cv::Rect(10, 60, 180, 40), cv::Scalar(200, 200, 200), -1);
    cv::rectangle(img, cv::Rect(10, 110, 180, 40), cv::Scalar(200, 200, 200), -1);
    cv::rectangle(img, cv::Rect(10, 160, 180, 40), cv::Scalar(50, 50, 50), -1);

    cv::putText(img, "Focus",         {20, 35},  cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);
    cv::putText(img, "Focus + Photo", {20, 85},  cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);
    cv::putText(img, "Auto Capture",  {20, 135}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 2);
    cv::putText(img, "Exit",          {20, 185}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 2);

    cv::namedWindow("Camera Control");
    cv::setMouseCallback("Camera Control", onMouse, &context);
    cv::imshow("Camera Control", img);
    cv::waitKey(1);
}

void ViewImage::onMouse(int event, int x, int y, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) return;

    auto* context = static_cast<CallbackContext*>(userdata);
    auto camera = context->camera;
    auto exitFlag = context->exitFlag;

    if (x >= 10 && x <= 190) {
        if (y >= 10 && y <= 50) {
            std::cout << "[Focus] clicked\n";
            //camera->af_start();
        }
        else if (y >= 60 && y <= 100) {
            std::cout << "[Focus + Photo] clicked\n";
        }
        else if (y >= 110 && y <= 150) {
            std::cout << "[Auto Capture] clicked\n";
        }
        else if (y >= 160 && y <= 200) {
            std::cout << "[Exit] clicked\n";
            *exitFlag = true;
        }
    }
}
