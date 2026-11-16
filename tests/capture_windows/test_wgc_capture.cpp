#include "platform/windows/visual/wgc_capture_core.hpp"
#include "test_support.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace oneintwo;
using namespace oneintwo::test;

namespace {

namespace wgc = oneintwo::windows::detail;

struct FakeCursorApi {
    std::atomic<int> calls{0};
    bool throw_on_call = false;

    void put_IsCursorCaptureEnabled(bool)
    {
        ++calls;
        if (throw_on_call) {
            throw std::runtime_error("cursor property failed");
        }
    }
};

struct FakeCursorSession {
    FakeCursorApi* api = nullptr;
    bool throw_on_try = false;

    template <typename>
    FakeCursorApi* try_as() const
    {
        if (throw_on_try) {
            throw std::runtime_error("cursor interface lookup failed");
        }
        return api;
    }
};

struct SessionState {
    std::mutex mutex;
    std::condition_variable cv;
    FakeCursorApi cursor;
    bool cursor_available = true;
    bool cursor_throws = false;
    bool cursor_lookup_throws = false;
    int configure_calls = 0;
    int start_calls = 0;
    int close_calls = 0;
    std::vector<int> order;
};

class FakeSession final : public wgc::WgcSession {
public:
    explicit FakeSession(std::shared_ptr<SessionState> state)
        : state_(std::move(state))
    {
    }

    void configureCursor() override
    {
        FakeCursorSession cursor;
        cursor.api = state_->cursor_available ? &state_->cursor : nullptr;
        cursor.throw_on_try = state_->cursor_lookup_throws;
        if (state_->cursor_throws) {
            state_->cursor.throw_on_call = true;
        }
        (void)wgc::tryDisableCursor<FakeCursorSession, FakeCursorApi>(cursor);

        std::lock_guard lock(state_->mutex);
        ++state_->configure_calls;
        state_->order.push_back(1);
        state_->cv.notify_all();
    }

    void start() override
    {
        std::lock_guard lock(state_->mutex);
        ++state_->start_calls;
        state_->order.push_back(2);
        state_->cv.notify_all();
    }

    void close() override
    {
        std::lock_guard lock(state_->mutex);
        ++state_->close_calls;
        state_->cv.notify_all();
    }

private:
    std::shared_ptr<SessionState> state_;
};

struct FrameStats {
    std::atomic<int> copies{0};
    std::atomic<int> closes{0};
};

class FakeFrame final : public wgc::WgcFrame {
public:
    FakeFrame(VisualSize size, std::shared_ptr<FrameStats> stats)
        : size_(size)
        , stats_(std::move(stats))
    {
    }

    VisualSize contentSize() override { return size_; }

    bool copyTo(VisualFrame& out) override
    {
        ++stats_->copies;
        out.size = size_;
        out.bgra.assign(static_cast<std::size_t>(size_.width) * size_.height * 4ull, 0x5A);
        return true;
    }

    void close() override { ++stats_->closes; }

private:
    VisualSize size_{};
    std::shared_ptr<FrameStats> stats_;
};

struct PoolState {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::unique_ptr<FakeFrame>> frames;
    std::shared_ptr<SessionState> session = std::make_shared<SessionState>();
    std::atomic<int> create_session_calls{0};
    std::atomic<int> close_calls{0};
    std::atomic<int> recreate_calls{0};
    bool recreate_ok = true;
    VisualSize last_recreate{};
};

class FakeFramePool final : public wgc::WgcFramePool {
public:
    explicit FakeFramePool(std::shared_ptr<PoolState> state)
        : state_(std::move(state))
    {
    }

    std::unique_ptr<wgc::WgcSession> createSession() override
    {
        ++state_->create_session_calls;
        return std::make_unique<FakeSession>(state_->session);
    }

    std::unique_ptr<wgc::WgcFrame> nextFrame() override
    {
        std::lock_guard lock(state_->mutex);
        if (state_->frames.empty()) {
            return nullptr;
        }
        auto frame = std::move(state_->frames.front());
        state_->frames.pop_front();
        return frame;
    }

    bool recreate(VisualSize size) override
    {
        {
            std::lock_guard lock(state_->mutex);
            state_->last_recreate = size;
        }
        ++state_->recreate_calls;
        state_->cv.notify_all();
        return state_->recreate_ok;
    }

    void close() override
    {
        ++state_->close_calls;
        state_->cv.notify_all();
    }

private:
    std::shared_ptr<PoolState> state_;
};

class FakeFramePoolFactory final : public wgc::WgcFramePoolFactory {
public:
    explicit FakeFramePoolFactory(std::shared_ptr<PoolState> state)
        : state_(std::move(state))
    {
    }

    std::unique_ptr<wgc::WgcFramePool> create(VisualSize initial_size) override
    {
        initial_size_ = initial_size;
        return std::make_unique<FakeFramePool>(state_);
    }

    VisualSize initial_size() const { return initial_size_; }

private:
    std::shared_ptr<PoolState> state_;
    VisualSize initial_size_{};
};

class FakeSource final : public wgc::WgcSource {
public:
    bool available() override { return available_.load(); }
    bool suspended() override { return suspended_.load(); }

    void setAvailable(bool available) { available_.store(available); }
    void setSuspended(bool suspended) { suspended_.store(suspended); }

private:
    std::atomic<bool> available_{true};
    std::atomic<bool> suspended_{false};
};

struct SinkState {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<VisualFrame> frames;
    std::vector<VisualSourceState> states;

    void frame(VisualFrame value)
    {
        std::lock_guard lock(mutex);
        frames.push_back(std::move(value));
        cv.notify_all();
    }

    void state(VisualSourceState value)
    {
        std::lock_guard lock(mutex);
        states.push_back(value);
        cv.notify_all();
    }

    bool waitForFrames(std::size_t count)
    {
        std::unique_lock lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(2),
                           [&] { return frames.size() >= count; });
    }

    std::size_t frameCount()
    {
        std::lock_guard lock(mutex);
        return frames.size();
    }

    std::size_t stateCount()
    {
        std::lock_guard lock(mutex);
        return states.size();
    }
};

struct Fixture {
    std::shared_ptr<PoolState> pool_state = std::make_shared<PoolState>();
    SinkState sink_state;
    std::vector<std::shared_ptr<FrameStats>> frame_stats;
    std::unique_ptr<wgc::WgcCaptureCore> core;

    void start(bool cursor_available, bool cursor_throws = false,
               bool cursor_lookup_throws = false)
    {
        pool_state->session->cursor_available = cursor_available;
        pool_state->session->cursor_throws = cursor_throws;
        pool_state->session->cursor_lookup_throws = cursor_lookup_throws;

        auto factory = std::make_unique<FakeFramePoolFactory>(pool_state);
        auto source = std::make_unique<FakeSource>();
        core = std::make_unique<wgc::WgcCaptureCore>(std::move(factory), std::move(source),
                                                     VisualSize{32, 32});

        windows::NativeCapture::Sink sink;
        sink.frame = [this](VisualFrame value) { sink_state.frame(std::move(value)); };
        sink.state = [this](VisualSourceState value) { sink_state.state(value); };
        core->start(std::move(sink));
    }

    bool waitForStart()
    {
        std::unique_lock lock(pool_state->session->mutex);
        return pool_state->session->cv.wait_for(lock, std::chrono::seconds(2),
                                                [&] {
                                                    return pool_state->session->start_calls > 0;
                                                });
    }

    void pushFrame(VisualSize size)
    {
        auto stats = std::make_shared<FrameStats>();
        frame_stats.push_back(stats);
        auto frame = std::make_unique<FakeFrame>(size, stats);
        std::lock_guard lock(pool_state->mutex);
        pool_state->frames.push_back(std::move(frame));
    }

    bool waitForRecreate()
    {
        std::unique_lock lock(pool_state->mutex);
        return pool_state->cv.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return pool_state->recreate_calls.load() > 0; });
    }
};

void testCursorIsConfiguredBeforeStart()
{
    Fixture fixture;
    fixture.start(true);
    CHECK(fixture.waitForStart());

    {
        std::lock_guard lock(fixture.pool_state->session->mutex);
        CHECK(fixture.pool_state->session->configure_calls == 1);
        CHECK(fixture.pool_state->session->start_calls == 1);
        CHECK(fixture.pool_state->session->order == std::vector<int>({1, 2}));
    }
    CHECK(fixture.pool_state->session->cursor.calls.load() == 1);

    fixture.pushFrame({32, 32});
    CHECK(fixture.sink_state.waitForFrames(1));
    CHECK(fixture.frame_stats.front()->copies.load() == 1);

    fixture.core->stop();
    CHECK(fixture.frame_stats.front()->closes.load() == 1);
    CHECK(fixture.pool_state->session->close_calls == 1);
    CHECK(fixture.pool_state->close_calls.load() == 1);
}

void testMissingCursorInterfaceStillCaptures()
{
    Fixture fixture;
    fixture.start(false);
    CHECK(fixture.waitForStart());
    CHECK(fixture.pool_state->session->cursor.calls.load() == 0);
    CHECK(fixture.pool_state->session->start_calls == 1);

    fixture.pushFrame({32, 32});
    CHECK(fixture.sink_state.waitForFrames(1));
    fixture.core->stop();
}

void testCursorFaultsDoNotAbortCapture()
{
    {
        Fixture fixture;
        fixture.start(true, true);
        CHECK(fixture.waitForStart());
        CHECK(fixture.pool_state->session->cursor.calls.load() == 1);
        CHECK(fixture.pool_state->session->start_calls == 1);
        fixture.pushFrame({32, 32});
        CHECK(fixture.sink_state.waitForFrames(1));
        fixture.core->stop();
    }
    {
        Fixture fixture;
        fixture.start(true, false, true);
        CHECK(fixture.waitForStart());
        CHECK(fixture.pool_state->session->cursor.calls.load() == 0);
        CHECK(fixture.pool_state->session->start_calls == 1);
        fixture.core->stop();
    }
}

void testStopDoesNotDeliverLateCallbacks()
{
    Fixture fixture;
    fixture.start(true);
    CHECK(fixture.waitForStart());
    fixture.pushFrame({32, 32});
    CHECK(fixture.sink_state.waitForFrames(1));

    fixture.core->stop();
    const std::size_t frames_after_stop = fixture.sink_state.frameCount();
    const std::size_t states_after_stop = fixture.sink_state.stateCount();

    fixture.pushFrame({32, 32});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CHECK(fixture.sink_state.frameCount() == frames_after_stop);
    CHECK(fixture.sink_state.stateCount() == states_after_stop);
    CHECK(fixture.frame_stats[1]->copies.load() == 0);
    CHECK(fixture.pool_state->session->close_calls == 1);
    CHECK(fixture.pool_state->close_calls.load() == 1);
}

void testFramePoolRecreationDropsTheTransitionFrame()
{
    Fixture fixture;
    fixture.start(false);
    CHECK(fixture.waitForStart());
    fixture.pushFrame({32, 32});
    CHECK(fixture.sink_state.waitForFrames(1));

    fixture.pushFrame({64, 48});
    CHECK(fixture.waitForRecreate());
    CHECK(fixture.pool_state->recreate_calls.load() == 1);
    {
        std::lock_guard lock(fixture.pool_state->mutex);
        CHECK(fixture.pool_state->last_recreate.width == 64);
        CHECK(fixture.pool_state->last_recreate.height == 48);
    }
    CHECK(fixture.frame_stats[1]->closes.load() == 1);

    fixture.pushFrame({64, 48});
    CHECK(fixture.sink_state.waitForFrames(2));
    fixture.core->stop();
}

} // namespace

int main()
{
    testCursorIsConfiguredBeforeStart();
    testMissingCursorInterfaceStillCaptures();
    testCursorFaultsDoNotAbortCapture();
    testStopDoesNotDeliverLateCallbacks();
    testFramePoolRecreationDropsTheTransitionFrame();

    std::printf("wgc_capture: %d comprobaciones, %d fallos\n", check_count(),
                failure_count());
    return failure_count() == 0 ? 0 : 1;
}
