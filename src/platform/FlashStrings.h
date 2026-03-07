#ifndef FLASH_STRINGS_H
#define FLASH_STRINGS_H

// ── PROGMEM-aware string helpers ──────────────────────────────────────────────
//
// On ESP8266: string constants declared with FLASH_STR are placed in flash
//             (PROGMEM), saving ~1 byte of IRAM per character.  Access them
//             via FLASH_STR_READ() which wraps FPSTR() for safe reading.
//
// On ESP32:  flash storage of .rodata is automatic — no annotation needed.
//             FLASH_STR reduces to a plain constexpr const char[] and
//             FLASH_STR_READ() is a pass-through.
//
// Usage (file/namespace scope only — never inside a function body):
//   FLASH_STR(kHello, "Hello, world!");
//   Utils::printSerial(FLASH_STR_READ(kHello));
//
// For inline anonymous strings prefer the Arduino F() macro:
//   Serial.print(F("inline string"));

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
    // Place a named string constant in flash (PROGMEM)
    #define FLASH_STR(name, value)   static const char name[] PROGMEM = value
    // Read a PROGMEM string — wraps FPSTR() for use with Serial.print, etc.
    #define FLASH_STR_READ(name)     FPSTR(name)
#else
    // ESP32: .rodata auto-mapped to flash — PROGMEM is accepted but a no-op
    #define FLASH_STR(name, value)   static const char name[] = value
    #define FLASH_STR_READ(name)     (name)
#endif

#endif // FLASH_STRINGS_H
