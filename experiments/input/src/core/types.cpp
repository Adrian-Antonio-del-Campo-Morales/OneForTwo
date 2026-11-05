// Responsabilidad: nombres legibles de estados y motivos para la salida de consola.
#include "types.hpp"

namespace oit {

const char* edgeName(Edge edge) {
  return edge == Edge::Right ? "right" : "left";
}

const char* leaveReasonName(LeaveReason reason) {
  switch (reason) {
    case LeaveReason::Returned: return "retorno";
    case LeaveReason::Emergency: return "salida local";
    case LeaveReason::LocalOverride: return "actividad local en destino";
    case LeaveReason::LinkLost: return "conexión perdida";
    case LeaveReason::Timeout: return "tiempo agotado";
    case LeaveReason::Shutdown: return "cierre";
    case LeaveReason::Stale: return "mensaje obsoleto";
    case LeaveReason::InjectionFailed: return "inyección no disponible";
  }
  return "desconocido";
}

const char* msgTypeName(MsgType type) {
  switch (type) {
    case MsgType::Hello: return "Hello";
    case MsgType::HelloAck: return "HelloAck";
    case MsgType::EnterRequest: return "EnterRequest";
    case MsgType::EnterAck: return "EnterAck";
    case MsgType::EnterReject: return "EnterReject";
    case MsgType::Leave: return "Leave";
    case MsgType::Motion: return "Motion";
    case MsgType::Button: return "Button";
    case MsgType::Wheel: return "Wheel";
    case MsgType::Key: return "Key";
    case MsgType::ReleaseAll: return "ReleaseAll";
    case MsgType::Ping: return "Ping";
    case MsgType::Pong: return "Pong";
  }
  return "?";
}

}  // namespace oit
