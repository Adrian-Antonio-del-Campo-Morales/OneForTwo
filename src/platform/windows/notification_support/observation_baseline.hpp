#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <set>

namespace oneintwo::windows_notifications {

// Separates the initial loading of the notification center from new notices.
// By opening a subscription you will be seeded with native identifiers already
// existing; only subsequent additions are delivered. A new baseline
// (reconnection, suspension output) is taken by restarting the instance.
class ObservationBaseline {
public:
    explicit ObservationBaseline(std::size_t recent_capacity = 4096)
        : recent_capacity_(recent_capacity) {}

    void seed(const std::set<std::uint32_t>& existing_ids) {
        for (const std::uint32_t identifier : existing_ids) {
            baseline_.insert(identifier);
        }
    }

    void seed(std::initializer_list<std::uint32_t> existing_ids) {
        baseline_.insert(existing_ids.begin(), existing_ids.end());
    }

    // Returns true only the first time a new identifier appears.
    bool accept(std::uint32_t native_id) {
        if (baseline_.erase(native_id) > 0) {
            // It was a previous existence, not a new incorporation.
            remember(native_id);
            return false;
        }
        if (seen_.find(native_id) != seen_.end()) {
            return false;
        }
        remember(native_id);
        return true;
    }

    void clear() {
        baseline_.clear();
        seen_.clear();
        recent_.clear();
    }

    std::size_t baseline_size() const { return baseline_.size(); }
    std::size_t seen_size() const { return seen_.size(); }

private:
    void remember(std::uint32_t native_id) {
        seen_.insert(native_id);
        recent_.push_back(native_id);
        while (recent_.size() > recent_capacity_) {
            seen_.erase(recent_.front());
            recent_.pop_front();
        }
    }

    std::size_t recent_capacity_;
    std::set<std::uint32_t> baseline_;
    std::deque<std::uint32_t> recent_;
    std::set<std::uint32_t> seen_;
};

} // namespace oneintwo::windows_notifications
