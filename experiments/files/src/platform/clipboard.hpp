// Responsabilidad: contrato del adaptador de portapapeles de archivos. La
// publicación nativa se ejecuta solo en modos manuales explícitos.
#pragma once

#include "core/model.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace o2f {

// Fuente de contenido para la entrega bajo demanda.
class ContentSource {
public:
    virtual ~ContentSource() = default;
    // Devuelve false y rellena err si el contenido no puede entregarse.
    virtual bool read(const Entry& entry, std::uint64_t offset, void* buffer, std::size_t len,
                      std::size_t& got, std::string& err) = 0;
};

struct ClipboardRequest {
    // Modo caché: rutas locales absolutas ya verificadas.
    std::vector<std::string> localPaths;
    // Modo bajo demanda: manifiesto e identificadores autorizados.
    Offer offer;
    std::shared_ptr<ContentSource> source;
};

bool clipboardAvailable(std::string& why);
bool onDemandClipboardAvailable(std::string& why);

// Publica la oferta y bloquea hasta Ctrl+C. Devuelve 0 si termina correctamente.
// flushOnExit=true solo es válido cuando el contenido ya está en disco local.
int runClipboardPublisher(const ClipboardRequest& request, bool flushOnExit, std::string& err);

}  // namespace o2f
