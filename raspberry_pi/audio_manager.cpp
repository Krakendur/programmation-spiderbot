#include "audio_manager.hpp"

#include <algorithm>
#include <iostream>

namespace spiderbot {

// ---------- utilitaires ----------

const char* audioStateStr(AudioState s) {
    switch (s) {
        case AudioState::AUDIO_INIT:  return "AUDIO_INIT";
        case AudioState::AUDIO_IDLE:  return "AUDIO_IDLE";
        case AudioState::LISTEN_MODE: return "LISTEN_MODE";
        case AudioState::TALK_MODE:   return "TALK_MODE";
        case AudioState::AUDIO_ERROR: return "AUDIO_ERROR";
    }
    return "UNKNOWN";
}

static bool alsa_configure(snd_pcm_t* pcm, unsigned int rate,
                            unsigned int channels, snd_pcm_uframes_t period) {
    snd_pcm_hw_params_t* hw;
    snd_pcm_hw_params_alloca(&hw);

    if (snd_pcm_hw_params_any(pcm, hw) < 0)                                        return false;
    if (snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)  return false;
    if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE) < 0)          return false;
    if (snd_pcm_hw_params_set_rate(pcm, hw, rate, 0) < 0)                          return false;
    if (snd_pcm_hw_params_set_channels(pcm, hw, channels) < 0)                     return false;
    if (snd_pcm_hw_params_set_period_size(pcm, hw, period, 0) < 0)                 return false;
    snd_pcm_uframes_t buf_size = period * 4;
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buf_size);
    if (snd_pcm_hw_params(pcm, hw) < 0)                                            return false;
    return true;
}

// ---------- ctor / dtor ----------

AudioManager::AudioManager(std::string cap, std::string pb)
    : capture_device_(std::move(cap)), playback_device_(std::move(pb)) {}

AudioManager::~AudioManager() { shutdown(); }

// ---------- privé ----------

void AudioManager::transitionTo(AudioState s) {
    state_.store(s, std::memory_order_release);
    std::cout << "[Audio] -> " << audioStateStr(s) << '\n';
}

bool AudioManager::openCapture() {
    int err = snd_pcm_open(&cap_pcm_, capture_device_.c_str(),
                           SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "[Audio] Micro ouverture echouee: " << snd_strerror(err) << '\n';
        return false;
    }
    if (!alsa_configure(cap_pcm_, kSampleRate, kChannels, kPeriodFrames)) {
        std::cerr << "[Audio] Micro config ALSA echouee\n";
        snd_pcm_close(cap_pcm_);
        cap_pcm_ = nullptr;
        return false;
    }
    return true;
}

bool AudioManager::openPlayback() {
    int err = snd_pcm_open(&pb_pcm_, playback_device_.c_str(),
                           SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "[Audio] Ampli ouverture echouee: " << snd_strerror(err) << '\n';
        return false;
    }
    if (!alsa_configure(pb_pcm_, kSampleRate, kChannels, kPeriodFrames)) {
        std::cerr << "[Audio] Ampli config ALSA echouee\n";
        snd_pcm_close(pb_pcm_);
        pb_pcm_ = nullptr;
        return false;
    }
    return true;
}

void AudioManager::closeCapture() {
    if (cap_pcm_) { snd_pcm_close(cap_pcm_); cap_pcm_ = nullptr; }
}

void AudioManager::closePlayback() {
    if (pb_pcm_) { snd_pcm_close(pb_pcm_); pb_pcm_ = nullptr; }
}

void AudioManager::captureLoop() {
    std::vector<int16_t> buf(kPeriodFrames * kChannels);

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        snd_pcm_sframes_t n = snd_pcm_readi(cap_pcm_, buf.data(), kPeriodFrames);

        if (n == -EPIPE) {
            // Underrun/Overrun : on relance simplement
            snd_pcm_prepare(cap_pcm_);
            continue;
        }
        if (n < 0) {
            std::cerr << "[Audio] Micro erreur capture: "
                      << snd_strerror(static_cast<int>(n)) << '\n';
            transitionTo(AudioState::AUDIO_ERROR);
            return;
        }
        if (capture_cb_) {
            capture_cb_({buf.begin(), buf.begin() + n * static_cast<long>(kChannels)});
        }
    }
}

void AudioManager::playbackLoop() {
    snd_pcm_prepare(pb_pcm_);

    const int16_t*      ptr    = playback_buf_.data();
    snd_pcm_uframes_t   remain = playback_buf_.size() / kChannels;

    while (remain > 0 && !stop_flag_.load(std::memory_order_relaxed)) {
        snd_pcm_uframes_t to_write = std::min(remain, kPeriodFrames);
        snd_pcm_sframes_t n        = snd_pcm_writei(pb_pcm_, ptr, to_write);

        if (n == -EPIPE) {
            snd_pcm_prepare(pb_pcm_);
            continue;
        }
        if (n < 0) {
            std::cerr << "[Audio] Ampli erreur lecture: "
                      << snd_strerror(static_cast<int>(n)) << '\n';
            transitionTo(AudioState::AUDIO_ERROR);
            return;
        }
        ptr    += static_cast<snd_pcm_uframes_t>(n) * kChannels;
        remain -= static_cast<snd_pcm_uframes_t>(n);
    }

    // Transition naturelle : lecture terminée
    AudioState expected = AudioState::TALK_MODE;
    if (state_.compare_exchange_strong(expected, AudioState::AUDIO_IDLE,
                                       std::memory_order_release)) {
        std::cout << "[Audio] -> AUDIO_IDLE\n";
    }
}

// ---------- public FSM ----------

bool AudioManager::init() {
    // Doit être appelé depuis AUDIO_INIT
    if (!openCapture() || !openPlayback()) {
        closeCapture();
        closePlayback();
        transitionTo(AudioState::AUDIO_ERROR);
        return false;
    }
    transitionTo(AudioState::AUDIO_IDLE);
    return true;
}

bool AudioManager::startListen(CaptureCallback cb) {
    if (state_.load() != AudioState::AUDIO_IDLE) return false;
    capture_cb_ = std::move(cb);
    stop_flag_.store(false);
    transitionTo(AudioState::LISTEN_MODE);
    capture_thread_ = std::thread(&AudioManager::captureLoop, this);
    return true;
}

bool AudioManager::stopListen() {
    if (state_.load() != AudioState::LISTEN_MODE) return false;
    stop_flag_.store(true);
    if (capture_thread_.joinable()) capture_thread_.join();
    transitionTo(AudioState::AUDIO_IDLE);
    return true;
}

bool AudioManager::startTalk(std::vector<int16_t> samples) {
    if (state_.load() != AudioState::AUDIO_IDLE) return false;
    playback_buf_ = std::move(samples);
    stop_flag_.store(false);
    transitionTo(AudioState::TALK_MODE);
    playback_thread_ = std::thread(&AudioManager::playbackLoop, this);
    return true;
}

bool AudioManager::stopTalk() {
    if (state_.load() != AudioState::TALK_MODE) return false;
    stop_flag_.store(true);
    if (playback_thread_.joinable()) playback_thread_.join();
    if (pb_pcm_) snd_pcm_drop(pb_pcm_);
    // Le thread peut avoir déjà transitionné vers AUDIO_IDLE
    AudioState expected = AudioState::TALK_MODE;
    if (state_.compare_exchange_strong(expected, AudioState::AUDIO_IDLE,
                                       std::memory_order_release)) {
        std::cout << "[Audio] -> AUDIO_IDLE\n";
    }
    return true;
}

void AudioManager::reset() {
    if (state_.load() != AudioState::AUDIO_ERROR) return;
    closeCapture();
    closePlayback();
    transitionTo(AudioState::AUDIO_INIT);
    init();
}

void AudioManager::shutdown() {
    switch (state_.load()) {
        case AudioState::LISTEN_MODE: stopListen(); break;
        case AudioState::TALK_MODE:   stopTalk();   break;
        default: break;
    }
    closeCapture();
    closePlayback();
    std::cout << "[Audio] Service audio arrete\n";
}

}  // namespace spiderbot
