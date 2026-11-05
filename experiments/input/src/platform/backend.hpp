#pragma once
// Responsabilidad: contrato común de los adaptadores de escritorio. El núcleo no
// conoce Windows, X11, hooks ni portales.

#include "core/types.hpp"

#include <memory>
#include <string>

namespace oit {

struct BackendCaps {
  std::string name;
  bool capture = false;
  bool inject = false;
  bool targetLocalOverride = false;
  std::string notes;
};

struct TargetSignal {
  enum class Kind : uint8_t { LocalOverride, Emergency, Stop };
  Kind kind = Kind::Stop;
};

class Backend {
 public:
  virtual ~Backend() = default;

  virtual bool init(std::string& error) = 0;
  virtual Geo geometry() const = 0;
  virtual int monitorCount() const = 0;
  virtual BackendCaps caps() const = 0;
  virtual std::string sessionType() const = 0;

  // Autoriza y prepara el mecanismo del escritorio antes de exigir geometría o
  // red: portal InputCapture (origen) o RemoteDesktop + GlobalShortcuts
  // (destino). Los backends sin autorización previa lo implementan como no-op.
  virtual bool prepareSource(Edge edge, std::string& error) = 0;
  virtual bool prepareTarget(std::string& error) = 0;

  // Origen: observa el escritorio sin bloquear hasta que se active la captura.
  virtual bool startSource(std::string& error) = 0;
  virtual void stopSource() = 0;
  virtual bool setCaptureActive(bool active, std::string& error) = 0;
  virtual bool nextSourceEvent(RawEvent& event, int waitMs) = 0;
  virtual void warpLocalCursor(int x, int y) = 0;
  // Describe un fallo irrecuperable de la captura (por ejemplo, portal cerrado).
  virtual bool sourceFatal(std::string& reason) {
    (void)reason;
    return false;
  }

  // Destino: instala las guardas locales y habilita la inyección.
  virtual bool startTarget(std::string& error) = 0;
  virtual void stopTarget() = 0;
  virtual void setTargetGuardArmed(bool armed) = 0;
  virtual bool nextTargetSignal(TargetSignal& signal, int waitMs) = 0;

  virtual bool injectMove(int x, int y, std::string& error) = 0;
  virtual bool injectKey(uint16_t usage, bool pressed, std::string& error) = 0;
  virtual bool injectButton(uint16_t button, bool pressed, std::string& error) = 0;
  virtual bool injectWheel(int16_t dx, int16_t dy, std::string& error) = 0;
  virtual bool cursorPosition(int& x, int& y) = 0;
};

std::unique_ptr<Backend> makePlatformBackend(std::string& error);

}  // namespace oit
