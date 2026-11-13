#pragma once

#include "platform/windows/visual/win32_capture_port.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace oneintwo::test {

// Estadísticas con vida propia: el adaptador destruye el objeto de captura al
// cerrar, y la prueba sigue necesitando observar su parada.
struct FakeCaptureStats {
    bool started = false;
    bool stopped = false;
    int starts = 0;
};

class FakeCapture final : public windows::NativeCapture {
public:
    explicit FakeCapture(std::shared_ptr<FakeCaptureStats> statistics)
        : stats(std::move(statistics))
    {
    }

    Sink sink;
    std::shared_ptr<FakeCaptureStats> stats;

    void start(Sink new_sink) override
    {
        ++stats->starts;
        stats->started = true;
        sink = std::move(new_sink);
    }

    void stop() override { stats->stopped = true; }

    void pushFrame(VisualFrame frame)
    {
        if (sink.frame) {
            sink.frame(std::move(frame));
        }
    }

    void pushState(VisualSourceState state)
    {
        if (sink.state) {
            sink.state(state);
        }
    }
};

class FakeCapturePort final : public windows::WindowsCapturePort {
public:
    enum class PromptMode {
        immediate_first,
        immediate_second,
        deferred,
        cancel,
    };

    VisualCaptureCapabilities caps{true, true};
    std::vector<windows::NativeSource> monitors;
    std::vector<windows::NativeSource> windows;
    bool adopt_succeeds = true;
    PromptMode prompt_mode = PromptMode::immediate_first;

    int prompt_calls = 0;
    int cancel_prompt_calls = 0;
    VisualSourceKind last_prompt_kind = VisualSourceKind::monitor;
    std::vector<windows::NativeSource> last_prompt_sources;
    std::function<void(std::optional<windows::NativeSource>)> pending_prompt;

    std::vector<std::uint64_t> adopted_handles;
    std::vector<VisualSourceKind> adopted_kinds;
    FakeCapture* last_capture = nullptr;
    std::shared_ptr<FakeCaptureStats> last_stats;

    VisualCaptureCapabilities capabilities() override { return caps; }

    std::vector<windows::NativeSource> enumerate(VisualSourceKind kind) override
    {
        return kind == VisualSourceKind::monitor ? monitors : windows;
    }

    void prompt(VisualSourceKind kind, const std::vector<windows::NativeSource>& sources,
                std::function<void(std::optional<windows::NativeSource>)> complete) override
    {
        ++prompt_calls;
        last_prompt_kind = kind;
        last_prompt_sources = sources;
        switch (prompt_mode) {
        case PromptMode::immediate_first:
            if (complete) {
                complete(sources.empty() ? std::nullopt
                                         : std::optional<windows::NativeSource>(sources.front()));
            }
            break;
        case PromptMode::immediate_second:
            if (complete) {
                complete(sources.size() > 1
                        ? std::optional<windows::NativeSource>(sources[1])
                        : std::optional<windows::NativeSource>{});
            }
            break;
        case PromptMode::deferred:
            pending_prompt = std::move(complete);
            break;
        case PromptMode::cancel:
            if (complete) {
                complete(std::nullopt);
            }
            break;
        }
    }

    void cancelPrompt() override
    {
        ++cancel_prompt_calls;
        auto callback = std::move(pending_prompt);
        pending_prompt = nullptr;
        if (callback) {
            callback(std::nullopt);
        }
    }

    void completePrompt(std::size_t index)
    {
        auto callback = std::move(pending_prompt);
        pending_prompt = nullptr;
        if (callback) {
            callback(index < last_prompt_sources.size()
                    ? std::optional<windows::NativeSource>(last_prompt_sources[index])
                    : std::optional<windows::NativeSource>{});
        }
    }

    std::unique_ptr<windows::NativeCapture> adopt(const windows::NativeSource& source,
                                                  VisualSourceKind kind) override
    {
        adopted_handles.push_back(source.handle);
        adopted_kinds.push_back(kind);
        if (!adopt_succeeds) {
            return nullptr;
        }
        last_stats = std::make_shared<FakeCaptureStats>();
        auto capture = std::make_unique<FakeCapture>(last_stats);
        last_capture = capture.get();
        return capture;
    }
};

} // namespace oneintwo::test
