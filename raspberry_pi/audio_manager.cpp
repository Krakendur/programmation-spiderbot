#include "audio_manager.hpp"

#include <iostream>

namespace spiderbot {

AudioManager::AudioManager() : recording_(false), playback_(false) {}

void AudioManager::startRecording() {
    recording_ = true;
    std::cout << "Micro: enregistrement demarre" << std::endl;
}

void AudioManager::stopRecording() {
    recording_ = false;
    std::cout << "Micro: enregistrement arrete" << std::endl;
}

std::vector<unsigned char> AudioManager::getAudioFrame() const {
    if (!recording_) {
        return {};
    }

    // Stub. A remplacer par l'acquisition ALSA/PortAudio.
    return {};
}

void AudioManager::playSound(const std::vector<unsigned char>& audioData) const {
    (void)audioData;
    std::cout << "Audio: lecture chunk" << std::endl;
}

void AudioManager::playFile(const std::string& filePath) const {
    std::cout << "Audio: lecture fichier " << filePath << std::endl;
}

}  // namespace spiderbot
