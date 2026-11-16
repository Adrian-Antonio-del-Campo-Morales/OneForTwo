// Responsibility: build the offer from authorized routes, apply
// limits and preserve the internal route map relative to absolute route.
// is never transmitted; the receiver only receives relative paths and identifiers.
#pragma once

#include "core/model.hpp"

#include <map>
#include <string>
#include <vector>

namespace o2f {

struct BuiltOffer {
    Offer offer;
    std::map<std::string, std::string> absoluteByRel;
};

BuiltOffer buildOffer(const std::vector<std::string>& rootsUtf8, const Limits& limits);

// Cache status format (one header line and one per element).
std::string serializeManifestText(const Offer& offer);
Offer parseManifestText(const std::string& text, const Limits& limits);

// Summary without absolute paths or file names: suitable for default logs.
std::string describeOffer(const Offer& offer);

}  // namespace o2f
