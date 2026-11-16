// Responsibility: file clipboard adapter contract.
// native publication is executed only in explicit manual modes.
#pragma once

#include "core/model.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace o2f {

// Source of content for delivery on demand.
class ContentSource {
public:
    virtual ~ContentSource() = default;
    // Returns false and fill err if the content cannot be delivered.
    virtual bool read(const Entry& entry, std::uint64_t offset, void* buffer, std::size_t len,
                      std::size_t& got, std::string& err) = 0;
};

struct ClipboardRequest {
    // Cache mode: absolute local routes already verified.
    std::vector<std::string> localPaths;
    // Mode on demand: manifest and authorised identifiers.
    Offer offer;
    std::shared_ptr<ContentSource> source;
};

bool clipboardAvailable(std::string& why);
bool onDemandClipboardAvailable(std::string& why);

// Publish the offer and lock to Ctrl+C. Returns 0 if finished correctly.
// flushOnExit=true is only valid when the content is already on local disk.
int runClipboardPublisher(const ClipboardRequest& request, bool flushOnExit, std::string& err);

}  // namespace o2f
