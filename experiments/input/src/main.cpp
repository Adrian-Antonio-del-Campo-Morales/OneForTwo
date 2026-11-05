// Responsabilidad: modos explícitos del prototipo (ayuda, self-test,
// diagnóstico, demostración de inyección, origen y destino) y sus bucles de
// sesión. Nada se ejecuta sin un modo explícito.
#include "core/clock.hpp"
#include "core/keymap.hpp"
#include "core/source_controller.hpp"
#include "core/target_controller.hpp"
#include "net/link.hpp"
#include "platform/backend.hpp"
#include "self_test.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

namespace {

#ifdef _WIN32
volatile LONG g_interrupted = 0;

BOOL WINAPI consoleHandler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
    InterlockedExchange(&g_interrupted, 1);
    return TRUE;
  }
  return FALSE;
}

bool interrupted() { return InterlockedCompareExchange(&g_interrupted, 0, 0) != 0; }
#else
volatile sig_atomic_t g_interrupted = 0;

void signalHandler(int) { g_interrupted = 1; }

bool interrupted() { return g_interrupted != 0; }
#endif

void installSignalHandlers() {
#ifdef _WIN32
  SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
#endif
}

enum class Mode { Help, SelfTest, Diagnose, InjectDemo, Source, Target };

struct Options {
  Mode mode = Mode::Help;
  std::string host;
  uint16_t port = 0;
  std::string certFile;
  std::string keyFile;
  std::string fingerprint;
  oit::Edge edge = oit::Edge::Right;
  int armSeconds = 5;
  int maxSeconds = 180;
};

void printHelp() {
  std::printf(
      "OneInTwo - experimento de teclado y raton\n"
      "\n"
      "Uso: input_experiment <modo> [opciones]\n"
      "\n"
      "Modos (uno solo):\n"
      "  --self-test          Pruebas aisladas del nucleo; no toca el escritorio ni la red.\n"
      "  --diagnose           Inventario de plataforma y capacidades; no instala hooks.\n"
      "  --inject-demo        Inyecta un movimiento breve y escribe 'oneintwo' en la ventana\n"
      "                       enfocada. Solo manual: enfoque un editor desechable.\n"
      "  --source             Origen de teclado y raton (captura e envia).\n"
      "  --target             Destino (escucha y inyecta).\n"
      "  --help               Muestra esta ayuda.\n"
      "\n"
      "Opciones de red (origen y destino, TLS obligatorio):\n"
      "  --connect HOST:PORT       Direccion del destino (solo origen).\n"
      "  --listen PORT             Puerto de escucha local (solo destino).\n"
      "  --cert FILE               Certificado PEM propio.\n"
      "  --key FILE                Clave privada PEM propia.\n"
      "  --peer-fingerprint HEX    Huella SHA-256 del certificado del otro equipo.\n"
      "\n"
      "Opciones de control:\n"
      "  --edge right|left         Borde compartido del origen (por defecto right).\n"
      "  --arm-seconds N           Preparacion visible antes de capturar (1..60, def. 5).\n"
      "  --max-seconds N           Duracion maxima de la sesion (10..3600, def. 180).\n"
      "\n"
      "Salida local de emergencia durante el control remoto: Ctrl+Alt+F12.\n"
      "El destino tambien libera al recibir ese atajo o actividad fisica local (Windows).\n"
      "\n"
      "Wayland (Linux): usa los portales InputCapture y RemoteDesktop + libei.\n"
      "El escritorio debe autorizar la captura y Ctrl+Alt+F12. Sin esa salida local\n"
      "el destino no inicia la inyeccion. Requiere gio-2.0 y libei-1.0 al compilar.\n"
      "\n"
      "Ejemplos:\n"
      "  input_experiment --diagnose\n"
      "  input_experiment --inject-demo --arm-seconds 5\n"
      "  input_experiment --target --listen 47830 --cert certificates/win.crt \\\n"
      "      --key certificates/win.key --peer-fingerprint AA:BB:...\n"
      "  input_experiment --source --connect 192.168.1.20:47830 --edge right \\\n"
      "      --cert certificates/linux.crt --key certificates/linux.key \\\n"
      "      --peer-fingerprint CC:DD:... --arm-seconds 10 --max-seconds 180\n");
  std::fflush(stdout);
}

bool parseInt(const std::string& text, long minimum, long maximum, int& value) {
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || parsed < minimum || parsed > maximum) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

bool parsePort(const std::string& text, uint16_t& port) {
  int value = 0;
  if (!parseInt(text, 1, 65535, value)) return false;
  port = static_cast<uint16_t>(value);
  return true;
}

bool parseHostPort(const std::string& text, std::string& host, uint16_t& port) {
  const size_t separator = text.rfind(':');
  if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) {
    return false;
  }
  host = text.substr(0, separator);
  return parsePort(text.substr(separator + 1), port);
}

bool parseArgs(int argc, char** argv, Options& options, std::string& error) {
  auto setMode = [&](Mode mode) {
    if (options.mode != Mode::Help) {
      error = "solo se permite un modo por ejecucion";
      return false;
    }
    options.mode = mode;
    return true;
  };
  auto valueAt = [&](int& index, std::string& value) {
    if (index + 1 >= argc) {
      error = std::string("falta el valor de ") + argv[index];
      return false;
    }
    value = argv[++index];
    return true;
  };

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help" || argument == "-h") {
      if (!setMode(Mode::Help)) return false;
    } else if (argument == "--self-test") {
      if (!setMode(Mode::SelfTest)) return false;
    } else if (argument == "--diagnose") {
      if (!setMode(Mode::Diagnose)) return false;
    } else if (argument == "--inject-demo") {
      if (!setMode(Mode::InjectDemo)) return false;
    } else if (argument == "--source") {
      if (!setMode(Mode::Source)) return false;
    } else if (argument == "--target") {
      if (!setMode(Mode::Target)) return false;
    } else if (argument == "--connect") {
      std::string value;
      if (!valueAt(index, value)) return false;
      if (!parseHostPort(value, options.host, options.port)) {
        error = "formato esperado para --connect: HOST:PORT";
        return false;
      }
    } else if (argument == "--listen") {
      std::string value;
      if (!valueAt(index, value)) return false;
      if (!parsePort(value, options.port)) {
        error = "puerto de escucha invalido";
        return false;
      }
    } else if (argument == "--cert") {
      if (!valueAt(index, options.certFile)) return false;
    } else if (argument == "--key") {
      if (!valueAt(index, options.keyFile)) return false;
    } else if (argument == "--peer-fingerprint") {
      if (!valueAt(index, options.fingerprint)) return false;
    } else if (argument == "--edge") {
      std::string value;
      if (!valueAt(index, value)) return false;
      if (value == "right") {
        options.edge = oit::Edge::Right;
      } else if (value == "left") {
        options.edge = oit::Edge::Left;
      } else {
        error = "--edge debe ser right o left";
        return false;
      }
    } else if (argument == "--arm-seconds") {
      std::string value;
      if (!valueAt(index, value)) return false;
      if (!parseInt(value, 1, 60, options.armSeconds)) {
        error = "--arm-seconds debe estar entre 1 y 60";
        return false;
      }
    } else if (argument == "--max-seconds") {
      std::string value;
      if (!valueAt(index, value)) return false;
      if (!parseInt(value, 10, 3600, options.maxSeconds)) {
        error = "--max-seconds debe estar entre 10 y 3600";
        return false;
      }
    } else {
      error = "opcion desconocida: " + argument;
      return false;
    }
  }

  if (options.mode == Mode::Source || options.mode == Mode::Target) {
    if (options.certFile.empty() || options.keyFile.empty() || options.fingerprint.empty()) {
      error = "faltan --cert, --key o --peer-fingerprint";
      return false;
    }
    if (options.mode == Mode::Source && (options.host.empty() || options.port == 0)) {
      error = "el origen requiere --connect HOST:PORT";
      return false;
    }
    if (options.mode == Mode::Target && options.port == 0) {
      error = "el destino requiere --listen PORT";
      return false;
    }
  }
  return true;
}

const char* sourceStateText(oit::SourceState state) {
  switch (state) {
    case oit::SourceState::Idle: return "control local, observando el borde";
    case oit::SourceState::Preparing: return "esperando aceptacion del destino";
    case oit::SourceState::Remote: return "control remoto activo";
    case oit::SourceState::Stopped: return "detenido";
  }
  return "?";
}

const char* targetStateText(oit::TargetState state) {
  switch (state) {
    case oit::TargetState::Idle: return "en espera de control";
    case oit::TargetState::Remote: return "inyectando entrada remota";
    case oit::TargetState::Stopped: return "detenido";
  }
  return "?";
}

void printBackend(const oit::Backend& backend) {
  const oit::BackendCaps caps = backend.caps();
  const oit::Geo geo = backend.geometry();
  std::printf("Plataforma: %s\n", caps.name.c_str());
  std::printf("Sesion: %s\n", backend.sessionType().c_str());
  std::printf("Monitores: %d\n", backend.monitorCount());
  if (geo.valid()) {
    std::printf("Escritorio: (%d,%d) %dx%d\n", geo.x, geo.y, geo.w, geo.h);
  } else {
    std::printf("Escritorio: no disponible\n");
  }
  std::printf("Captura declarada: %s\n", caps.capture ? "si" : "no");
  std::printf("Inyeccion declarada: %s\n", caps.inject ? "si" : "no");
  std::printf("Actividad local del destino: %s\n", caps.targetLocalOverride ? "si" : "no");
  std::printf("Notas: %s\n", caps.notes.c_str());
  std::fflush(stdout);
}

bool countdown(int seconds, const char* message) {
  for (int left = seconds; left > 0; --left) {
    if (interrupted()) return false;
    std::printf("%s %d s...\n", message, left);
    std::fflush(stdout);
    for (int slice = 0; slice < 10; ++slice) {
      if (interrupted()) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  return !interrupted();
}

bool exchangeHello(oit::Link& link, bool source, std::string& error) {
  if (source) {
    oit::Message hello;
    hello.type = oit::MsgType::Hello;
    hello.code = oit::kRoleSource;
    if (!link.send(hello, error)) return false;

    const int64_t deadline = oit::steadyNowMs() + 5000;
    while (oit::steadyNowMs() < deadline) {
      std::vector<oit::Message> messages;
      if (!link.receive(messages, 100, error)) return false;
      for (const oit::Message& message : messages) {
        if (message.type == oit::MsgType::HelloAck && message.code == oit::kRoleTarget) return true;
      }
    }
    error = "el destino no respondio al saludo";
    return false;
  }

  const int64_t deadline = oit::steadyNowMs() + 8000;
  while (oit::steadyNowMs() < deadline) {
    std::vector<oit::Message> messages;
    if (!link.receive(messages, 100, error)) return false;
    for (const oit::Message& message : messages) {
      if (message.type == oit::MsgType::Hello && message.code == oit::kRoleSource) {
        oit::Message ack;
        ack.type = oit::MsgType::HelloAck;
        ack.code = oit::kRoleTarget;
        return link.send(ack, error);
      }
    }
  }
  error = "el origen no envió el saludo";
  return false;
}

class SourceIOImpl : public oit::SourceIO {
 public:
  SourceIOImpl(oit::Link& link, oit::Backend& backend) : link_(link), backend_(backend) {}

  bool send(const oit::Message& message) override {
    if (!link_.send(message, lastError_)) return false;
    return true;
  }
  bool startObserving() override { return backend_.startSource(lastError_); }
  bool setCaptureActive(bool active) override { return backend_.setCaptureActive(active, lastError_); }
  void stopObserving() override { backend_.stopSource(); }
  void warpCursor(int x, int y) override { backend_.warpLocalCursor(x, y); }
  void requestStop(const std::string&) override {}
  const std::string& lastError() const { return lastError_; }

 private:
  oit::Link& link_;
  oit::Backend& backend_;
  std::string lastError_;
};

class TargetIOImpl : public oit::TargetIO {
 public:
  TargetIOImpl(oit::Link& link, oit::Backend& backend) : link_(link), backend_(backend) {}

  bool send(const oit::Message& message) override { return link_.send(message, lastError_); }

  bool injectMove(int x, int y) override {
    return report(backend_.injectMove(x, y, lastError_));
  }
  bool injectKey(uint16_t usage, bool pressed) override {
    return report(backend_.injectKey(usage, pressed, lastError_));
  }
  bool injectButton(uint16_t button, bool pressed) override {
    return report(backend_.injectButton(button, pressed, lastError_));
  }
  bool injectWheel(int16_t dx, int16_t dy) override {
    return report(backend_.injectWheel(dx, dy, lastError_));
  }
  bool cursorPosition(int& x, int& y) override { return backend_.cursorPosition(x, y); }
  void requestStop(const std::string&) override {}

 private:
  bool report(bool ok) {
    if (!ok && !warned_) {
      std::printf("[destino] Aviso de inyeccion: %s\n", lastError_.c_str());
      std::fflush(stdout);
      warned_ = true;
    }
    return ok;
  }

  oit::Link& link_;
  oit::Backend& backend_;
  std::string lastError_;
  bool warned_ = false;
};

int runDiagnose(oit::Backend& backend) {
  printBackend(backend);
  const oit::BackendCaps caps = backend.caps();
  if (!caps.capture || !caps.inject) {
    std::printf(
        "Este entorno no declara todas las capacidades. No se habilitara el control real.\n");
  }
  return 0;
}

int runInjectDemo(oit::Backend& backend, const Options& options) {
  if (!backend.caps().inject) {
    std::printf("La inyeccion de entrada no esta disponible en este entorno.\n");
    return 1;
  }
  std::string error;
  if (!backend.prepareTarget(error)) {
    std::printf("No se pudo preparar la inyeccion: %s\n", error.c_str());
    return 1;
  }
  const oit::Geo geo = backend.geometry();
  if (!geo.valid()) {
    std::printf("No hay geometria de escritorio disponible.\n");
    backend.stopTarget();
    return 1;
  }

  std::printf(
      "Demostracion de inyeccion: se movera el cursor unos pixeles y se escribira\n"
      "'oneintwo'. No se pulsan botones. Enfoque una ventana de texto desechable.\n");
  if (!countdown(options.armSeconds, "Comienzo en")) {
    std::printf("Cancelado.\n");
    backend.stopTarget();
    return 0;
  }

  int baseX = geo.x + geo.w / 2;
  int baseY = geo.y + geo.h / 2;
  backend.cursorPosition(baseX, baseY);

  const int offsets[][2] = {{0, 0}, {12, 0}, {-12, 0}, {0, 0}};
  for (const auto& offset : offsets) {
    if (interrupted()) break;
    backend.injectMove(baseX + offset[0], baseY + offset[1], error);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
  }

  const uint16_t keys[] = {oit::hid::O, oit::hid::N, oit::hid::E, oit::hid::I,
                           oit::hid::N, oit::hid::T, oit::hid::W, oit::hid::O};
  for (uint16_t usage : keys) {
    if (interrupted()) break;
    if (!backend.injectKey(usage, true, error)) {
      std::printf("Fallo al inyectar tecla: %s\n", error.c_str());
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    backend.injectKey(usage, false, error);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  backend.injectMove(baseX, baseY, error);
  backend.stopTarget();
  std::printf("Demostracion finalizada. Revise el texto y el cursor.\n");
  return 0;
}

int runSource(oit::Backend& backend, const Options& options) {
  if (!backend.caps().capture) {
    printBackend(backend);
    std::printf("La captura de entrada no esta disponible en este entorno.\n");
    return 1;
  }

  // La autorización del portal precede a la red y a la geometría: en Wayland la
  // zona del escritorio solo se conoce después de Start.
  std::string error;
  if (!backend.prepareSource(options.edge, error)) {
    std::printf("No se pudo preparar la captura del escritorio: %s\n", error.c_str());
    return 1;
  }
  printBackend(backend);
  const oit::Geo geo = backend.geometry();
  if (!geo.valid()) {
    std::printf("El origen no tiene geometria de escritorio disponible.\n");
    backend.stopSource();
    return 1;
  }

  if (!oit::netInit(error)) {
    std::printf("Error de red: %s\n", error.c_str());
    backend.stopSource();
    return 1;
  }
  std::unique_ptr<oit::Link> link = oit::openTlsClient(
      options.host, options.port, options.certFile, options.keyFile, options.fingerprint, error);
  if (!link) {
    std::printf("No se pudo abrir el canal TLS: %s\n", error.c_str());
    backend.stopSource();
    oit::netShutdown();
    return 1;
  }
  if (!exchangeHello(*link, true, error)) {
    std::printf("Fallo el saludo: %s\n", error.c_str());
    link->close();
    backend.stopSource();
    oit::netShutdown();
    return 1;
  }

  std::printf(
      "Conectado al destino. Enfoque en el destino la ventana de prueba.\n"
      "El control se armara tras la preparacion. Ctrl+Alt+F12 devuelve el control local.\n");
  if (!countdown(options.armSeconds, "Preparacion")) {
    std::printf("Cancelado antes de capturar.\n");
    link->close();
    backend.stopSource();
    oit::netShutdown();
    return 0;
  }

  oit::SourceTuning tuning;
  tuning.maxSessionMs = options.maxSeconds * 1000;
  SourceIOImpl io(*link, backend);
  oit::SourceController controller(geo, options.edge, io, tuning, oit::steadyNowMs());
  if (!controller.start()) {
    std::printf("No se pudo iniciar la observacion: %s\n", io.lastError().c_str());
    link->close();
    backend.stopSource();
    oit::netShutdown();
    return 1;
  }

  std::printf("[origen] Armado. Borde compartido: %s. Duracion maxima: %d s.\n",
              oit::edgeName(options.edge), options.maxSeconds);
  std::fflush(stdout);

  const int64_t startMs = oit::steadyNowMs();
  oit::SourceState lastState = controller.state();
  while (!controller.stopRequested() && !interrupted()) {
    const int64_t now = oit::steadyNowMs();
    std::string fatal;
    if (backend.sourceFatal(fatal)) {
      controller.onBackendFailure(fatal, now);
      break;
    }
    oit::RawEvent event;
    while (backend.nextSourceEvent(event, 0)) controller.onRaw(event, now);

    std::vector<oit::Message> messages;
    if (!link->receive(messages, 2, error)) {
      std::printf("[origen] Error de enlace: %s\n", error.c_str());
      controller.onLinkLost(now);
      break;
    }
    for (const oit::Message& message : messages) controller.onMessage(message, now);
    controller.tick(now);

    if (controller.state() != lastState) {
      lastState = controller.state();
      std::printf("[origen %5.1fs] %s\n", (now - startMs) / 1000.0,
                  sourceStateText(lastState));
      std::fflush(stdout);
    }
  }

  if (!controller.stopRequested()) controller.stop(oit::steadyNowMs());
  backend.stopSource();
  link->close();
  oit::netShutdown();

  if (!controller.stopReason().empty()) {
    std::printf("[origen] Fin: %s\n", controller.stopReason().c_str());
  }
  return 0;
}

int runTarget(oit::Backend& backend, const Options& options) {
  if (!backend.caps().inject) {
    printBackend(backend);
    std::printf("La inyeccion de entrada no esta disponible en este entorno.\n");
    return 1;
  }

  // Autoriza RemoteDesktop y Ctrl+Alt+F12 antes de escuchar: sin esa salida
  // local no se habilita ninguna inyección.
  std::string error;
  if (!backend.prepareTarget(error)) {
    std::printf("No se pudo preparar el destino: %s\n", error.c_str());
    return 1;
  }
  printBackend(backend);
  const oit::Geo geo = backend.geometry();
  if (!geo.valid()) {
    std::printf("El destino no tiene geometria de escritorio disponible.\n");
    backend.stopTarget();
    return 1;
  }

  if (!oit::netInit(error)) {
    std::printf("Error de red: %s\n", error.c_str());
    backend.stopTarget();
    return 1;
  }
  std::printf("Escuchando TLS en el puerto %u. Ctrl+C cancela antes de conectar.\n",
              static_cast<unsigned>(options.port));
  std::fflush(stdout);
  std::unique_ptr<oit::Link> link = oit::openTlsServer(
      options.port, options.certFile, options.keyFile, options.fingerprint, &interrupted, error);
  if (!link) {
    std::printf("No se pudo abrir el canal TLS: %s\n", error.c_str());
    backend.stopTarget();
    oit::netShutdown();
    return interrupted() ? 0 : 1;
  }
  if (!exchangeHello(*link, false, error)) {
    std::printf("Fallo el saludo: %s\n", error.c_str());
    link->close();
    backend.stopTarget();
    oit::netShutdown();
    return 1;
  }
  if (!backend.startTarget(error)) {
    std::printf("No se pudo habilitar la inyeccion: %s\n", error.c_str());
    link->close();
    backend.stopTarget();
    oit::netShutdown();
    return 1;
  }

  oit::TargetTuning tuning;
  tuning.maxSessionMs = options.maxSeconds * 1000;
  TargetIOImpl io(*link, backend);
  oit::TargetController controller(geo, io, tuning, oit::steadyNowMs());

  std::printf(
      "[destino] Conexion aceptada. Ctrl+Alt+F12 libera el control localmente.\n");
  std::fflush(stdout);

  const int64_t startMs = oit::steadyNowMs();
  oit::TargetState lastState = controller.state();
  while (!controller.stopRequested() && !interrupted()) {
    const int64_t now = oit::steadyNowMs();

    oit::TargetSignal signal;
    while (backend.nextTargetSignal(signal, 0)) {
      if (signal.kind == oit::TargetSignal::Kind::LocalOverride) {
        controller.onLocalOverride(now);
      } else if (signal.kind == oit::TargetSignal::Kind::Emergency) {
        controller.onEmergency(now);
      }
    }
    if (controller.stopRequested()) break;

    std::vector<oit::Message> messages;
    if (!link->receive(messages, 2, error)) {
      std::printf("[destino] Error de enlace: %s\n", error.c_str());
      controller.onLinkLost(now);
      break;
    }
    for (const oit::Message& message : messages) controller.onMessage(message, now);
    controller.tick(now);
    backend.setTargetGuardArmed(controller.state() == oit::TargetState::Remote);

    if (controller.state() != lastState) {
      lastState = controller.state();
      std::printf("[destino %5.1fs] %s\n", (now - startMs) / 1000.0,
                  targetStateText(lastState));
      std::fflush(stdout);
    }
  }

  if (!controller.stopRequested()) controller.stop(oit::steadyNowMs());
  backend.stopTarget();
  link->close();
  oit::netShutdown();

  if (!controller.stopReason().empty()) {
    std::printf("[destino] Fin: %s\n", controller.stopReason().c_str());
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  installSignalHandlers();

  Options options;
  std::string error;
  if (!parseArgs(argc, argv, options, error)) {
    std::printf("Error: %s\n\n", error.c_str());
    printHelp();
    return 2;
  }

  if (options.mode == Mode::Help) {
    printHelp();
    return 0;
  }
  if (options.mode == Mode::SelfTest) {
    return oit::runSelfTests() == 0 ? 0 : 1;
  }

  std::unique_ptr<oit::Backend> backend = oit::makePlatformBackend(error);
  if (!backend) {
    std::printf("Error creando el backend: %s\n", error.c_str());
    return 1;
  }
  if (!backend->init(error)) {
    std::printf("Error inicializando el backend: %s\n", error.c_str());
    return 1;
  }

  switch (options.mode) {
    case Mode::Diagnose:
      return runDiagnose(*backend);
    case Mode::InjectDemo:
      return runInjectDemo(*backend, options);
    case Mode::Source:
      return runSource(*backend, options);
    case Mode::Target:
      return runTarget(*backend, options);
    case Mode::SelfTest:
    case Mode::Help:
      break;
  }
  return 0;
}
