// Responsabilidad: respuesta explícita cuando la plataforma no tiene adaptador
// nativo compilado. Nunca simula una publicación correcta.
#include "platform/clipboard.hpp"

namespace o2f {

bool clipboardAvailable(std::string& why) {
    why = "esta plataforma no tiene adaptador de portapapeles en el prototipo";
    return false;
}

bool onDemandClipboardAvailable(std::string& why) {
    why = "esta plataforma no tiene adaptador de portapapeles en el prototipo";
    return false;
}

int runClipboardPublisher(const ClipboardRequest&, bool, std::string& err) {
    err = "esta plataforma no tiene adaptador de portapapeles en el prototipo";
    return 2;
}

}  // namespace o2f
