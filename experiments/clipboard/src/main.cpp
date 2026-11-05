// Responsabilidad: obtener los argumentos de forma segura para Unicode y
// entregarlos a la interfaz de consola. No ejecuta modos por sí mismo.

#include "cli.hpp"

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#endif

namespace {

#ifdef _WIN32
std::string wide_to_utf8(const wchar_t* text) {
  if (text == nullptr) {
    return {};
  }
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                         nullptr, nullptr);
  if (length <= 1) {
    return {};
  }
  std::string out(static_cast<std::size_t>(length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr,
                      nullptr);
  return out;
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  (void)argc;
  (void)argv;
  SetConsoleOutputCP(CP_UTF8);

  int count = 0;
  LPWSTR* wide_args = CommandLineToArgvW(GetCommandLineW(), &count);
  std::vector<std::string> args;
  if (wide_args != nullptr) {
    args.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      args.push_back(wide_to_utf8(wide_args[i]));
    }
    LocalFree(wide_args);
  }
  return oit::run_cli(args);
#else
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return oit::run_cli(args);
#endif
}
