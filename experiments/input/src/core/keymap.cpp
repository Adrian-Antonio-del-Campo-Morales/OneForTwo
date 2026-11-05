// Responsabilidad: tabla explícita de teclas soportadas y conversiones entre
// uso HID, código de escaneo de Windows y código evdev de Linux.
#include "keymap.hpp"

namespace oit {
namespace {

struct KeyEntry {
  uint16_t usage;
  uint8_t winScan;
  bool winExtended;
  uint16_t linuxCode;
};

// Tabla basada en el conjunto de escaneo 1 de Windows, los usos HID de teclado
// (página 0x07) y linux/input-event-codes.h. Cubre el bloque principal, teclas
// de función, navegación, teclado numérico y modificadores.
constexpr KeyEntry kKeys[] = {
    {0x04, 0x1E, false, 30},   // A
    {0x05, 0x30, false, 48},   // B
    {0x06, 0x2E, false, 46},   // C
    {0x07, 0x20, false, 32},   // D
    {0x08, 0x12, false, 18},   // E
    {0x09, 0x21, false, 33},   // F
    {0x0A, 0x22, false, 34},   // G
    {0x0B, 0x23, false, 35},   // H
    {0x0C, 0x17, false, 23},   // I
    {0x0D, 0x24, false, 36},   // J
    {0x0E, 0x25, false, 37},   // K
    {0x0F, 0x26, false, 38},   // L
    {0x10, 0x32, false, 50},   // M
    {0x11, 0x31, false, 49},   // N
    {0x12, 0x18, false, 24},   // O
    {0x13, 0x19, false, 25},   // P
    {0x14, 0x10, false, 16},   // Q
    {0x15, 0x13, false, 19},   // R
    {0x16, 0x1F, false, 31},   // S
    {0x17, 0x14, false, 20},   // T
    {0x18, 0x16, false, 22},   // U
    {0x19, 0x2F, false, 47},   // V
    {0x1A, 0x11, false, 17},   // W
    {0x1B, 0x2D, false, 45},   // X
    {0x1C, 0x15, false, 21},   // Y
    {0x1D, 0x2C, false, 44},   // Z

    {0x1E, 0x02, false, 2},    // 1
    {0x1F, 0x03, false, 3},    // 2
    {0x20, 0x04, false, 4},    // 3
    {0x21, 0x05, false, 5},    // 4
    {0x22, 0x06, false, 6},    // 5
    {0x23, 0x07, false, 7},    // 6
    {0x24, 0x08, false, 8},    // 7
    {0x25, 0x09, false, 9},    // 8
    {0x26, 0x0A, false, 10},   // 9
    {0x27, 0x0B, false, 11},   // 0

    {0x28, 0x1C, false, 28},   // Enter
    {0x29, 0x01, false, 1},    // Esc
    {0x2A, 0x0E, false, 14},   // Backspace
    {0x2B, 0x0F, false, 15},   // Tab
    {0x2C, 0x39, false, 57},   // Space
    {0x2D, 0x0C, false, 12},   // -
    {0x2E, 0x0D, false, 13},   // =
    {0x2F, 0x1A, false, 26},   // [
    {0x30, 0x1B, false, 27},   // ]
    {0x31, 0x2B, false, 43},   // backslash
    {0x33, 0x27, false, 39},   // ; / ñ en distribución española
    {0x34, 0x28, false, 40},   // ' / ´
    {0x35, 0x29, false, 41},   // ` / º
    {0x36, 0x33, false, 51},   // ,
    {0x37, 0x34, false, 52},   // .
    {0x38, 0x35, false, 53},   // /
    {0x39, 0x3A, false, 58},   // CapsLock

    {0x3A, 0x3B, false, 59},   // F1
    {0x3B, 0x3C, false, 60},   // F2
    {0x3C, 0x3D, false, 61},   // F3
    {0x3D, 0x3E, false, 62},   // F4
    {0x3E, 0x3F, false, 63},   // F5
    {0x3F, 0x40, false, 64},   // F6
    {0x40, 0x41, false, 65},   // F7
    {0x41, 0x42, false, 66},   // F8
    {0x42, 0x43, false, 67},   // F9
    {0x43, 0x44, false, 68},   // F10
    {0x44, 0x57, false, 87},   // F11
    {0x45, 0x58, false, 88},   // F12

    {0x46, 0x37, true, 99},    // PrintScreen
    {0x47, 0x46, false, 70},   // ScrollLock
    {0x48, 0x45, true, 119},   // Pause
    {0x49, 0x52, true, 110},   // Insert
    {0x4A, 0x47, true, 102},   // Home
    {0x4B, 0x49, true, 104},   // PageUp
    {0x4C, 0x53, true, 111},   // Delete
    {0x4D, 0x4F, true, 107},   // End
    {0x4E, 0x51, true, 109},   // PageDown
    {0x4F, 0x4D, true, 106},   // Right
    {0x50, 0x4B, true, 105},   // Left
    {0x51, 0x50, true, 108},   // Down
    {0x52, 0x48, true, 103},   // Up

    {0x53, 0x45, false, 69},   // NumLock
    {0x54, 0x35, true, 98},    // KP /
    {0x55, 0x37, false, 55},   // KP *
    {0x56, 0x4A, false, 74},   // KP -
    {0x57, 0x4E, false, 78},   // KP +
    {0x58, 0x1C, true, 96},    // KP Enter
    {0x59, 0x4F, false, 79},   // KP 1
    {0x5A, 0x50, false, 80},   // KP 2
    {0x5B, 0x51, false, 81},   // KP 3
    {0x5C, 0x4B, false, 75},   // KP 4
    {0x5D, 0x4C, false, 76},   // KP 5
    {0x5E, 0x4D, false, 77},   // KP 6
    {0x5F, 0x47, false, 71},   // KP 7
    {0x60, 0x48, false, 72},   // KP 8
    {0x61, 0x49, false, 73},   // KP 9
    {0x62, 0x52, false, 82},   // KP 0
    {0x63, 0x53, false, 83},   // KP .
    {0x64, 0x56, false, 86},   // tecla ISO adicional (< > en español)
    {0x65, 0x5D, true, 127},   // Application / menú

    {0xE0, 0x1D, false, 29},   // Ctrl izquierdo
    {0xE1, 0x2A, false, 42},   // Shift izquierdo
    {0xE2, 0x38, false, 56},   // Alt izquierdo
    {0xE3, 0x5B, true, 125},   // Win izquierdo
    {0xE4, 0x1D, true, 97},    // Ctrl derecho
    {0xE5, 0x36, false, 54},   // Shift derecho
    {0xE6, 0x38, true, 100},   // Alt derecho / AltGr
    {0xE7, 0x5C, true, 126},   // Win derecho
};

}  // namespace

bool isKeyboardUsage(uint16_t usage) {
  return usage >= 0x04 && usage <= 0xE7;
}

bool isMappedUsage(uint16_t usage) {
  for (const KeyEntry& entry : kKeys) {
    if (entry.usage == usage) return true;
  }
  return false;
}

bool isModifierUsage(uint16_t usage) {
  return usage >= 0xE0 && usage <= 0xE7;
}

uint16_t winScanToHid(uint32_t scanCode, bool extended) {
  const uint32_t scan = scanCode & 0xFFu;
  for (const KeyEntry& entry : kKeys) {
    if (entry.winScan == scan && entry.winExtended == extended) return entry.usage;
  }
  return 0;
}

bool hidToWinScan(uint16_t usage, uint32_t& scanCode, bool& extended) {
  for (const KeyEntry& entry : kKeys) {
    if (entry.usage == usage) {
      scanCode = entry.winScan;
      extended = entry.winExtended;
      return true;
    }
  }
  return false;
}

uint16_t linuxCodeToHid(uint16_t linuxCode) {
  for (const KeyEntry& entry : kKeys) {
    if (entry.linuxCode == linuxCode) return entry.usage;
  }
  return 0;
}

uint16_t hidToLinuxCode(uint16_t usage) {
  for (const KeyEntry& entry : kKeys) {
    if (entry.usage == usage) return entry.linuxCode;
  }
  return 0;
}

uint16_t hidToX11Keycode(uint16_t usage) {
  const uint16_t code = hidToLinuxCode(usage);
  return code == 0 ? 0 : static_cast<uint16_t>(code + 8);
}

uint16_t x11KeycodeToHid(uint16_t keycode) {
  return keycode <= 8 ? 0 : linuxCodeToHid(static_cast<uint16_t>(keycode - 8));
}

}  // namespace oit
