#pragma once

#include "../core/session.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace oneintwo {

inline constexpr std::size_t max_link_url_bytes = 4096;
inline constexpr auto link_offer_lifetime = std::chrono::seconds{60};

struct LinkOfferRef {
    SessionGeneration generation;
    std::uint64_t offer_id;
};

struct LinkOffer {
    LinkOfferRef ref;
    std::string url;
};

// Design tickets. The coordinator supplies the current generation when sending.
// The module validates, presents and consumes only one outstanding offer per session.
class SharedLinks {
public:
    void offerLink(SessionGeneration generation, const std::string& url);
    void receiveLinkOffer(const LinkOffer& offer);
    void openLink(LinkOfferRef offer);
    void dismissLink(LinkOfferRef offer);
};

} // namespace oneintwo
