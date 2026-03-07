#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "Features.h"

// ════════════════════════════════════════════════════════════════════════
// Device Configuration
//
// String constants that are large (JWT_PUB_KEY, file paths, firmware
// version) are declared extern — defined once in Config.cpp — so that
// the linker keeps a single copy in flash instead of one per TU.
//
// Short identity strings remain inline (negligible duplication).
// ════════════════════════════════════════════════════════════════════════
namespace Config {

    // ── Firmware version (substituted at build time by create_ota_package.py)
    extern const char FIRMWARE_VERSION[];

    // ── Authentication — EC P-256 public key (PEM format, verified only)
    extern const char JWT_PUB_KEY[];

    // ── Device identity (short — kept inline for convenience)
    const char DEVICE_NAME[]     = "AetherPulse";
    const char DEVICE_PASSWORD[] = "ESP.device@8266";
    const char DEFAULT_MODE[]    = "AP"; // AP | WIFI | AP_STA

    // ── Hardware pin configuration ────────────────────────────────────────
    #if defined(LED_BUILTIN)
        constexpr uint8_t LED_PIN = LED_BUILTIN;
    #elif defined(ARDUINO_ARCH_ESP8266)
        constexpr uint8_t LED_PIN = 2;
    #elif defined(ARDUINO_ARCH_ESP32)
        constexpr uint8_t LED_PIN = 8;
    #endif

    constexpr uint8_t IR_RECV_PIN = 14;
    constexpr uint8_t IR_SEND_PIN = 4;

    // ── Network ports ─────────────────────────────────────────────────────
    constexpr uint16_t HTTP_PORT          = 80;
    constexpr uint16_t OTA_PORT           = 48325;
    // Camera stream port (ESP32 only — the embedded JS uses origin + ':81')
    constexpr uint16_t CAMERA_STREAM_PORT = 81;

    // ── Timing ────────────────────────────────────────────────────────────
    constexpr uint8_t  RECV_TIMEOUT_SEC     = 8;
    constexpr uint8_t  WIRELESS_TIMEOUT_SEC = 20;
    constexpr uint8_t  IR_TIMEOUT_MS        = 50;

    // ── Session ───────────────────────────────────────────────────────────
    constexpr unsigned long SESSION_EXPIRY_SECONDS = 604800UL;       // 1 week
    constexpr unsigned long SESSION_EXPIRY_MS      = 604800000UL;    // 1 week (millis)
    constexpr uint8_t       MAX_SESSIONS           = 5;

    // ── Serial ────────────────────────────────────────────────────────────
    constexpr uint32_t BAUD_RATE             = 115200;
    constexpr bool     SERIAL_MONITOR_ENABLED = (FEATURE_SERIAL_LOG_ENABLED != 0);
    constexpr bool     LEGACY_TIMING_INFO     = false;

    // ── IR ────────────────────────────────────────────────────────────────
    constexpr uint16_t CAPTURE_BUFFER_SIZE = 1024;
    constexpr uint16_t IR_FREQUENCY        = 38000;
    constexpr uint8_t  MIN_UNKNOWN_SIZE    = 12;
    // Maximum number of raw IR entries to send (prevents 2 KB VLA on stack)
    constexpr uint16_t IR_RAW_SEND_MAX     = 512;

    // ── Camera (ESP32 only) ───────────────────────────────────────────────
    constexpr uint32_t CAMERA_XCLK_FREQ_HZ = 20000000; // 20 MHz

    // ── Range extender ────────────────────────────────────────────────────
    // ESP8266 lwIP NAPT table sizes
    constexpr uint16_t NAPT_TABLE_SIZE      = 1000;
    constexpr uint16_t NAPT_PORT_TABLE_SIZE = 10;
    // Soft-AP address (android-compatible Google subnet)
    const uint8_t RANGE_EXT_AP_IP[4]     = { 172, 217,  28, 254 };
    const uint8_t RANGE_EXT_AP_SUBNET[4] = { 255, 255, 255,   0 };
    // ESP32 AP-side defaults
    const uint8_t RANGE_EXT32_AP_IP[4]    = { 192, 168,  4,   1 };
    const uint8_t RANGE_EXT32_AP_LEASE[4] = { 192, 168,  4,   2 };
    const uint8_t RANGE_EXT32_AP_DNS[4]   = {   8,   8,  4,   4 };

    // ── Protocol ──────────────────────────────────────────────────────────
    constexpr uint16_t MAX_REQUEST_LENGTH = 5120;

    // ── Rate limiting ─────────────────────────────────────────────────────
    constexpr uint8_t  RATE_LIMIT_MAX_REQUESTS = 2;      // max requests per window
    constexpr uint32_t RATE_LIMIT_WINDOW_MS     = 1000;  // sliding window duration (ms)

    // ── Flash file paths (extern — single copy in flash via Config.cpp) ───
    extern const char WIFI_CONFIG_FILE[];
    extern const char LOGIN_CREDENTIAL_FILE[];
    extern const char GPIO_CONFIG_FILE[];
    extern const char SESSION_FILE[];
    extern const char BOUND_TOKEN_FILE[];
}

// ================================
// Data Structures
// ================================

// Bound identity stored to flash on first-login; read back on every boot.
// jwt[] is sized for the longest ES256 JWT we expect to receive.
struct BoundTokenData {
    char sub[64];   // JWT "sub" claim
    char jwt[512];  // raw JWT string (header.payload.sig)

    BoundTokenData() {
        sub[0] = '\0';
        jwt[0] = '\0';
    }
};

struct WirelessConfig {
    char mode[8];           // "AP", "WIFI", "AP_STA"
    char stationSSID[33];   // WiFi SSID (max 32 chars)
    char stationPSK[64];    // WiFi passphrase (max 63 chars)
    char apSSID[33];        // SoftAP SSID
    char apPSK[64];         // SoftAP passphrase

    WirelessConfig() {
        strncpy(mode,        Config::DEFAULT_MODE,     sizeof(mode)        - 1); mode[sizeof(mode)               - 1] = '\0';
        strncpy(stationSSID, Config::DEVICE_NAME,      sizeof(stationSSID) - 1); stationSSID[sizeof(stationSSID) - 1] = '\0';
        strncpy(stationPSK,  Config::DEVICE_PASSWORD,  sizeof(stationPSK)  - 1); stationPSK[sizeof(stationPSK)   - 1] = '\0';
        strncpy(apSSID,      Config::DEVICE_NAME,      sizeof(apSSID)      - 1); apSSID[sizeof(apSSID)           - 1] = '\0';
        strncpy(apPSK,       Config::DEVICE_PASSWORD,  sizeof(apPSK)       - 1); apPSK[sizeof(apPSK)             - 1] = '\0';
    }
};

// GPIO pin mode — sequential enum stored as uint8_t.
//   0 = INPUT, 1 = OUTPUT, 2 = INPUT_PULLUP, 3 = INPUT_PULLDOWN
enum GPIOPinMode : uint8_t {
    AP_GPIO_INPUT          = 0,
    AP_GPIO_OUTPUT         = 1,
    AP_GPIO_INPUT_PULLUP   = 2,
    AP_GPIO_INPUT_PULLDOWN = 3,
};

// Single GPIO pin configuration (binary-safe, no strings).
struct GPIOConfig {
    int     pinNumber;
    uint8_t mode;       // GPIOPinMode enum value
    int     pinValue;

    GPIOConfig() : pinNumber(-1), mode(AP_GPIO_INPUT), pinValue(0) {}
    GPIOConfig(int pin, uint8_t m, int value) : pinNumber(pin), mode(m), pinValue(value) {}
};

// Fixed-size container written as a single binary blob to flash.
static constexpr uint8_t MAX_GPIO_PINS = 20;

struct GPIOConfigData {
    uint8_t    count;
    GPIOConfig pins[MAX_GPIO_PINS];

    GPIOConfigData() : count(0) {}
};

// ── Response message tokens (extern — single definition in Config.cpp) ────────
namespace ResponseMsg {
    extern const char SUCCESS[];
    extern const char FAILURE[];
    extern const char DENY[];
    extern const char UNAUTHORIZED[];
    extern const char AUTHENTICATED[];
    extern const char INVALID_TOKEN[];
    extern const char SESSION_EXPIRED[];
    extern const char TIMEOUT[];
    extern const char PROGRESS[];
    extern const char UNDEFINED[];
    extern const char INVALID_PURPOSE[];
    extern const char JSON_ERROR[];
    extern const char PURPOSE_NOT_DEFINED[];
}

#endif // CONFIG_H
