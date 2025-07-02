#include "CRSDK/CameraRemote_SDK.h"
#include "CRSDK/ICrCameraObjectInfo.h"
#include "CRSDK/IDeviceCallback.h"
#include "CRSDK/CrImageDataBlock.h"
#include "CameraDevice.h"
#include <iostream>

#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <iostream>

using namespace SCRSDK;
using cli::CameraDevice;

int main()
{
    // Initialize SDK
    if (!Init())
    {
        std::cerr << "Failed to initialize Camera Remote SDK." << std::endl;
        return 1;
    }

    // Enumerate cameras
    ICrEnumCameraObjectInfo *cameraList = nullptr;
    CrError err = EnumCameraObjects(&cameraList);
    if (err != 0 || !cameraList)
    {
        std::cerr << "No camera found or enumeration failed." << std::endl;
        Release();
        return 1;
    }

    int numCameras = cameraList->GetCount();
    if (numCameras == 0)
    {
        std::cerr << "No cameras detected." << std::endl;
        Release();
        return 1;
    }

    std::cout << "Detected " << numCameras << " camera(s):" << std::endl;
    for (int i = 0; i < numCameras; ++i)
    {
        const ICrCameraObjectInfo *camInfo = cameraList->GetCameraObjectInfo(i);
        if (camInfo)
        {
            std::wcout << L"[" << i << L"] " << camInfo->GetModel() << L" (ID: " << camInfo->GetId() << L")" << std::endl;
        }
    }

    // Select camera
    int camIndex = 0;
    if (numCameras > 0)
    {
        std::cout << "Enter camera index to connect: ";
        std::cin >> camIndex;
        if (camIndex < 0 || camIndex >= numCameras)
        {
            std::cerr << "Invalid camera index." << std::endl;
            Release();
            return 1;
        }
    }

    // Connect to selected camera (RemoteCli passes a default IDeviceCallback, not a custom subclass)
    const ICrCameraObjectInfo *camInfo = cameraList->GetCameraObjectInfo(camIndex);
    CrDeviceHandle deviceHandle = 0;
    IDeviceCallback callback; // Default, no overrides

    CrError connectErr = Connect(const_cast<ICrCameraObjectInfo *>(camInfo), &callback, &deviceHandle);
    if (connectErr != 0)
    {
        std::cerr << "Failed to connect to camera. Error code: " << connectErr << std::endl;
        Release();
        return 1;
    }
    std::cout << "Connected to camera " << camIndex << "." << std::endl;

    // CameraDevice* camera = new CameraDevice(camIndex, camInfo);
    // camera->get_live_view();

    // Always disconnect and release resources
    Disconnect(deviceHandle);
    Release();
    return 0;
}
