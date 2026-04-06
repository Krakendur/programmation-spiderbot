#pragma once

#include <string>
#include <vector>

namespace spiderbot {

class AudioManager {
public:
    AudioManager();

    void startRecording();
    void stopRecording();
    std::vector<unsigned char> getAudioFrame() const;
    void playSound(const std::vector<unsigned char>& audioData) const;
    void playFile(const std::string& filePath) const;

private:
    bool recording_;
    bool playback_;
};

}  // namespace spiderbot
