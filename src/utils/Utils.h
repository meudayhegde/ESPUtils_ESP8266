#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "src/config/Config.h"

namespace Utils {
    /**
     * @brief Print message to serial if enabled
     * @param message Message to print
     * @param end End character (default: newline)
     * @return Number of bytes written
     */
    size_t printSerial(const String& message, const char* end = "\n");
    size_t printSerial(const char* message, const char* end = "\n");
    size_t printSerial(const __FlashStringHelper* message, const char* end = "\n");
    
    /**
     * @brief Pulse LED for visual feedback
     * @param onTimeMs LED on duration in milliseconds
     * @param offTimeMs LED off duration in milliseconds
     * @param count Number of pulses
     */
    void ledPulse(int onTimeMs, int offTimeMs, int count);
    
    /**
     * @brief Convert hex string to uint64_t
     * @param hex Hex string to convert (with or without 0x prefix)
     * @return Converted uint64_t value
     */
    uint64_t getUInt64FromHex(const char* hex);
    
    /**
     * @brief Initialize LED pin
     */
    inline void initLED() {
        pinMode(Config::LED_PIN, OUTPUT);
        digitalWrite(Config::LED_PIN, HIGH); // LED off (assuming active LOW)
    }
    
    /**
     * @brief Set LED state
     * @param state HIGH or LOW
     */
    inline void setLED(uint8_t state) {
        digitalWrite(Config::LED_PIN, state);
    }
    
    /**
     * @brief Get unique device ID (eFuseMac) from device
     * Works for both ESP8266 and ESP32
     * @return 48-bit device ID as uint64_t
     */
    uint64_t getDeviceID();
    
    /**
     * @brief Get device ID as formatted hex string
     * @return Device ID formatted as uppercase hex (e.g., "AABBCCDDEEFF")
     */
    String getDeviceIDString();
}

#endif // UTILS_H
