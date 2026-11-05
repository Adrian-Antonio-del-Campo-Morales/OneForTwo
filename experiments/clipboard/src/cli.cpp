// Responsabilidad: interfaz de consola explícita. Ningún modo se ejecuta sin
// argumentos; las auto-pruebas y el inventario no tocan el portapapeles.
// Sólo los modos documentados leen, escriben u observan el portapapeles real.

#include "cli.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "model.hpp"
#include "net/peer.hpp"
#include "platform/clipboard.hpp"

namespace oit {
namespace {

enum class Mode {
  Help,
  Version,
  SelfTest,
  Inventory,
  ReadText,
  PublishText,
  PublishFile,
  Watch,
  EmitSamples,
  Serve,
  Connect,
};

struct Options {
  Mode mode = Mode::Help;
  bool mode_set = false;
  std::string value;
  std::string path;
  std::string host;
  std::string bind_address = "0.0.0.0";
  std::string cert;
  std::string key;
  std::string peer_cert;
  int port = 0;
  bool show_content = false;
};

const char* yes_no(bool value) { return value ? "yes" : "no"; }

std::string join_path(const std::string& directory,
                      const std::string& name) {
  if (directory.empty()) {
    return name;
  }
  const char last = directory.back();
  if (last == '/' || last == '\\') {
    return directory + name;
  }
  return directory + "/" + name;
}

bool read_utf8_file(const std::string& path, std::string& out,
                    std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open file: " + path;
    return false;
  }
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) {
    error = "cannot determine file size: " + path;
    return false;
  }
  if (static_cast<std::uint64_t>(size) > kMaxTextBytes) {
    error = "file is larger than the text limit: " + path;
    return false;
  }
  out.resize(static_cast<std::size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!out.empty()) {
    file.read(out.data(), static_cast<std::streamsize>(out.size()));
  }
  if (!file && !file.eof()) {
    error = "cannot read file: " + path;
    return false;
  }
  return true;
}

bool file_exists(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return static_cast<bool>(file);
}

void print_content(const std::string& text) {
  std::cout << "--- content begin (" << text.size() << " bytes) ---\n";
  std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (text.empty() || text.back() != '\n') {
    std::cout << '\n';
  }
  std::cout << "--- content end ---\n" << std::flush;
}

void print_metrics(const Validation& validation, const std::string& marker) {
  std::cout << "bytes=" << validation.metrics.bytes
            << " codepoints=" << validation.metrics.codepoints
            << " lines=" << validation.metrics.lines
            << " fnv1a64="
            << fingerprint_hex(fingerprint(validation.normalized))
            << " marker=" << (marker.empty() ? "absent" : "present") << "\n"
            << std::flush;
}

void print_capabilities(const ClipboardCapabilities& capabilities) {
  std::cout << "backend=" << capabilities.backend << "\n";
  std::cout << "session=" << capabilities.session << "\n";
  std::cout << "read=" << yes_no(capabilities.read)
            << " write=" << yes_no(capabilities.write)
            << " watch=" << yes_no(capabilities.watch)
            << " independent_of_input="
            << yes_no(capabilities.independent_of_input) << "\n";
  if (!capabilities.limitation.empty()) {
    std::cout << "limitation=" << capabilities.limitation << "\n";
  }
  for (const std::string& note : capabilities.notes) {
    std::cout << "note=" << note << "\n";
  }
}

int run_read_text(const Options& options) {
  PortCreation creation = create_clipboard_port();
  if (!creation.port) {
    std::cerr << "clipboard unavailable: " << creation.status.detail << "\n";
    return 2;
  }
  ClipboardContent content;
  const Status status = creation.port->read(content);
  if (!status.ok()) {
    std::cerr << "read failed: " << status_name(status.code) << ": "
              << status.detail << "\n";
    return 2;
  }
  const Validation validation = validate_text(content.text);
  if (!validation.valid) {
    std::cerr << "read rejected: " << status_name(validation.status.code)
              << ": " << validation.status.detail << "\n";
    return 2;
  }
  print_metrics(validation, content.marker);
  if (options.show_content) {
    print_content(validation.normalized);
  }
  return 0;
}

int hold_portal_publication(ClipboardPort& port) {
  if (port.capabilities().backend != "xdg-desktop-portal-clipboard") {
    return 0;
  }
  std::cout << "publishing for 60 seconds; paste in the editor now. "
               "Ctrl+C stops publication.\n" << std::flush;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(60);
  while (std::chrono::steady_clock::now() < deadline) {
    ClipboardContent change;
    std::string detail;
    const WaitResult result = port.wait_for_change(change, 100, detail);
    if (result == WaitResult::Error) {
      std::cerr << "publication failed: " << detail << "\n";
      return 2;
    }
    if (result == WaitResult::Stopped || result == WaitResult::Change) {
      break;
    }
  }
  std::cout << "publication finished; portal selection released.\n";
  return 0;
}

int publish_text(const Options& options, const std::string& text) {
  const Validation validation = validate_text(text);
  if (!validation.valid) {
    std::cerr << "input rejected: " << status_name(validation.status.code)
              << ": " << validation.status.detail << "\n";
    return 2;
  }

  PortCreation creation = create_clipboard_port();
  if (!creation.port) {
    std::cerr << "clipboard unavailable: " << creation.status.detail << "\n";
    return 2;
  }
  const Status write_status = creation.port->write(validation.normalized, "");
  if (!write_status.ok()) {
    std::cerr << "write failed: " << status_name(write_status.code) << ": "
              << write_status.detail << "\n";
    return 2;
  }
  std::cout << "written bytes=" << validation.metrics.bytes
            << " codepoints=" << validation.metrics.codepoints
            << " lines=" << validation.metrics.lines << "\n";
  if (!write_status.detail.empty()) {
    std::cout << "note=" << write_status.detail << "\n";
  }

  ClipboardContent readback;
  const Status read_status = creation.port->read(readback);
  if (!read_status.ok()) {
    std::cout << "readback=unavailable (" << read_status.detail << ")\n";
    return hold_portal_publication(*creation.port);
  }
  const Validation read_validation = validate_text(readback.text);
  if (!read_validation.valid) {
    std::cout << "readback=invalid (" << read_validation.status.detail
              << ")\n";
    return hold_portal_publication(*creation.port);
  }
  const bool match = fingerprint(read_validation.normalized) ==
                     fingerprint(validation.normalized);
  std::cout << "readback=" << (match ? "match" : "mismatch")
            << " bytes=" << read_validation.metrics.bytes << "\n";
  if (options.show_content) {
    print_content(read_validation.normalized);
  }
  return hold_portal_publication(*creation.port);
}

int run_watch(const Options& options) {
  PortCreation creation = create_clipboard_port();
  if (!creation.port) {
    std::cerr << "clipboard unavailable: " << creation.status.detail << "\n";
    return 2;
  }
  print_capabilities(creation.port->capabilities());
  std::cout << "watching; press Ctrl+C to stop. Content is hidden by default.\n"
            << std::flush;

  std::size_t changes = 0;
  for (;;) {
    ClipboardContent content;
    std::string detail;
    const WaitResult result = creation.port->wait_for_change(content, 500,
                                                             detail);
    if (result == WaitResult::Change) {
      ++changes;
      const Validation validation = validate_text(content.text);
      std::cout << "change #" << changes << " ";
      if (!validation.valid) {
        std::cout << "rejected=" << status_name(validation.status.code)
                  << " detail=" << validation.status.detail << "\n"
                  << std::flush;
        continue;
      }
      print_metrics(validation, content.marker);
      if (options.show_content) {
        print_content(validation.normalized);
      }
    } else if (result == WaitResult::Error) {
      std::cerr << "watch failed: " << detail << "\n";
      return 2;
    } else if (result == WaitResult::Stopped) {
      break;
    }
  }
  return 0;
}

int run_emit_samples(const std::string& directory) {
  struct Sample {
    const char* name;
    std::string content;
  };

  std::string large;
  for (int line = 1; line <= 2000; ++line) {
    large += "línea " + std::to_string(line) + ": ñandú 😀 prueba\n";
  }

  const std::vector<Sample> samples = {
      {"accents.txt", u8"Mañana, pingüino, José.\nCanción número 1.\n"},
      {"emoji.txt", u8"Emoji: 😀 🚀 🧪 ñandú\nSegunda línea con 😃.\n"},
      {"multiline.txt",
       u8"línea uno\nlínea dos\n\n\tlínea con tabulador\nfin\n"},
      {"large.txt", large},
  };

  for (const Sample& sample : samples) {
    const std::string path = join_path(directory, sample.name);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
      std::cerr << "cannot create sample file: " << path << "\n";
      return 2;
    }
    file.write(sample.content.data(),
               static_cast<std::streamsize>(sample.content.size()));
    if (!file) {
      std::cerr << "cannot write sample file: " << path << "\n";
      return 2;
    }
    std::cout << "sample=" << path << " bytes=" << sample.content.size()
              << "\n";
  }
  return 0;
}

void print_help() {
  std::cout
      << "OneInTwo — experimento de portapapeles (prototipo 0.1)\n"
      << "Uso: oneintwo-clipboard <modo> [opciones]\n\n"
      << "Modos:\n"
      << "  --help, -h           Muestra esta ayuda y termina.\n"
      << "  --version            Muestra versión y protocolo.\n"
      << "  --inventory          Plataforma, sesión y capacidades. No toca el portapapeles.\n"
      << "  --self-test          Pruebas aisladas con datos sintéticos; sin escritorio ni red.\n"
      << "  --read-text          Lee una vez el texto del portapapeles.\n"
      << "  --publish-text <t>   Escribe <t> en el portapapeles y verifica la lectura.\n"
      << "  --publish-file <p>   Escribe el contenido UTF-8 de <p> en el portapapeles.\n"
      << "                       En Wayland mantiene la oferta hasta 60 s para pegar.\n"
      << "  --watch              Observa cambios hasta Ctrl+C; contenido oculto por defecto.\n"
      << "  --emit-samples <dir> Crea archivos UTF-8 de prueba en un directorio existente.\n"
      << "  --serve              Escucha una conexión TLS; requiere --port y certificados.\n"
      << "  --connect            Conecta a otro equipo; requiere --host, --port y certificados.\n\n"
      << "Opciones:\n"
      << "  --show-content       Muestra el texto observado (sólo datos sintéticos).\n"
      << "  --bind <addr>        Dirección local para --serve (por defecto 0.0.0.0).\n"
      << "  --host <addr>        Dirección del servidor para --connect.\n"
      << "  --port <n>           Puerto TCP explícito (1-65535).\n"
      << "  --cert <pem>         Certificado local.\n"
      << "  --key <pem>          Clave privada local.\n"
      << "  --peer-cert <pem>    Certificado esperado del otro equipo (pin SHA-256).\n\n"
      << "Sin argumentos sólo se muestra esta ayuda. Los certificados y claves\n"
      << "quedan fuera del repositorio; el prototipo no descubre equipos ni\n"
      << "guarda confianza. No se ejecutan pruebas con el portapapeles real\n"
      << "desde --self-test.\n";
}

int print_version() {
  std::cout << "oneintwo-clipboard prototype 0.1\n"
            << "protocol=" << kProtocolVersion << "\n"
            << "max_text_bytes=" << kMaxTextBytes << "\n";
  return 0;
}

int print_inventory() {
  std::cout << "prototype=0.1\n";
  std::cout << "protocol=" << kProtocolVersion << "\n";
  std::cout << "max_text_bytes=" << kMaxTextBytes << "\n";
  std::cout << "echo_window_ms=" << kEchoWindowMs << "\n";
  std::cout << "platform=" << platform_name() << "\n";
  std::cout << platform_inventory_text();
#ifdef OIT_HAVE_OPENSSL
  std::cout << "tls=openssl\n";
#else
  std::cout << "tls=unavailable\n";
#endif
  print_capabilities(clipboard_capabilities());
  return 0;
}

int handle_peer_mode(const Options& options) {
  PeerOptions peer;
  peer.server = options.mode == Mode::Serve;
  peer.bind_address = options.bind_address;
  peer.host = options.host;
  peer.port = static_cast<std::uint16_t>(options.port);
  peer.tls = {options.cert, options.key, options.peer_cert};
  peer.show_content = options.show_content;

  PortCreation creation = create_clipboard_port();
  const ClipboardCapabilities capabilities = clipboard_capabilities();
  if (!creation.port) {
    std::cerr << "local clipboard unavailable: " << creation.status.detail
              << "\n";
  }
  return run_peer(peer, creation.port.get(), capabilities);
}

}  // namespace

int run_cli(const std::vector<std::string>& args) {
  Options options;
  if (args.size() <= 1) {
    print_help();
    return 0;
  }

  std::string error;
  bool stop_parsing = false;
  for (std::size_t i = 1; i < args.size() && !stop_parsing; ++i) {
    const std::string& argument = args[i];

    const auto select_mode = [&](Mode mode, const char* name) {
      if (options.mode_set) {
        error = std::string("only one mode is allowed; unexpected '") + name +
                "'";
        return false;
      }
      options.mode = mode;
      options.mode_set = true;
      return true;
    };
    const auto take_value = [&](const char* name, std::string& destination) {
      if (i + 1 >= args.size()) {
        error = std::string("missing value for ") + name;
        return false;
      }
      destination = args[++i];
      return true;
    };

    if (argument == "--help" || argument == "-h") {
      if (!select_mode(Mode::Help, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--version") {
      if (!select_mode(Mode::Version, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--self-test") {
      if (!select_mode(Mode::SelfTest, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--inventory") {
      if (!select_mode(Mode::Inventory, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--read-text") {
      if (!select_mode(Mode::ReadText, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--watch") {
      if (!select_mode(Mode::Watch, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--serve") {
      if (!select_mode(Mode::Serve, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--connect") {
      if (!select_mode(Mode::Connect, argument.c_str())) {
        stop_parsing = true;
      }
    } else if (argument == "--publish-text") {
      if (!select_mode(Mode::PublishText, argument.c_str()) ||
          !take_value("--publish-text", options.value)) {
        stop_parsing = true;
      }
    } else if (argument == "--publish-file") {
      if (!select_mode(Mode::PublishFile, argument.c_str()) ||
          !take_value("--publish-file", options.path)) {
        stop_parsing = true;
      }
    } else if (argument == "--emit-samples") {
      if (!select_mode(Mode::EmitSamples, argument.c_str()) ||
          !take_value("--emit-samples", options.path)) {
        stop_parsing = true;
      }
    } else if (argument == "--host") {
      if (!take_value("--host", options.host)) {
        stop_parsing = true;
      }
    } else if (argument == "--bind") {
      if (!take_value("--bind", options.bind_address)) {
        stop_parsing = true;
      }
    } else if (argument == "--cert") {
      if (!take_value("--cert", options.cert)) {
        stop_parsing = true;
      }
    } else if (argument == "--key") {
      if (!take_value("--key", options.key)) {
        stop_parsing = true;
      }
    } else if (argument == "--peer-cert") {
      if (!take_value("--peer-cert", options.peer_cert)) {
        stop_parsing = true;
      }
    } else if (argument == "--show-content") {
      options.show_content = true;
    } else if (argument == "--port") {
      std::string value;
      if (!take_value("--port", value)) {
        stop_parsing = true;
        continue;
      }
      try {
        const int parsed = std::stoi(value);
        if (parsed < 1 || parsed > 65535) {
          error = "--port must be between 1 and 65535";
          stop_parsing = true;
        } else {
          options.port = parsed;
        }
      } catch (const std::exception&) {
        error = "invalid --port value";
        stop_parsing = true;
      }
    } else {
      error = "unknown argument: " + argument;
      stop_parsing = true;
    }
  }

  if (!error.empty()) {
    std::cerr << "error: " << error << "\n\n";
    print_help();
    return 2;
  }
  if (!options.mode_set) {
    std::cerr << "error: a mode is required\n\n";
    print_help();
    return 2;
  }

  if (options.mode == Mode::Serve || options.mode == Mode::Connect) {
    if (options.port == 0) {
      std::cerr << "error: --port is required for peer mode\n";
      return 2;
    }
    if (options.cert.empty() || options.key.empty() ||
        options.peer_cert.empty()) {
      std::cerr << "error: --cert, --key and --peer-cert are required\n";
      return 2;
    }
    const std::pair<const char*, const std::string*> files[] = {
        {"--cert", &options.cert},
        {"--key", &options.key},
        {"--peer-cert", &options.peer_cert},
    };
    for (const auto& entry : files) {
      if (!file_exists(*entry.second)) {
        std::cerr << "error: cannot open " << entry.first << " file: "
                  << *entry.second << "\n";
        return 2;
      }
    }
    if (options.mode == Mode::Connect && options.host.empty()) {
      std::cerr << "error: --host is required for --connect\n";
      return 2;
    }
  }

  switch (options.mode) {
    case Mode::Help:
      print_help();
      return 0;
    case Mode::Version:
      return print_version();
    case Mode::SelfTest:
      return run_self_tests();
    case Mode::Inventory:
      return print_inventory();
    case Mode::ReadText:
      return run_read_text(options);
    case Mode::PublishText:
      return publish_text(options, options.value);
    case Mode::PublishFile: {
      std::string text;
      std::string read_error;
      if (!read_utf8_file(options.path, text, read_error)) {
        std::cerr << read_error << "\n";
        return 2;
      }
      return publish_text(options, text);
    }
    case Mode::Watch:
      return run_watch(options);
    case Mode::EmitSamples:
      return run_emit_samples(options.path);
    case Mode::Serve:
    case Mode::Connect:
      return handle_peer_mode(options);
  }
  return 2;
}

}  // namespace oit
