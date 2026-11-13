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

// Entradas de diseño. El coordinador suministra la generación vigente al enviar.
// El módulo valida, presenta y consume una sola oferta pendiente por sesión.
class SharedLinks {
public:
    void offerLink(SessionGeneration generation, const std::string& url);
    void receiveLinkOffer(const LinkOffer& offer);
    void openLink(LinkOfferRef offer);
    void dismissLink(LinkOfferRef offer);
};

} // namespace oneintwo
