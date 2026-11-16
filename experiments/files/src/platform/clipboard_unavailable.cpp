// Responsibility: explicit response when the platform has no adapter
// Native compiled. Never simulates a correct publication.
#include "platform/clipboard.hpp"

namespace o2f {

bool clipboardAvailable(std::string& why) {
    why = "this platform has no clipboard adapter in the prototype";
    return false;
}

bool onDemandClipboardAvailable(std::string& why) {
    why = "this platform has no clipboard adapter in the prototype";
    return false;
}

int runClipboardPublisher(const ClipboardRequest&, bool, std::string& err) {
    err = "this platform has no clipboard adapter in the prototype";
    return 2;
}

}  // namespace o2f
