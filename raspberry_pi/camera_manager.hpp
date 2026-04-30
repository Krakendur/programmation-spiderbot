#pragma once

#include <libcamera/libcamera.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace spiderbot {

enum class VideoState {
    CAMERA_INIT,
    CAMERA_READY,
    STREAMING,
    STREAM_PAUSED,
    VIDEO_ERROR,
};

const char* videoStateStr(VideoState s);

struct Frame {
    std::vector<uint8_t> data;
    uint32_t             width;
    uint32_t             height;
};

class CameraManager {
public:
    static constexpr uint32_t kWidth     = 1280;
    static constexpr uint32_t kHeight    = 720;
    static constexpr uint32_t kFramerate = 30;

    // Appelé pour chaque frame capturée (STREAMING)
    using FrameCallback = std::function<void(const Frame&)>;

    CameraManager();
    ~CameraManager();

    CameraManager(const CameraManager&)            = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // --- Transitions FSM ---
    bool init();            // CAMERA_INIT  -> CAMERA_READY | VIDEO_ERROR
    bool startStream(FrameCallback cb);  // CAMERA_READY  -> STREAMING
    bool pauseStream();     // STREAMING    -> STREAM_PAUSED
    bool resumeStream();    // STREAM_PAUSED -> STREAMING
    bool stopStream();      // STREAMING | STREAM_PAUSED -> CAMERA_READY
    void reset();           // VIDEO_ERROR  -> CAMERA_INIT -> CAMERA_READY
    void shutdown();        // tout état    -> [*]

    VideoState state() const { return state_.load(std::memory_order_acquire); }

private:
    void requestComplete(libcamera::Request* request);
    void dispatchLoop();
    void transitionTo(VideoState s);
    void releaseResources();

    std::unique_ptr<libcamera::CameraManager>    cam_manager_;
    std::shared_ptr<libcamera::Camera>           camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    libcamera::FrameBufferAllocator*             allocator_{nullptr};
    std::vector<std::unique_ptr<libcamera::Request>> requests_;

    std::atomic<VideoState> state_{VideoState::CAMERA_INIT};
    std::atomic<bool>       paused_{false};

    // File des frames complétées pour le thread de dispatch
    std::queue<Frame>       frame_queue_;
    std::mutex              queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool>       stop_dispatch_{false};
    std::thread             dispatch_thread_;

    FrameCallback           frame_cb_;
};

}  // namespace spiderbot
