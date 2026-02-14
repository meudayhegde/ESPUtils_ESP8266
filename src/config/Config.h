#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Helper macros for PROGMEM strings
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#define FPSTR_TO_CSTR(pstr_pointer) (reinterpret_cast<const char *>(pstr_pointer))

// ================================
// Device Configuration
// ================================
namespace Config {
    // Device Identity
    const char DEVICE_NAME[] PROGMEM = "ESPUtils";
    const char DEVICE_PASSWORD[] PROGMEM = "ESP.device@8266";
    const char DEFAULT_MODE[] PROGMEM = "AP"; // AP / WIFI

    // Hardware Pin Configuration
    #ifdef ARDUINO_ARCH_ESP8266
        constexpr uint8_t LED_PIN = 2;
    #elif ARDUINO_ARCH_ESP32
        constexpr uint8_t LED_PIN = 8;
    #endif
    
    constexpr uint8_t IR_RECV_PIN = 14;
    constexpr uint8_t IR_SEND_PIN = 4;
    
    // Network Ports
    constexpr uint16_t SOCKET_PORT = 48321;
    constexpr uint16_t OTA_PORT = 48325;
    constexpr uint16_t UDP_PORT_ESP = 48327;
    constexpr uint16_t UDP_PORT_APP = 48326;
    
    // Timing Configuration
    constexpr uint8_t RECV_TIMEOUT_SEC = 8;
    constexpr uint8_t WIRELESS_TIMEOUT_SEC = 20;
    constexpr uint8_t IR_TIMEOUT_MS = 50;
    
    // Serial Configuration
    constexpr uint32_t BAUD_RATE = 115200;
    constexpr bool SERIAL_MONITOR_ENABLED = true;
    constexpr bool LEGACY_TIMING_INFO = false;
    
    // IR Configuration
    constexpr uint16_t CAPTURE_BUFFER_SIZE = 1024;
    constexpr uint16_t IR_FREQUENCY = 38000;
    constexpr uint8_t MIN_UNKNOWN_SIZE = 12;
    
    // Protocol Configuration
    constexpr uint16_t MAX_REQUEST_LENGTH = 5120;
    constexpr uint16_t UDP_PACKET_SIZE = 255;
    
    // File Paths
    const char WIFI_CONFIG_FILE[] PROGMEM = "/WiFiConfig.json";
    const char LOGIN_CREDENTIAL_FILE[] PROGMEM = "/LoginCredential.json";
    const char GPIO_CONFIG_FILE[] PROGMEM = "/GPIOConfig.json";
}

// ================================
// Data Structures
// ================================
struct UserConfig {
    String username;
    String password;
    
    UserConfig() : username(String(FPSTR(Config::DEVICE_NAME))), password(String(FPSTR(Config::DEVICE_PASSWORD))) {}
};

struct WirelessConfig {
    String mode;
    String stationSSID;
    String stationPSK;
    String apSSID;
    String apPSK;
    
    WirelessConfig() : 
        mode(String(FPSTR(Config::DEFAULT_MODE))),
        stationSSID(String(FPSTR(Config::DEVICE_NAME))),
        stationPSK(String(FPSTR(Config::DEVICE_PASSWORD))),
        apSSID(String(FPSTR(Config::DEVICE_NAME))),
        apPSK(String(FPSTR(Config::DEVICE_PASSWORD))) {}
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
    const char SUCCESS[] PROGMEM = "success";
    const char FAILURE[] PROGMEM = "failure";
    const char DENY[] PROGMEM = "deny";
    const char AUTHENTICATED[] PROGMEM = "authenticated";
    const char TIMEOUT[] PROGMEM = "timeout";
    const char PROGRESS[] PROGMEM = "progress";
    const char UNDEFINED[] PROGMEM = "undefined";
    const char INVALID_PURPOSE[] PROGMEM = "Invalid Purpose";
    const char JSON_ERROR[] PROGMEM = "JSON Error, failed to parse the request";
    const char PURPOSE_NOT_DEFINED[] PROGMEM = "Purpose not defined";
}

#endif // CONFIG_H
