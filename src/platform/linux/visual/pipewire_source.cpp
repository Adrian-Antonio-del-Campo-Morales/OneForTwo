#include "pipewire_source.hpp"
#include "video_frame_convert.hpp"

#include <pipewire/core.h>
#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/raw-utils.h>
#include <spa/pod/builder.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unistd.h>

namespace oneintwo::wayland {
namespace {

constexpr auto minimum_frame_interval = std::chrono::milliseconds(33);

VideoPixelFormat mapFormat(spa_video_format format)
{
    switch (format) {
    case SPA_VIDEO_FORMAT_BGRA:
        return VideoPixelFormat::bgra;
    case SPA_VIDEO_FORMAT_BGRx:
        return VideoPixelFormat::bgrx;
    case SPA_VIDEO_FORMAT_RGBA:
        return VideoPixelFormat::rgba;
    case SPA_VIDEO_FORMAT_RGBx:
        return VideoPixelFormat::rgbx;
    default:
        return VideoPixelFormat::unknown;
    }
}

void initialisePipeWire()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

} // namespace

class PipeWireSourceImpl final : public PipeWireSource {
public:
    ~PipeWireSourceImpl() override { stop(); }

    bool start(int fd, std::uint32_t node_id, Callbacks callbacks) override
    {
        if (loop_ != nullptr || fd < 0) {
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
        initialisePipeWire();
        callbacks_ = std::move(callbacks);
        stopping_.store(false);
        closed_reported_.store(false);
        started_.store(false);

        loop_ = pw_thread_loop_new("oneintwo-screencast", nullptr);
        if (loop_ == nullptr) {
            ::close(fd);
            return false;
        }

        pw_thread_loop_lock(loop_);
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            cleanupAfterFailure(fd);
            return false;
        }
        core_ = pw_context_connect_fd(context_, fd, nullptr, 0);
        if (core_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            cleanupAfterFailure(fd);
            return false;
        }
        // El core toma la propiedad del descriptor a partir de este punto.
        stream_ = pw_stream_new(
            core_, "oneintwo-capture",
            pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Screen", nullptr));
        if (stream_ == nullptr) {
            pw_thread_loop_unlock(loop_);
            cleanupAfterFailure(-1);
            return false;
        }
        pw_stream_add_listener(stream_, &listener_, &stream_events_, this);

        std::uint8_t pod_buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
        const spa_pod* params[1];
        params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_BGRA,
                                   SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBA,
                                   SPA_VIDEO_FORMAT_RGBx)));

        const int result = pw_stream_connect(
            stream_, PW_DIRECTION_INPUT, node_id,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT), params, 1);
        pw_thread_loop_unlock(loop_);

        if (result < 0) {
            cleanupAfterFailure(-1);
            return false;
        }
        if (pw_thread_loop_start(loop_) < 0) {
            cleanupAfterFailure(-1);
            return false;
        }
        started_.store(true);
        return true;
    }

    void stop() override
    {
        stopping_.store(true);
        if (loop_ != nullptr) {
            pw_thread_loop_stop(loop_);
            pw_thread_loop_lock(loop_);
            if (stream_ != nullptr) {
                spa_hook_remove(&listener_);
                pw_stream_destroy(stream_);
                stream_ = nullptr;
            }
            if (core_ != nullptr) {
                pw_core_disconnect(core_);
                core_ = nullptr;
            }
            if (context_ != nullptr) {
                pw_context_destroy(context_);
                context_ = nullptr;
            }
            pw_thread_loop_unlock(loop_);
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
        }
        callbacks_ = {};
    }

private:
    static void onProcess(void* data)
    {
        static_cast<PipeWireSourceImpl*>(data)->process();
    }

    static void onParamChanged(void* data, std::uint32_t id, const spa_pod* param)
    {
        static_cast<PipeWireSourceImpl*>(data)->handleFormat(id, param);
    }

    static void onStateChanged(void* data, pw_stream_state, pw_stream_state state,
                               const char* error)
    {
        auto* self = static_cast<PipeWireSourceImpl*>(data);
        if (state == PW_STREAM_STATE_ERROR) {
            (void)error;
            self->reportClosed();
        }
    }

    void cleanupAfterFailure(int fd)
    {
        if (stream_ != nullptr) {
            spa_hook_remove(&listener_);
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (core_ != nullptr) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }
        if (context_ != nullptr) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }
        if (loop_ != nullptr) {
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
        }
        if (fd >= 0) {
            ::close(fd);
        }
        callbacks_ = {};
    }

    void reportClosed()
    {
        bool expected = false;
        if (!closed_reported_.compare_exchange_strong(expected, true)) {
            return;
        }
        auto callback = callbacks_.closed;
        if (callback) {
            callback();
        }
    }

    void handleFormat(std::uint32_t id, const spa_pod* param)
    {
        if (id != SPA_PARAM_Format || param == nullptr || stopping_.load()) {
            return;
        }
        spa_video_info_raw info;
        if (spa_format_video_raw_parse(param, &info) < 0) {
            return;
        }
        const VideoPixelFormat format = mapFormat(info.format);
        if (format == VideoPixelFormat::unknown || info.size.width == 0
            || info.size.height == 0) {
            reportClosed();
            return;
        }
        const bool resized = info.size.width != width_ || info.size.height != height_;
        format_ = format;
        width_ = info.size.width;
        height_ = info.size.height;
        if (resized) {
            auto callback = callbacks_.size_changed;
            if (callback) {
                callback({width_, height_});
            }
        }
    }

    void process()
    {
        if (stopping_.load() || stream_ == nullptr) {
            return;
        }
        pw_buffer* buffer = nullptr;
        while ((buffer = pw_stream_dequeue_buffer(stream_)) != nullptr) {
            if (buffer->buffer != nullptr) {
                deliver(buffer->buffer);
            }
            pw_stream_queue_buffer(stream_, buffer);
        }
    }

    void deliver(spa_buffer* buffer)
    {
        if (format_ == VideoPixelFormat::unknown || width_ == 0 || height_ == 0) {
            return;
        }
        spa_data* data = &buffer->datas[0];
        if (data->data == nullptr || data->chunk == nullptr) {
            return;
        }
        const std::uint32_t chunk_size = data->chunk->size;
        const std::uint32_t offset = data->chunk->offset;
        if (chunk_size == 0 || offset > data->maxsize) {
            return;
        }
        const auto* pixels = static_cast<const std::uint8_t*>(data->data) + offset;
        const std::size_t available = std::min<std::size_t>(
            chunk_size, static_cast<std::size_t>(data->maxsize) - offset);

        VideoBufferView view;
        view.format = format_;
        view.width = width_;
        view.height = height_;
        view.stride = data->chunk->stride;
        view.data = pixels;
        view.bytes = available;

        VisualFrame frame;
        const FrameConvertResult result = convertToBgra(view, frame);
        if (result == FrameConvertResult::unsupported_format) {
            reportClosed();
            return;
        }
        if (result != FrameConvertResult::ok) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (last_frame_ != std::chrono::steady_clock::time_point{}
            && now - last_frame_ < minimum_frame_interval) {
            return;
        }
        last_frame_ = now;
        auto callback = callbacks_.frame;
        if (callback) {
            callback(std::move(frame));
        }
    }

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_stream* stream_ = nullptr;
    spa_hook listener_{};
    const pw_stream_events stream_events_ = [] {
        pw_stream_events events{};
        events.version = PW_VERSION_STREAM_EVENTS;
        events.process = &PipeWireSourceImpl::onProcess;
        events.param_changed = &PipeWireSourceImpl::onParamChanged;
        events.state_changed = &PipeWireSourceImpl::onStateChanged;
        return events;
    }();

    Callbacks callbacks_;
    VideoPixelFormat format_ = VideoPixelFormat::unknown;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> closed_reported_{false};
    std::atomic<bool> started_{false};
    std::chrono::steady_clock::time_point last_frame_{};
};

std::unique_ptr<PipeWireSource> makePipeWireSource()
{
    return std::make_unique<PipeWireSourceImpl>();
}

} // namespace oneintwo::wayland
