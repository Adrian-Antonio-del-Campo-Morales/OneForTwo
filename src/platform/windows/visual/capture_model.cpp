#include "capture_model.hpp"

#include <cstddef>
#include <utility>

namespace oneintwo::windows {
namespace {

bool sameRef(const VisualShareRef& left, const VisualShareRef& right)
{
    return left.generation == right.generation && left.initiator == right.initiator
        && left.offer_id == right.offer_id;
}

constexpr std::uint32_t kMaxDimension = 16384;
constexpr std::uint64_t kMaxPixels = 64ull * 1024ull * 1024ull;

bool frameIsSane(const VisualFrame& frame)
{
    if (frame.size.width == 0 || frame.size.height == 0
        || frame.size.width > kMaxDimension || frame.size.height > kMaxDimension) {
        return false;
    }
    const std::uint64_t pixels
        = static_cast<std::uint64_t>(frame.size.width) * frame.size.height;
    if (pixels > kMaxPixels) {
        return false;
    }
    return frame.bgra.size() == pixels * 4ull;
}

void makeOpaque(VisualFrame& frame)
{
    const std::size_t count = frame.bgra.size() / 4;
    for (std::size_t index = 0; index < count; ++index) {
        frame.bgra[index * 4 + 3] = 0xFF;
    }
}

} // namespace

struct CaptureModel::Lifetime {
    std::atomic<bool> alive{true};
};

struct CaptureModel::Selection {
    VisualShareRef ref{};
    VisualSourceKind kind = VisualSourceKind::monitor;
    std::function<void(VisualShareRef, DesktopResult)> callback;
    std::uint64_t epoch = 0;
};

struct CaptureModel::Active {
    VisualShareRef ref{};
    VisualSourceKind kind = VisualSourceKind::monitor;
    std::unique_ptr<NativeCapture> capture;
    VisualCaptureCallbacks callbacks;
    VisualSize size{};
    std::uint64_t epoch = 0;
    std::uint64_t sequence = 0;
    std::uint64_t last_sequence = 0;
    bool capturing = false;
    bool closed = false;
    bool suspended = false;
    std::optional<VisualSourceState> last_state;
};

CaptureModel::CaptureModel(PlatformContext context, std::unique_ptr<WindowsCapturePort> port)
    : runtime_(std::move(context.runtime))
    , lifetime_(std::make_shared<Lifetime>())
    , port_(std::move(port))
{
    if (port_) {
        capabilities_ = port_->capabilities();
    }
}

CaptureModel::~CaptureModel()
{
    lifetime_->alive.store(false);
    if (selection_) {
        selection_.reset();
        if (port_) {
            port_->cancelPrompt();
        }
    }
    if (auto active = active_) {
        ++active->epoch;
        active->capturing = false;
        auto capture = std::move(active->capture);
        active_.reset();
        if (capture) {
            capture->stop();
        }
    }
}

VisualCaptureCapabilities CaptureModel::visualCapabilities() const
{
    return capabilities_;
}

void CaptureModel::selectSource(
    VisualShareRef offer, VisualSourceKind kind,
    std::function<void(VisualShareRef, DesktopResult)> selected)
{
    if (!selected) {
        return;
    }
    if (selection_ || active_) {
        post([selected = std::move(selected), offer] { selected(offer, DesktopResult::error); });
        return;
    }
    const bool supported = kind == VisualSourceKind::monitor ? capabilities_.monitor
                                                             : capabilities_.window;
    if (!port_ || !supported) {
        post([selected = std::move(selected), offer] {
            selected(offer, DesktopResult::unavailable);
        });
        return;
    }

    auto sources = port_->enumerate(kind);
    if (sources.empty()) {
        post([selected = std::move(selected), offer] {
            selected(offer, DesktopResult::unavailable);
        });
        return;
    }

    auto selection = std::make_unique<Selection>();
    selection->ref = offer;
    selection->kind = kind;
    selection->callback = std::move(selected);
    selection->epoch = ++epoch_;
    selection_ = std::move(selection);
    const std::uint64_t epoch = selection_->epoch;

    port_->prompt(kind, sources, [this, epoch](std::optional<NativeSource> chosen) {
        post([this, epoch, chosen = std::move(chosen)]() mutable {
            onPrompt(epoch, std::move(chosen));
        });
    });
}

void CaptureModel::onPrompt(std::uint64_t epoch, std::optional<NativeSource> chosen)
{
    if (!selection_ || selection_->epoch != epoch) {
        return;
    }
    if (!chosen) {
        finishSelection(epoch, DesktopResult::cancelled);
        return;
    }
    auto capture = port_->adopt(*chosen, selection_->kind);
    if (!capture) {
        finishSelection(epoch, DesktopResult::unavailable);
        return;
    }

    auto active = std::make_shared<Active>();
    active->ref = selection_->ref;
    active->kind = selection_->kind;
    active->capture = std::move(capture);
    active->epoch = ++epoch_;
    const VisualShareRef ref = active->ref;
    auto callback = std::move(selection_->callback);
    selection_.reset();
    active_ = active;
    if (callback) {
        post([callback = std::move(callback), ref] { callback(ref, DesktopResult::ready); });
    }
}

void CaptureModel::startCapture(VisualShareRef offer, VisualCaptureCallbacks callbacks)
{
    auto active = active_;
    if (!active || !sameRef(active->ref, offer)) {
        if (callbacks.state_changed) {
            post([callback = callbacks.state_changed, offer] {
                callback(offer, VisualSourceState::closed);
            });
        }
        return;
    }

    active->callbacks = std::move(callbacks);
    if (active->closed) {
        announce(*active, VisualSourceState::closed);
        return;
    }

    active->capturing = true;
    active->suspended = false;
    active->size = {};
    active->last_state.reset();
    active->sequence = 0;
    active->last_sequence = 0;
    const std::uint64_t epoch = ++active->epoch;
    NativeCapture* capture = active->capture.get();

    NativeCapture::Sink sink;
    sink.frame = [this, epoch, offer](VisualFrame frame) {
        post([this, epoch, offer, frame = std::move(frame)]() mutable {
            handleFrame(epoch, offer, std::move(frame));
        });
    };
    sink.state = [this, epoch, offer](VisualSourceState state) {
        post([this, epoch, offer, state] { handleState(epoch, offer, state); });
    };

    try {
        capture->start(std::move(sink));
    } catch (...) {
        post([this, epoch, offer] { handleState(epoch, offer, VisualSourceState::closed); });
    }
}

void CaptureModel::handleFrame(std::uint64_t epoch, VisualShareRef offer, VisualFrame frame)
{
    auto active = active_;
    if (!active || active->epoch != epoch || !sameRef(active->ref, offer)
        || !active->capturing || active->suspended) {
        return;
    }
    if (!frameIsSane(frame)) {
        return;
    }
    makeOpaque(frame);

    const bool resized = active->size.width != 0
        && (active->size.width != frame.size.width || active->size.height != frame.size.height);
    active->size = frame.size;
    ++active->sequence;
    const std::uint64_t sequence = active->sequence;
    const bool ordered = sequence > active->last_sequence;
    active->last_sequence = sequence;
    if (resized) {
        announce(*active, VisualSourceState::available);
    }
    if (!ordered || !active->callbacks.frame) {
        return;
    }
    active->callbacks.frame(offer, std::move(frame));
}

void CaptureModel::handleState(std::uint64_t epoch, VisualShareRef offer,
                               VisualSourceState state)
{
    auto active = active_;
    if (!active || active->epoch != epoch || !sameRef(active->ref, offer)) {
        return;
    }
    if (state == VisualSourceState::closed) {
        if (active->closed) {
            return;
        }
        active->closed = true;
        active->capturing = false;
        auto capture = std::move(active->capture);
        announce(*active, VisualSourceState::closed);
        if (capture) {
            capture->stop();
        }
        return;
    }
    if (!active->capturing) {
        return;
    }
    if (state == VisualSourceState::suspended) {
        active->suspended = true;
    } else if (state == VisualSourceState::available) {
        active->suspended = false;
    }
    announce(*active, state);
}

void CaptureModel::announce(Active& active, VisualSourceState state)
{
    if (active.last_state && *active.last_state == state) {
        return;
    }
    active.last_state = state;
    if (active.callbacks.state_changed) {
        active.callbacks.state_changed(active.ref, state);
    }
}

void CaptureModel::stopCapture(VisualShareRef offer)
{
    if (selection_ && sameRef(selection_->ref, offer)) {
        auto callback = std::move(selection_->callback);
        const VisualShareRef ref = selection_->ref;
        ++selection_->epoch;
        selection_.reset();
        port_->cancelPrompt();
        if (callback) {
            post([callback = std::move(callback), ref] {
                callback(ref, DesktopResult::cancelled);
            });
        }
        return;
    }
    auto active = active_;
    if (!active || !sameRef(active->ref, offer)) {
        return;
    }
    ++active->epoch;
    active->capturing = false;
    auto capture = std::move(active->capture);
    active_.reset();
    if (capture) {
        capture->stop();
    }
}

void CaptureModel::finishSelection(std::uint64_t epoch, DesktopResult result)
{
    if (!selection_ || selection_->epoch != epoch) {
        return;
    }
    auto callback = std::move(selection_->callback);
    const VisualShareRef ref = selection_->ref;
    selection_.reset();
    if (callback) {
        post([callback = std::move(callback), ref, result] { callback(ref, result); });
    }
}

void CaptureModel::post(std::function<void()> task)
{
    if (!runtime_.post) {
        return;
    }
    auto lifetime = lifetime_;
    runtime_.post([lifetime, task = std::move(task)]() mutable {
        if (lifetime->alive.load()) {
            task();
        }
    });
}

} // namespace oneintwo::windows
