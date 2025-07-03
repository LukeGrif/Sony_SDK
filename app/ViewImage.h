#ifndef VIEWIMAGE_H
#define VIEWIMAGE_H

#include <string>
#include <memory>
#include <atomic>

#include "CameraDevice.h"

class ViewImage
{
public:
    void displayImage(const std::string &path,
                      std::shared_ptr<cli::CameraDevice> camera,
                      std::atomic<bool> &exitFlag,
                      std::atomic<bool> &autoCaptureFlag);

    static void onMouse(int event, int x, int y, int flags, void *userdata);

private:
    struct CallbackContext
    {
        std::shared_ptr<cli::CameraDevice> camera;
        std::atomic<bool> *exitFlag;
        std::atomic<bool> *autoCaptureFlag;
    };
};

#endif // VIEWIMAGE_H
