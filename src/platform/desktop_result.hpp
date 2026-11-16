#pragma once

namespace oneintwo {

// Resultado común de una operación que depende del escritorio. Se mantiene en
// una cabecera propia para que las consultas del módulo de notificaciones
// puedan declararlo sin incluir todo el contrato de escritorio.
enum class DesktopResult {
    ready,
    cancelled,
    denied,
    unavailable,
    error,
};

} // namespace oneintwo
