#include "wgc_capture_core.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace oneintwo::windows::detail {
namespace {

constexpr std::uint32_t kMaxDimension = 16384;
constexpr std::uint64_t kMaxPixels = 64ull * 1024ull * 1024ull;

} // namespace

bool saneSize(VisualSize size)
{
    if (size.width == 0 || size.height == 0 || size.width > kMaxDimension
        || size.height > kMaxDimension) {
        return false;
    }
    return static_cast<std::uint64_t>(size.width) * size.height <= kMaxPixels;
}

WgcCaptureCore::WgcCaptureCore(std::unique_ptr<WgcFramePoolFactory> factory,
                               std::unique_ptr<WgcSource> source, VisualSize initial_size,
                               std::function<void()> initialise_thread)
    : factory_(std::move(factory))
    , source_(std::move(source))
    , initialise_thread_(std::move(initialise_thread))
    , initial_size_(initial_size)
{
}

WgcCaptureCore::~WgcCaptureCore()
{
    stop();
}

void WgcCaptureCore::start(Sink sink)
{
    if (started_ || thread_.joinable() || !factory_ || !source_) {
        if (sink.state) {
            sink.state(VisualSourceState::closed);
        }
        return;
    }

    started_ = true;
    sink_ = std::move(sink);
    stop_.store(false);
    suspended_.store(false);
    last_state_.reset();

    try {
        if (initialise_thread_) {
            initialise_thread_();
        }
        pool_ = factory_->create(initial_size_);
        if (!pool_) {
            throw std::runtime_error("WGC frame pool unavailable");
        }
        pool_size_ = initial_size_;
        session_ = pool_->createSession();
        if (!session_) {
            throw std::runtime_error("WGC capture session unvailable");
        }
        session_->configureCursor();
        session_->start();
    } catch (...) {
        releaseGraphics();
        if (sink_.state) {
            sink_.state(VisualSourceState::closed);
        }
        return;
    }

    try {
        thread_ = std::thread([this] { poll(); });
    } catch (...) {
        releaseGraphics();
        if (sink_.state) {
            sink_.state(VisualSourceState::closed);
        }
    }
}

void WgcCaptureCore::stop()
{
    stop_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    releaseGraphics();
    sink_ = {};
}

void WgcCaptureCore::releaseGraphics()
{
    if (session_) {
        try {
            session_->close();
        } catch (...) {
        }
        session_ = nullptr;
    }
    if (pool_) {
        try {
            pool_->close();
        } catch (...) {
        }
        pool_ = nullptr;
    }
    // The factory retains the WinRT item, device and resources.
    // capture is not restarted after stop(), so they are released here.
    factory_ = nullptr;
}

void WgcCaptureCore::publishState(VisualSourceState state)
{
    if (last_state_ && *last_state_ == state) {
        return;
    }
    last_state_ = state;
    if (sink_.state) {
        sink_.state(state);
    }
}

void WgcCaptureCore::poll()
{
    if (initialise_thread_) {
        try {
            initialise_thread_();
        } catch (...) {
            // The actual catch is already initialized in createWin32Capture; if the
            // initializer fails, WinRT objects will report error.
        }
    }
    auto last_check = std::chrono::steady_clock::now();
    while (!stop_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_check >= std::chrono::milliseconds(200)) {
            last_check = now;
            if (!source_->available()) {
                publishState(VisualSourceState::closed);
                stop_.store(true);
                break;
            }
            const bool suspended = source_->suspended();
            if (suspended != suspended_.load()) {
                suspended_.store(suspended);
                publishState(suspended ? VisualSourceState::suspended
                                       : VisualSourceState::available);
            }
        }

        if (suspended_.load() || !session_ || !pool_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::unique_ptr<WgcFrame> frame;
        try {
            frame = pool_->nextFrame();
        } catch (...) {
            publishState(VisualSourceState::closed);
            stop_.store(true);
            break;
        }
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto closeFrame = [&frame] {
            if (frame) {
                try {
                    frame->close();
                } catch (...) {
                }
                frame = nullptr;
            }
        };

        try {
            const VisualSize content = frame->contentSize();
            if (saneSize(content)) {
                if (content.width != pool_size_.width || content.height != pool_size_.height) {
                    // The frame belongs to the previous pool: close it before
                    // recreate the pool and discard this transition frame.
                    closeFrame();
                    if (!pool_->recreate(content)) {
                        publishState(VisualSourceState::closed);
                        stop_.store(true);
                        break;
                    }
                    pool_size_ = content;
                    continue;
                }

                VisualFrame visual;
                if (frame->copyTo(visual) && sink_.frame) {
                    sink_.frame(std::move(visual));
                }
            }
            closeFrame();
        } catch (...) {
            closeFrame();
            publishState(VisualSourceState::closed);
            stop_.store(true);
            break;
        }
    }
}

} // namespace oneintwo::windows::detail
