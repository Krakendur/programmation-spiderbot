#include "av_fsm.hpp"

#include <iostream>

namespace spiderbot {

const char* avStateStr(AvState s) {
    switch (s) {
        case AvState::INIT_AV:      return "INIT_AV";
        case AvState::IDLE_AV:      return "IDLE_AV";
        case AvState::VIDEO_MODE:   return "VIDEO_MODE";
        case AvState::AV_MODE:      return "AV_MODE";
        case AvState::SAFE_STOP_AV: return "SAFE_STOP_AV";
        case AvState::ERROR_AV:     return "ERROR_AV";
    }
    return "UNKNOWN";
}

AvFsm::AvFsm(std::string audio_cap, std::string audio_pb)
    : audio_(std::move(audio_cap), std::move(audio_pb)) {}

AvFsm::~AvFsm() { shutdown(); }

void AvFsm::transitionTo(AvState s) {
    state_.store(s, std::memory_order_release);
    std::cout << "[AV] -> " << avStateStr(s) << '\n';
}

bool AvFsm::init() {
    const bool audio_ok  = audio_.init();
    const bool camera_ok = camera_.init();

    if (!audio_ok || !camera_ok) {
        transitionTo(AvState::ERROR_AV);
        return false;
    }
    transitionTo(AvState::IDLE_AV);
    return true;
}

bool AvFsm::startVideo() {
    if (state_.load() != AvState::IDLE_AV) return false;

    if (!camera_.startStream([](const Frame& f) {
            // Point d'extension : transmettre la frame (UART, réseau…)
            (void)f;
        })) {
        transitionTo(AvState::ERROR_AV);
        return false;
    }
    transitionTo(AvState::VIDEO_MODE);
    return true;
}

bool AvFsm::startAV() {
    const AvState s = state_.load();
    if (s != AvState::IDLE_AV && s != AvState::VIDEO_MODE) return false;

    // Si on vient de IDLE_AV, démarrer aussi la vidéo
    if (s == AvState::IDLE_AV) {
        if (!camera_.startStream([](const Frame& f) { (void)f; })) {
            transitionTo(AvState::ERROR_AV);
            return false;
        }
    }

    if (!audio_.startListen([](const std::vector<int16_t>& samples) {
            // Point d'extension : transmettre les samples (UART, réseau…)
            (void)samples;
        })) {
        camera_.stopStream();
        transitionTo(AvState::ERROR_AV);
        return false;
    }
    transitionTo(AvState::AV_MODE);
    return true;
}

bool AvFsm::stopAudio() {
    if (state_.load() != AvState::AV_MODE) return false;
    audio_.stopListen();
    transitionTo(AvState::VIDEO_MODE);
    return true;
}

bool AvFsm::stopAll() {
    const AvState s = state_.load();
    if (s != AvState::VIDEO_MODE && s != AvState::AV_MODE) return false;

    if (s == AvState::AV_MODE) audio_.stopListen();
    camera_.stopStream();
    transitionTo(AvState::IDLE_AV);
    return true;
}

bool AvFsm::safeStop() {
    const AvState s = state_.load();
    if (s == AvState::IDLE_AV || s == AvState::INIT_AV ||
        s == AvState::SAFE_STOP_AV) return false;

    transitionTo(AvState::SAFE_STOP_AV);

    bool ok = true;
    if (audio_.state() == AudioState::LISTEN_MODE) ok &= audio_.stopListen();
    if (audio_.state() == AudioState::TALK_MODE)   ok &= audio_.stopTalk();
    if (camera_.state() == VideoState::STREAMING ||
        camera_.state() == VideoState::STREAM_PAUSED) ok &= camera_.stopStream();

    if (ok) {
        transitionTo(AvState::IDLE_AV);
    } else {
        transitionTo(AvState::ERROR_AV);
    }
    return ok;
}

void AvFsm::reset() {
    if (state_.load() != AvState::ERROR_AV) return;
    audio_.reset();
    camera_.reset();
    transitionTo(AvState::INIT_AV);
    // init() re-valide les deux sous-systèmes
    if (!audio_.init() || !camera_.init()) {
        transitionTo(AvState::ERROR_AV);
        return;
    }
    transitionTo(AvState::IDLE_AV);
}

void AvFsm::shutdown() {
    safeStop();
    audio_.shutdown();
    camera_.shutdown();
    std::cout << "[AV] Systeme audiovisuel arrete\n";
}

}  // namespace spiderbot
