#pragma once

#include "audio_manager.hpp"
#include "camera_manager.hpp"

#include <atomic>
#include <string>

namespace spiderbot {

enum class AvState {
    INIT_AV,
    IDLE_AV,
    VIDEO_MODE,
    AV_MODE,
    SAFE_STOP_AV,
    ERROR_AV,
};

const char* avStateStr(AvState s);

// Orchestre la FSM globale audiovisuelle.
// Crée et possède les deux sous-gestionnaires.
class AvFsm {
public:
    explicit AvFsm(
        std::string audio_cap  = "plughw:CARD=sndi2smic,DEV=0",
        std::string audio_pb   = "plughw:CARD=MAX98357A,DEV=0");
    ~AvFsm();

    AvFsm(const AvFsm&)            = delete;
    AvFsm& operator=(const AvFsm&) = delete;

    // --- Transitions FSM globale ---
    bool init();             // INIT_AV      -> IDLE_AV | ERROR_AV
    bool startVideo();       // IDLE_AV      -> VIDEO_MODE
    bool startAV();          // IDLE_AV | VIDEO_MODE -> AV_MODE
    bool stopAudio();        // AV_MODE      -> VIDEO_MODE
    bool stopAll();          // VIDEO_MODE | AV_MODE -> IDLE_AV
    bool safeStop();         // tout mode actif -> SAFE_STOP_AV -> IDLE_AV
    void reset();            // ERROR_AV     -> INIT_AV -> IDLE_AV
    void shutdown();         // tout état    -> [*]

    AvState    state()   const { return state_.load(std::memory_order_acquire); }
    AudioManager& audio()      { return audio_; }
    CameraManager& camera()    { return camera_; }

private:
    void transitionTo(AvState s);

    AudioManager            audio_;
    CameraManager           camera_;
    std::atomic<AvState>    state_{AvState::INIT_AV};
};

}  // namespace spiderbot
