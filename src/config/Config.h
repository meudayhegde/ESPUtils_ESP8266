#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================================
// Device Configuration
// ================================
namespace Config {
    const char FIRMWARE_VERSION[] = "{{--@FIRMWARE_VERSION@--}}";
    // const char DEVICE_SECRET_KEY[] = "{{--@DEVICE_SECRET_KEY@--}}";
    const char JWT_PUB_KEY[] = "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIAnXvd2yBqvGfsjTi4cAQ0hYkaRi\n/MqU8VSHyIBzErl8L2A2SERAXb6Epvjv4Zb5nu78LiIfkuB6gJvMj/fUrA==\n-----END PUBLIC KEY-----";
    
    // Device Identity
    const char DEVICE_NAME[] = "ESPUtils";
    const char DEVICE_PASSWORD[] = "ESP.device@8266";
    const char DEFAULT_MODE[] = "AP"; // AP / WIFI

    // Hardware Pin Configuration
    #ifdef ARDUINO_ARCH_ESP8266
        constexpr uint8_t LED_PIN = 2;
    #elif ARDUINO_ARCH_ESP32
        constexpr uint8_t LED_PIN = 8;
    #endif
    
    constexpr uint8_t IR_RECV_PIN = 14;
    constexpr uint8_t IR_SEND_PIN = 4;
    
    // Network Ports
    constexpr uint16_t HTTP_PORT          = 80;
    constexpr uint16_t OTA_PORT           = 48325;
    // Camera stream port (ESP32 only) — the embedded camera HTML/JS
    // constructs the stream URL as  document.location.origin + ':81'
    constexpr uint16_t CAMERA_STREAM_PORT = 81;   // MJPEG stream (hardcoded in JS)
    
    // Timing Configuration
    constexpr uint8_t RECV_TIMEOUT_SEC = 8;
    constexpr uint8_t WIRELESS_TIMEOUT_SEC = 20;
    constexpr uint8_t IR_TIMEOUT_MS = 50;
    
    // Session Configuration
    constexpr unsigned long SESSION_EXPIRY_SECONDS = 604800;      // 1 week in seconds
    constexpr unsigned long SESSION_EXPIRY_MS      = 604800000UL; // 1 week in ms (for millis() comparison)
    constexpr uint8_t       MAX_SESSIONS           = 5;           // max concurrent session tokens
    
    // Serial Configuration
    constexpr uint32_t BAUD_RATE = 115200;
    constexpr bool SERIAL_MONITOR_ENABLED = true;
    constexpr bool LEGACY_TIMING_INFO = false;
    
    // IR Configuration
    constexpr uint16_t CAPTURE_BUFFER_SIZE = 1024;
    constexpr uint16_t IR_FREQUENCY = 38000;
    constexpr uint8_t MIN_UNKNOWN_SIZE = 12;
    
    // Camera Configuration (ESP32 only)
    constexpr uint32_t CAMERA_XCLK_FREQ_HZ = 20000000;  // 20 MHz
    
    // Range Extender Configuration
    // ESP8266: lwIP NAPT table sizes (heap-limited; default 1000/10 is a safe choice)
    constexpr uint16_t NAPT_TABLE_SIZE = 1000;
    constexpr uint16_t NAPT_PORT_TABLE_SIZE = 10;
    // AP-side address used by the range-extender soft-AP (Android-compatible Google subnet)
    const uint8_t RANGE_EXT_AP_IP[4]      = { 172, 217, 28, 254 };
    const uint8_t RANGE_EXT_AP_SUBNET[4]  = { 255, 255, 255, 0  };
    // ESP32 AP-side defaults
    const uint8_t RANGE_EXT32_AP_IP[4]       = { 192, 168,  4, 1 };
    const uint8_t RANGE_EXT32_AP_LEASE[4]    = { 192, 168,  4, 2 };
    const uint8_t RANGE_EXT32_AP_DNS[4]      = {   8,   8,  4, 4 };
    
    // Protocol Configuration
    constexpr uint16_t MAX_REQUEST_LENGTH = 5120;
    
    // File Paths
    const char WIFI_CONFIG_FILE[] = "/WiFiConfig.bin";
    const char LOGIN_CREDENTIAL_FILE[] = "/LoginCredential.json";
    const char GPIO_CONFIG_FILE[] = "/GPIOConfig.json";
    const char SESSION_FILE[] = "/Session.json";
    const char BOUND_TOKEN_FILE[] = "/BoundToken.bin";  // persisted JWT used to verify identity on boot
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

struct GPIOConfig {
    int pinNumber;
    String pinMode;
    int pinValue;
    
    GPIOConfig() : pinNumber(-1), pinMode(""), pinValue(0) {}
    GPIOConfig(int pin, const String& mode, int value) : 
        pinNumber(pin), pinMode(mode), pinValue(value) {}
};

// ================================
// Response Messages
// ================================
namespace ResponseMsg {
    const char SUCCESS[] = "success";
    const char FAILURE[] = "failure";
    const char DENY[] = "deny";
    const char UNAUTHORIZED[] = "unauthorized";
    const char AUTHENTICATED[] = "authenticated";
    const char INVALID_TOKEN[] = "invalid_token";
    const char SESSION_EXPIRED[] = "session_expired";
    const char TIMEOUT[] = "timeout";
    const char PROGRESS[] = "progress";
    const char UNDEFINED[] = "undefined";
    const char INVALID_PURPOSE[] = "Invalid Purpose";
    const char JSON_ERROR[] = "JSON Error, failed to parse the request";
    const char PURPOSE_NOT_DEFINED[] = "Purpose not defined";
}

#endif // CONFIG_H
