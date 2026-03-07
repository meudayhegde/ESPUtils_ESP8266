#ifndef FEATURES_H
#define FEATURES_H

// ════════════════════════════════════════════════════════════════════════
// Compile-time feature flags
//
// Each flag controls whether a hardware/software module is compiled in.
// Set a flag to 0 (via build flags or override in a board-specific header)
// to exclude that module — reduces binary size and RAM usage.
//
// Recommended build-flag overrides for constrained boards:
//   -D FEATURE_IR_ENABLED=0        (saves ~30 KB flash)
//   -D FEATURE_SERIAL_LOG_ENABLED=0 (saves ~2 KB flash + improves speed)
// ════════════════════════════════════════════════════════════════════════

// IR capture / transmit hardware support
#ifndef FEATURE_IR_ENABLED
    #define FEATURE_IR_ENABLED 1
#endif

// GPIO pin control with persistent config
#ifndef FEATURE_GPIO_ENABLED
    #define FEATURE_GPIO_ENABLED 1
#endif

// HTTP authentication (JWT ES256)
#ifndef FEATURE_AUTH_ENABLED
    #define FEATURE_AUTH_ENABLED 1
#endif

// Range extender: STA + NATed soft-AP mode
#ifndef FEATURE_RANGE_EXTENDER_ENABLED
    #define FEATURE_RANGE_EXTENDER_ENABLED 1
#endif

// Sleep mode — ESP32 light sleep / ESP8266 modem sleep
// Reduces power consumption when enabled from the client
#ifndef FEATURE_SLEEP_ENABLED
    #define FEATURE_SLEEP_ENABLED 1
#endif

// Serial diagnostic logging via Utils::printSerial
// Set to 0 in production to eliminate all log strings from flash
#ifndef FEATURE_SERIAL_LOG_ENABLED
    #define FEATURE_SERIAL_LOG_ENABLED 1
#endif

// ── Debug build — enable verbose internal logging ─────────────────────────────
// Enable by passing -DDEBUG_BUILD to the compiler (never in production).
// Exposes hash/signature hex dumps in AuthManager and other diagnostics.
#ifdef DEBUG_BUILD
    // Verbose internal diagnostics — only enable during development, NEVER in production.
    // Exposes hash/signature hex dumps and other security-sensitive data over serial.
    #define DEBUG_LOG(msg) \
        do { Serial.print(F("[DBG] ")); Serial.println(F(msg)); } while(0)
    #define DEBUG_LOG_VAL(msg, val) \
        do { Serial.print(F("[DBG] " msg ": ")); Serial.println(val); } while(0)
#else
    #define DEBUG_LOG(msg)          do {} while(0)
    #define DEBUG_LOG_VAL(msg, val) do {} while(0)
#endif

// ── Camera authentication gate ────────────────────────────────────────────────
// Uncomment to require a valid session token for camera capture/control routes.
// Off by default to preserve backward compatibility with the camera web UI.
// #define CAMERA_AUTH_REQUIRED

// ── IR default pin overrides (can be set via build flags) ─────────────────────
// #define DEFAULT_IR_RECV_PIN 14
// #define DEFAULT_IR_SEND_PIN  4

#endif // FEATURES_H
