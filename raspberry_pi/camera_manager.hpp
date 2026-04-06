#pragma once

#include <string>
#include <vector>

namespace spiderbot {

class CameraManager {
public:
    CameraManager();

    void startCapture();
    void stopCapture();
    std::vector<unsigned char> getFrame() const;
    void setResolution(int width, int height);

private:
    bool running_;
    int width_;
    int height_;
};

}  // namespace spiderbot
