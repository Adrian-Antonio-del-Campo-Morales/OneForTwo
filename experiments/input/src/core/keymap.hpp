#pragma once
// Responsibility: canonical key identity (use HID) and conversions with
// Windows and Linux/X11. codes Identity is the physical position of
// the key, not the character it produces.

#include <cstdint>

namespace oit {

namespace hid {
constexpr uint16_t A = 0x04, B = 0x05, C = 0x06, D = 0x07, E = 0x08, F = 0x09,
                     G = 0x0A, H = 0x0B, I = 0x0C, J = 0x0D, K = 0x0E, L = 0x0F,
                     M = 0x10, N = 0x11, O = 0x12, P = 0x13, Q = 0x14, R = 0x15,
                     S = 0x16, T = 0x17, U = 0x18, V = 0x19, W = 0x1A, X = 0x1B,
                     Y = 0x1C, Z = 0x1D;
constexpr uint16_t F12 = 0x45;
constexpr uint16_t Semicolon = 0x33;
constexpr uint16_t NonUsBackslash = 0x64;
constexpr uint16_t LeftCtrl = 0xE0;
constexpr uint16_t LeftShift = 0xE1;
constexpr uint16_t LeftAlt = 0xE2;
constexpr uint16_t LeftGui = 0xE3;
constexpr uint16_t RightCtrl = 0xE4;
constexpr uint16_t RightShift = 0xE5;
constexpr uint16_t RightAlt = 0xE6;
constexpr uint16_t RightGui = 0xE7;
}  // namespace hid

bool isKeyboardUsage(uint16_t usage);
bool isMappedUsage(uint16_t usage);
bool isModifierUsage(uint16_t usage);

// Windows scan code (set 1) -> use HID. 0 if not recognized.
uint16_t winScanToHid(uint32_t scanCode, bool extended);

// Use HID -> Windows scan code. false if it cannot be injected.
bool hidToWinScan(uint16_t usage, uint32_t& scanCode, bool& extended);

// Linux evdev code -> use HID. 0 if not recognized.
uint16_t linuxCodeToHid(uint16_t linuxCode);

// Use HID -> code evdev. 0 if not recognized.
uint16_t hidToLinuxCode(uint16_t usage);

// X11 uses evdev+8 code on evdov-based servers.
uint16_t hidToX11Keycode(uint16_t usage);
uint16_t x11KeycodeToHid(uint16_t keycode);

}  // namespace oit
