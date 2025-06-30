// #include "CRSDK/CameraRemote_SDK.h"
// #include <opencv2/opencv.hpp>
// #include <iostream>

// using namespace SCRSDK;

// int main() {
//     // 1. Initialize SDK
//     if (!Init()) {
//         std::cerr << "Failed to initialize Camera Remote SDK." << std::endl;
//         return -1;
//     }

//     // 2. Enumerate cameras
//     ICrEnumCameraObjectInfo* enumInfo = nullptr;
//     if (EnumCameraObjects(&enumInfo) != CrError_Success || !enumInfo) {
//         std::cerr << "No camera found." << std::endl;
//         Release();
//         return -1;
//     }

//     // 3. Get first camera info
//     ICrCameraObjectInfo* cameraInfo = enumInfo->GetCameraObjectInfo(0);
//     if (!cameraInfo) {
//         std::cerr << "Failed to get camera info." << std::endl;
//         Release();
//         return -1;
//     }

//     // 4. Connect to camera
//     CrDeviceHandle deviceHandle = 0;
//     IDeviceCallback* callback = nullptr; // Use your callback if needed
//     if (Connect(cameraInfo, callback, &deviceHandle) != CrError_Success) {
//         std::cerr << "Failed to connect to camera." << std::endl;
//         Release();
//         return -1;
//     }

//     // 5. Live view loop
//     cv::namedWindow("Live View", cv::WINDOW_AUTOSIZE);
//     while (true) {
//         CrImageDataBlock imageData = {};
//         if (GetLiveViewImage(deviceHandle, &imageData) == CrError_Success && imageData.pData && imageData.dataSize > 0) {
//             // Assume JPEG data, decode with OpenCV
//             std::vector<uchar> buf((uchar*)imageData.pData, (uchar*)imageData.pData + imageData.dataSize);
//             cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
//             if (!img.empty()) {
//                 cv::imshow("Live View", img);
//             }
//         }
//         if (cv::waitKey(30) == 27) break; // Exit on ESC
//     }

//     // 6. Cleanup
//     Disconnect(deviceHandle);
//     Release();
//     cv::destroyAllWindows();
//     return 0;
// }