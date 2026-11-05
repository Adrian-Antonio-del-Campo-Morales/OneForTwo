// Responsabilidad: regresión de lectura PEM en la DLL TLS, sin red ni escritorio.
#include "net/tls_channel.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: clipboard_tls_certificate_test CERT SHA256 INVALID_PEM\n";
    return 1;
  }
  oit::TlsFiles files;
  files.cert_file = argv[1];
  oit::LocalTlsInfo info;
  std::string error;
  if (!oit::describe_local_certificate(files, info, error)) {
    std::cerr << error << '\n';
    return 1;
  }
  std::string actual;
  for (unsigned char c : info.cert_fingerprint) {
    if (c != ':') actual += static_cast<char>(std::tolower(c));
  }
  std::ifstream expected_file(argv[2]);
  std::string expected;
  if (!std::getline(expected_file, expected) || actual != expected) {
    std::cerr << "certificate fingerprint mismatch\n";
    return 1;
  }
  files.cert_file = std::string(argv[1]) + ".missing";
  if (oit::describe_local_certificate(files, info, error)) return 1;
  files.cert_file = argv[3];
  if (oit::describe_local_certificate(files, info, error)) return 1;
  std::cout << "PASS certificate fingerprint, missing file and invalid PEM\n";
  return 0;
}
