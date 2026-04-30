#pragma once

#include <alsa/asoundlib.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace spiderbot {

enum class AudioState {
    AUDIO_INIT,
    AUDIO_IDLE,
    LISTEN_MODE,
    TALK_MODE,
    AUDIO_ERROR,
};

const char* audioStateStr(AudioState s);

class AudioManager {
public:
    // INMP441 : 24 bits dans 32 bits, on travaille en S16_LE via plughw
    static constexpr unsigned int      kSampleRate   = 16000;
    static constexpr unsigned int      kChannels     = 1;
    static constexpr snd_pcm_uframes_t kPeriodFrames = 512; // ~32 ms à 16 kHz

    // Appelé pour chaque période capturée (LISTEN_MODE)
    using CaptureCallback = std::function<void(const std::vector<int16_t>&)>;

    // Les noms ALSA dépendent des dtoverlay dans /boot/config.txt :
    //   dtoverlay=i2s-mems-mic   -> carte sndi2smic
    //   dtoverlay=max98357a      -> carte MAX98357A
    explicit AudioManager(
        std::string capture_device  = "plughw:CARD=sndi2smic,DEV=0",
        std::string playback_device = "plughw:CARD=MAX98357A,DEV=0");
    ~AudioManager();

    AudioManager(const AudioManager&)            = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // --- Transitions FSM ---
    bool init();                                   // AUDIO_INIT  -> AUDIO_IDLE | AUDIO_ERROR
    bool startListen(CaptureCallback cb);          // AUDIO_IDLE  -> LISTEN_MODE
    bool stopListen();                             // LISTEN_MODE -> AUDIO_IDLE
    bool startTalk(std::vector<int16_t> samples);  // AUDIO_IDLE  -> TALK_MODE
    bool stopTalk();                               // TALK_MODE   -> AUDIO_IDLE
    void reset();                                  // AUDIO_ERROR -> AUDIO_INIT -> AUDIO_IDLE
    void shutdown();                               // tout état   -> [*]

    AudioState state() const { return state_.load(std::memory_order_acquire); }

private:
    bool openCapture();
    bool openPlayback();
    void closeCapture();
    void closePlayback();
    void captureLoop();
    void playbackLoop();
    void transitionTo(AudioState s);

    std::string             capture_device_;
    std::string             playback_device_;
    snd_pcm_t*              cap_pcm_{nullptr};
    snd_pcm_t*              pb_pcm_{nullptr};
    std::atomic<AudioState> state_{AudioState::AUDIO_INIT};
    std::atomic<bool>       stop_flag_{false};
    std::thread             capture_thread_;
    std::thread             playback_thread_;
    CaptureCallback         capture_cb_;
    std::vector<int16_t>    playback_buf_;
};

}  // namespace spiderbot
