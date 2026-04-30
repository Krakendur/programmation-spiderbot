#include "camera_manager.hpp"

#include <iostream>

#include <libcamera/formats.h>

namespace spiderbot {

// ---------- utilitaires ----------

const char* videoStateStr(VideoState s) {
    switch (s) {
        case VideoState::CAMERA_INIT:   return "CAMERA_INIT";
        case VideoState::CAMERA_READY:  return "CAMERA_READY";
        case VideoState::STREAMING:     return "STREAMING";
        case VideoState::STREAM_PAUSED: return "STREAM_PAUSED";
        case VideoState::VIDEO_ERROR:   return "VIDEO_ERROR";
    }
    return "UNKNOWN";
}

// ---------- ctor / dtor ----------

CameraManager::CameraManager() = default;

CameraManager::~CameraManager() { shutdown(); }

// ---------- privé ----------

void CameraManager::transitionTo(VideoState s) {
    state_.store(s, std::memory_order_release);
    std::cout << "[Video] -> " << videoStateStr(s) << '\n';
}

void CameraManager::releaseResources() {
    if (camera_) {
        camera_->stop();
        requests_.clear();
        if (allocator_) {
            allocator_->free(config_->at(0).stream());
            delete allocator_;
            allocator_ = nullptr;
        }
        config_.reset();
        camera_->release();
        camera_.reset();
    }
    if (cam_manager_) {
        cam_manager_->stop();
        cam_manager_.reset();
    }
}

void CameraManager::requestComplete(libcamera::Request* request) {
    if (request->status() == libcamera::Request::RequestCancelled) return;

    const auto& buffers = request->buffers();
    for (const auto& [stream, fb] : buffers) {
        const auto& planes = fb->planes();
        if (planes.empty()) continue;

        // Copie des données de la frame dans un vecteur
        Frame frame;
        frame.width  = kWidth;
        frame.height = kHeight;

        const libcamera::FrameMetadata& meta = fb->metadata();
        for (const auto& plane_meta : meta.planes()) {
            const libcamera::FrameBuffer::Plane& plane = planes[0];
            // Mappe le dma-buf pour lire les pixels
            void* mem = mmap(nullptr, plane_meta.bytesused,
                             PROT_READ, MAP_SHARED, plane.fd.get(), 0);
            if (mem != MAP_FAILED) {
                const auto* src = static_cast<const uint8_t*>(mem);
                frame.data.insert(frame.data.end(), src,
                                  src + plane_meta.bytesused);
                munmap(mem, plane_meta.bytesused);
            }
            break; // une seule plane pour NV12/YUYV
        }

        if (!paused_.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lk(queue_mutex_);
            // On limite la file à 2 frames pour ne pas saturer la RAM
            if (frame_queue_.size() < 2) {
                frame_queue_.push(std::move(frame));
            }
            queue_cv_.notify_one();
        }
    }

    // Re-queue la request pour continuer le streaming
    request->reuse(libcamera::Request::ReuseBuffers);
    if (camera_) camera_->queueRequest(request);
}

void CameraManager::dispatchLoop() {
    while (!stop_dispatch_.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lk(queue_mutex_);
        queue_cv_.wait(lk, [this] {
            return !frame_queue_.empty() ||
                   stop_dispatch_.load(std::memory_order_relaxed);
        });

        while (!frame_queue_.empty()) {
            Frame frame = std::move(frame_queue_.front());
            frame_queue_.pop();
            lk.unlock();
            if (frame_cb_) frame_cb_(frame);
            lk.lock();
        }
    }
}

// ---------- public FSM ----------

bool CameraManager::init() {
    cam_manager_ = std::make_unique<libcamera::CameraManager>();
    if (cam_manager_->start() < 0) {
        std::cerr << "[Video] CameraManager start echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    const auto& cameras = cam_manager_->cameras();
    if (cameras.empty()) {
        std::cerr << "[Video] Aucune camera detectee\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    camera_ = cameras[0];
    if (camera_->acquire() < 0) {
        std::cerr << "[Video] Camera acquire echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    config_ = camera_->generateConfiguration(
        {libcamera::StreamRole::VideoRecording});
    if (!config_) {
        std::cerr << "[Video] generateConfiguration echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    libcamera::StreamConfiguration& stream_cfg = config_->at(0);
    stream_cfg.pixelFormat = libcamera::formats::NV12;
    stream_cfg.size        = {kWidth, kHeight};
    stream_cfg.bufferCount = 4; // 4 buffers en rotation

    if (config_->validate() == libcamera::CameraConfiguration::Invalid) {
        std::cerr << "[Video] Configuration invalide\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    if (camera_->configure(config_.get()) < 0) {
        std::cerr << "[Video] Camera configure echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    allocator_ = new libcamera::FrameBufferAllocator(camera_);
    libcamera::Stream* stream = config_->at(0).stream();
    if (allocator_->allocate(stream) < 0) {
        std::cerr << "[Video] FrameBufferAllocator echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }

    for (const auto& buf : allocator_->buffers(stream)) {
        auto req = camera_->createRequest();
        if (!req || req->addBuffer(stream, buf.get()) < 0) {
            std::cerr << "[Video] createRequest echoue\n";
            transitionTo(VideoState::VIDEO_ERROR);
            return false;
        }
        requests_.push_back(std::move(req));
    }

    // Branchement du signal requestCompleted
    camera_->requestCompleted.connect(this, &CameraManager::requestComplete);

    transitionTo(VideoState::CAMERA_READY);
    return true;
}

bool CameraManager::startStream(FrameCallback cb) {
    if (state_.load() != VideoState::CAMERA_READY) return false;
    frame_cb_       = std::move(cb);
    paused_.store(false);
    stop_dispatch_.store(false);

    if (camera_->start() < 0) {
        std::cerr << "[Video] Camera start echoue\n";
        transitionTo(VideoState::VIDEO_ERROR);
        return false;
    }
    for (auto& req : requests_) {
        if (camera_->queueRequest(req.get()) < 0) {
            std::cerr << "[Video] queueRequest echoue\n";
            transitionTo(VideoState::VIDEO_ERROR);
            return false;
        }
    }

    dispatch_thread_ = std::thread(&CameraManager::dispatchLoop, this);
    transitionTo(VideoState::STREAMING);
    return true;
}

bool CameraManager::pauseStream() {
    if (state_.load() != VideoState::STREAMING) return false;
    paused_.store(true);
    transitionTo(VideoState::STREAM_PAUSED);
    return true;
}

bool CameraManager::resumeStream() {
    if (state_.load() != VideoState::STREAM_PAUSED) return false;
    paused_.store(false);
    transitionTo(VideoState::STREAMING);
    return true;
}

bool CameraManager::stopStream() {
    const VideoState s = state_.load();
    if (s != VideoState::STREAMING && s != VideoState::STREAM_PAUSED) return false;

    stop_dispatch_.store(true);
    queue_cv_.notify_all();
    if (dispatch_thread_.joinable()) dispatch_thread_.join();

    if (camera_) camera_->stop();
    transitionTo(VideoState::CAMERA_READY);
    return true;
}

void CameraManager::reset() {
    if (state_.load() != VideoState::VIDEO_ERROR) return;
    releaseResources();
    transitionTo(VideoState::CAMERA_INIT);
    init();
}

void CameraManager::shutdown() {
    const VideoState s = state_.load();
    if (s == VideoState::STREAMING || s == VideoState::STREAM_PAUSED) {
        stopStream();
    }
    releaseResources();
    std::cout << "[Video] Service video arrete\n";
}

}  // namespace spiderbot
