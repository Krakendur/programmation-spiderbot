#include "audio_manager.hpp"
#include "camera_manager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    spiderbot::CameraManager camera;
    spiderbot::AudioManager audio;

    std::cout << "Spider-Bot Raspberry Pi C++ - Demarrage" << std::endl;

    camera.startCapture();
    audio.startRecording();

    while (true) {
        const auto frame = camera.getFrame();
        const auto audioChunk = audio.getAudioFrame();

        // TODO: transmission vers ESP32 via UART si necessaire.
        (void)frame;
        (void)audioChunk;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
