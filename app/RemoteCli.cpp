#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <regex>
#include <thread>
#include <chrono>
#include <atomic>
#include <opencv2/opencv.hpp>

#include "CRSDK/CameraRemote_SDK.h"
#include "CameraDevice.h"
#include "Text.h"
#include "ViewImage.h"

namespace SDK = SCRSDK;
typedef std::shared_ptr<cli::CameraDevice> CameraDevicePtr;

int main()
{
    std::locale::global(std::locale(""));
    cli::tin.imbue(std::locale());
    cli::tout.imbue(std::locale());

    if (!SDK::Init())
    {
        cli::tout << "SDK init failed.\n";
        return EXIT_FAILURE;
    }

    cli::tout << "Connecting to camera...\n";

    SDK::ICrEnumCameraObjectInfo *camera_list = nullptr;
    if (CR_FAILED(SDK::EnumCameraObjects(&camera_list)) || camera_list == nullptr)
    {
        cli::tout << "No cameras found.\n"
                  << "Check that: \n"
                  << "         1. The camera is connected and powered on.\n"
                  << "         2. The correct Network Configurations are applied.\n"
                  << "         3. The camera is not being used by another application.\n";
        SDK::Release();
        return EXIT_FAILURE;
    }

    CrInt32u count = camera_list->GetCount();
    for (CrInt32u i = 0; i < count; ++i)
    {
        auto info = camera_list->GetCameraObjectInfo(i);
        cli::tout << "Found camera:\n";
        cli::tout << "[" << i + 1 << "] " << info->GetModel() << "\n";
    }
    int camIndex = 0;

    auto cam_info = camera_list->GetCameraObjectInfo(camIndex);
    CameraDevicePtr camera = std::make_shared<cli::CameraDevice>(1, cam_info);
    camera_list->Release();

    camera->connect(SDK::CrSdkControlMode_Remote, SDK::CrReconnecting_ON);

    std::atomic<bool> exitFlag{false};
    std::atomic<bool> autoCaptureFlag{false};

    ViewImage viewer;
    viewer.runUI(camera, exitFlag, autoCaptureFlag);

    SDK::Release();
    return EXIT_SUCCESS;
}
