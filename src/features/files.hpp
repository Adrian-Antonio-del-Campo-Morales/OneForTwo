#pragma once

#include "../core/session.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace oneintwo {

struct DirectFileOfferRef {
    SessionGeneration generation;
    std::uint64_t offer_id;
};

struct DirectFileOffer {
    DirectFileOfferRef ref;
    std::string first_item_name;
    std::uint32_t item_count;
    std::uint64_t total_bytes;
};

// Common module design entries. The session authorizes messages and the
// coordinator publishes the state that the module generates when executing each action.
class FileTransfers {
public:
    void offerDirectFiles(
        SessionGeneration generation, const std::vector<std::filesystem::path>& selected_paths);
    void receiveDirectFileOffer(const DirectFileOffer& offer);
    void acceptDirectFileOffer(
        DirectFileOfferRef offer, const std::filesystem::path& local_destination);
    void declineDirectFileOffer(DirectFileOfferRef offer);
    void cancelDirectFileOffer(DirectFileOfferRef offer);
};

} // namespace oneintwo
