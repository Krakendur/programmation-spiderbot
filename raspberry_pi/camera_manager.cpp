#include "camera_manager.hpp"

#include <iostream>

namespace spiderbot {

CameraManager::CameraManager() : running_(false), width_(640), height_(480) {}

void CameraManager::startCapture() {
    running_ = true;
    std::cout << "Camera: capture demarree" << std::endl;
}

void CameraManager::stopCapture() {
    running_ = false;
    std::cout << "Camera: capture arretee" << std::endl;
}

std::vector<unsigned char> CameraManager::getFrame() const {
    if (!running_) {
        return {};
    }

    // Stub. A remplacer par la capture camera reelle (libcamera/picamera2 C++).
    return {};
}

void CameraManager::setResolution(int width, int height) {
    width_ = width;
    height_ = height;
}

}  // namespace spiderbot
