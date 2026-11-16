#include "wayland_visual_capture.hpp"
#include "parent_window.hpp"
#include "video_frame_convert.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>

namespace oneintwo::wayland {
namespace {

struct Lifetime {
    std::atomic<bool> alive{true};
};

enum class Phase {
    idle,
    selecting,
    selected,
    capturing,
    closed,
};

bool sameRef(const VisualShareRef& left, const VisualShareRef& right)
{
    return left.generation == right.generation && left.initiator == right.initiator
        && left.offer_id == right.offer_id;
}

DesktopResult mapStatus(ScreenCastStatus status)
{
    switch (status) {
    case ScreenCastStatus::ready:
        return DesktopResult::ready;
    case ScreenCastStatus::cancelled:
        return DesktopResult::cancelled;
    case ScreenCastStatus::denied:
        return DesktopResult::denied;
    case ScreenCastStatus::unavailable:
        return DesktopResult::unavailable;
    case ScreenCastStatus::error:
        return DesktopResult::error;
    }
    return DesktopResult::error;
}

} // namespace

class WaylandVisualCapture final : public DesktopVisualCapture {
public:
    WaylandVisualCapture(PlatformContext context, WaylandCaptureDependencies dependencies)
        : context_(std::move(context))
        , dependencies_(std::move(dependencies))
        , lifetime_(std::make_shared<Lifetime>())
        , portal_(dependencies_.portal ? dependencies_.portal() : nullptr)
    {
        if (portal_) {
            portal_->probe([this](ScreenCastSupport support, bool available) {
                post([this, support, available] { applyCapabilities(support, available); });
            });
        }
    }

    ~WaylandVisualCapture() override
    {
        lifetime_->alive.store(false);
        ++operation_epoch_;
        if (pipe_) {
            pipe_->stop();
            pipe_.reset();
        }
        if (portal_) {
            portal_->close();
            portal_.reset();
        }
    }

    VisualCaptureCapabilities visualCapabilities() const override
    {
        return capabilities_;
    }

    void selectSource(VisualShareRef offer, VisualSourceKind kind,
                      std::function<void(VisualShareRef, DesktopResult)> selected) override
    {
        if (!selected) {
            return;
        }
        if (phase_ != Phase::idle) {
            post([selected = std::move(selected), offer] { selected(offer, DesktopResult::error); });
            return;
        }
        if (!portal_) {
            post([selected = std::move(selected), offer] {
                selected(offer, DesktopResult::unavailable);
            });
            return;
        }

        phase_ = Phase::selecting;
        has_operation_ = true;
        operation_ref_ = offer;
        operation_kind_ = kind;
        selection_callback_ = std::move(selected);
        frame_size_ = {};
        last_state_.reset();
        source_lost_ = false;
        const std::uint64_t epoch = ++operation_epoch_;
        session_epoch_ = epoch;

        portal_->probe([this, epoch](ScreenCastSupport support, bool available) {
            post([this, epoch, support, available] {
                applyCapabilities(support, available);
                if (phase_ != Phase::selecting || epoch != operation_epoch_) {
                    return;
                }
                const bool kind_supported = operation_kind_ == VisualSourceKind::monitor
                    ? support.monitor
                    : support.window;
                if (!available || !kind_supported) {
                    finishSelection(DesktopResult::unavailable);
                    return;
                }
                openSession(epoch);
            });
        });
    }

    void startCapture(VisualShareRef offer, VisualCaptureCallbacks callbacks) override
    {
        if (!has_operation_ || !sameRef(offer, operation_ref_)
            || (phase_ != Phase::selected && phase_ != Phase::closed)) {
            if (callbacks.state_changed) {
                post([callback = callbacks.state_changed, offer] {
                    callback(offer, VisualSourceState::closed);
                });
            }
            return;
        }

        capture_callbacks_ = std::move(callbacks);
        if (phase_ == Phase::closed || source_lost_ || !portal_) {
            phase_ = Phase::closed;
            if (portal_) {
                portal_->close();
            }
            reportState(VisualSourceState::closed);
            return;
        }

        const int fd = portal_->duplicatePipeWireFd();
        if (fd < 0) {
            closeSource();
            return;
        }
        pipe_ = dependencies_.pipewire ? dependencies_.pipewire() : nullptr;
        if (!pipe_) {
            ::close(fd);
            closeSource();
            return;
        }

        phase_ = Phase::capturing;
        frame_size_ = {};
        last_state_.reset();
        const std::uint64_t epoch = ++operation_epoch_;

        PipeWireSource::Callbacks pipe_callbacks;
        pipe_callbacks.frame = [this, epoch](VisualFrame frame) {
            post([this, epoch, frame = std::move(frame)]() mutable {
                handleFrame(epoch, std::move(frame));
            });
        };
        pipe_callbacks.size_changed = [this, epoch](VisualSize size) {
            post([this, epoch, size] { handleSizeHint(epoch, size); });
        };
        pipe_callbacks.closed = [this, epoch] {
            post([this, epoch] { handlePipeWireClosed(epoch); });
        };

        if (!pipe_->start(fd, source_.node_id, std::move(pipe_callbacks))) {
            pipe_.reset();
            closeSource();
        }
    }

    void stopCapture(VisualShareRef offer) override
    {
        if (!has_operation_ || !sameRef(offer, operation_ref_)) {
            return;
        }
        ++operation_epoch_;
        ++session_epoch_;
        const Phase phase = phase_;
        phase_ = Phase::idle;
        has_operation_ = false;
        operation_ref_ = {};
        frame_size_ = {};
        last_state_.reset();

        if (phase == Phase::selecting) {
            auto callback = std::move(selection_callback_);
            selection_callback_ = nullptr;
            if (portal_) {
                portal_->cancel();
                portal_->close();
            }
            if (callback) {
                post([callback = std::move(callback), offer] {
                    callback(offer, DesktopResult::cancelled);
                });
            }
            return;
        }

        selection_callback_ = nullptr;
        capture_callbacks_ = {};
        if (pipe_) {
            pipe_->stop();
            pipe_.reset();
        }
        if (portal_) {
            portal_->close();
        }
    }

private:
    void post(std::function<void()> task)
    {
        if (!context_.runtime.post) {
            return;
        }
        auto lifetime = lifetime_;
        context_.runtime.post([lifetime, task = std::move(task)]() mutable {
            if (lifetime->alive.load()) {
                task();
            }
        });
    }

    void applyCapabilities(ScreenCastSupport support, bool available)
    {
        VisualCaptureCapabilities next;
        next.monitor = available && support.monitor;
        next.window = available && support.window;
        const bool changed = next.monitor != capabilities_.monitor
            || next.window != capabilities_.window;
        capabilities_ = next;
        if (changed && context_.events.capabilities_changed) {
            context_.events.capabilities_changed();
        }
    }

    void openSession(std::uint64_t epoch)
    {
        const std::uint64_t session_epoch = session_epoch_;
        portal_->open(
            operation_kind_, portalParentWindow(context_),
            [this, session_epoch] {
                post([this, session_epoch] { handleSessionClosed(session_epoch); });
            },
            [this, epoch](ScreenCastStatus status, ScreenCastSource source) {
                post([this, epoch, status, source] { handleSessionResult(epoch, status, source); });
            });
    }

    void handleSessionResult(std::uint64_t epoch, ScreenCastStatus status,
                             const ScreenCastSource& source)
    {
        if (phase_ != Phase::selecting || epoch != operation_epoch_) {
            return;
        }
        if (status != ScreenCastStatus::ready) {
            finishSelection(mapStatus(status));
            return;
        }
        const std::uint32_t expected_type
            = operation_kind_ == VisualSourceKind::monitor ? 1u : 2u;
        if (!source.has_source_type || source.source_type != expected_type
            || source.node_id == 0 || source.width == 0 || source.height == 0
            || source.width > max_capture_dimension || source.height > max_capture_dimension
            || static_cast<std::uint64_t>(source.width) * source.height > max_capture_pixels) {
            // El portal no acredita el tipo o las dimensiones no son utilizables.
            if (portal_) {
                portal_->close();
            }
            finishSelection(status == ScreenCastStatus::ready ? DesktopResult::unavailable
                                                             : mapStatus(status));
            return;
        }
        source_ = source;
        phase_ = Phase::selected;
        auto callback = std::move(selection_callback_);
        selection_callback_ = nullptr;
        if (callback) {
            post([callback = std::move(callback), offer = operation_ref_] {
                callback(offer, DesktopResult::ready);
            });
        }
    }

    void finishSelection(DesktopResult result)
    {
        ++operation_epoch_;
        phase_ = Phase::idle;
        has_operation_ = false;
        const VisualShareRef offer = operation_ref_;
        operation_ref_ = {};
        auto callback = std::move(selection_callback_);
        selection_callback_ = nullptr;
        if (portal_) {
            portal_->close();
        }
        if (callback) {
            post([callback = std::move(callback), offer, result] { callback(offer, result); });
        }
    }

    void handleSessionClosed(std::uint64_t epoch)
    {
        if (epoch != session_epoch_) {
            return;
        }
        if (phase_ == Phase::capturing) {
            closeSource();
            return;
        }
        if (phase_ == Phase::selected) {
            source_lost_ = true;
            if (portal_) {
                portal_->close();
            }
            return;
        }
        if (phase_ == Phase::selecting) {
            finishSelection(DesktopResult::error);
        }
    }

    void handleFrame(std::uint64_t epoch, VisualFrame frame)
    {
        if (epoch != operation_epoch_ || phase_ != Phase::capturing) {
            return;
        }
        if (!frameContractHolds(frame)) {
            return;
        }
        if (frame.size.width != frame_size_.width || frame.size.height != frame_size_.height) {
            const bool resized = frame_size_.width != 0;
            frame_size_ = frame.size;
            if (resized) {
                announceAvailable();
            }
        }
        if (capture_callbacks_.frame) {
            capture_callbacks_.frame(operation_ref_, std::move(frame));
        }
    }

    void handleSizeHint(std::uint64_t epoch, VisualSize size)
    {
        if (epoch != operation_epoch_ || phase_ != Phase::capturing) {
            return;
        }
        if (size.width == 0 || size.height == 0) {
            return;
        }
        if (frame_size_.width == 0) {
            frame_size_ = size;
            return;
        }
        if (size.width != frame_size_.width || size.height != frame_size_.height) {
            frame_size_ = size;
            announceAvailable();
        }
    }

    void announceAvailable()
    {
        if (last_state_ == VisualSourceState::available) {
            return;
        }
        last_state_ = VisualSourceState::available;
        if (capture_callbacks_.state_changed) {
            capture_callbacks_.state_changed(operation_ref_, VisualSourceState::available);
        }
    }

    void handlePipeWireClosed(std::uint64_t epoch)
    {
        if (epoch != operation_epoch_ || phase_ != Phase::capturing) {
            return;
        }
        closeSource();
    }

    void closeSource()
    {
        if (pipe_) {
            pipe_->stop();
            pipe_.reset();
        }
        phase_ = Phase::closed;
        if (portal_) {
            portal_->close();
        }
        reportState(VisualSourceState::closed);
    }

    void reportState(VisualSourceState state)
    {
        if (last_state_ == state) {
            return;
        }
        last_state_ = state;
        if (capture_callbacks_.state_changed) {
            capture_callbacks_.state_changed(operation_ref_, state);
        }
    }

    PlatformContext context_;
    WaylandCaptureDependencies dependencies_;
    std::shared_ptr<Lifetime> lifetime_;
    std::unique_ptr<PortalScreencastClient> portal_;
    std::unique_ptr<PipeWireSource> pipe_;

    VisualCaptureCapabilities capabilities_{};
    Phase phase_ = Phase::idle;
    bool has_operation_ = false;
    bool source_lost_ = false;
    VisualShareRef operation_ref_{};
    VisualSourceKind operation_kind_ = VisualSourceKind::monitor;
    std::function<void(VisualShareRef, DesktopResult)> selection_callback_;
    VisualCaptureCallbacks capture_callbacks_;
    ScreenCastSource source_{};
    std::uint64_t operation_epoch_ = 0;
    std::uint64_t session_epoch_ = 0;
    VisualSize frame_size_{};
    std::optional<VisualSourceState> last_state_;
};

std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCaptureWithDependencies(
    PlatformContext context, WaylandCaptureDependencies dependencies)
{
    return std::make_unique<WaylandVisualCapture>(std::move(context), std::move(dependencies));
}

} // namespace oneintwo::wayland
