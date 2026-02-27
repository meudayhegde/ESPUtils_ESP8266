#include "Utils.h"

namespace Utils {
    
    
    void ledPulse(int onTimeMs, int offTimeMs, int count) {
        for (int i = 0; i < count; i++) {
            printSerial(F("."), "");
            digitalWrite(Config::LED_PIN, LOW);
            delay(onTimeMs);
            digitalWrite(Config::LED_PIN, HIGH);
            delay(offTimeMs);
        }
        printSerial(F("."));
    }
    
    uint64_t getUInt64FromHex(const char* hex) {
        uint64_t result = 0;
        uint16_t offset = 0;
        
        // Skip 0x or 0X prefix if present
        if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            offset = 2;
        }
        
        // Convert hex string to uint64
        while (isxdigit((unsigned char)hex[offset])) {
            char c = hex[offset];
            result <<= 4; // Faster than multiply by 16
            
            if (isdigit(c)) {
                result += c - '0';
            } else {
                result += (isupper(c) ? c - 'A' : c - 'a') + 10;
            }
            offset++;
        }
        
        return result;
    }
    
    uint64_t getDeviceID() {
        #ifdef ARDUINO_ARCH_ESP8266
            // For ESP8266: Use built-in chip ID
            return ESP.getChipId();
        #elif ARDUINO_ARCH_ESP32
            // For ESP32: Get eFuseMac (48-bit unique identifier)
            return ESP.getEfuseMac();
        #else
            return 0;
        #endif
    }
    
    char* getDeviceIDString() {
        uint64_t deviceid = getDeviceID();
        static char buffer[16];
        
        #ifdef ARDUINO_ARCH_ESP8266
            // ESP8266 device ID is 32-bit
            snprintf(buffer, sizeof(buffer), "%08llX", (unsigned long long)deviceid);
        #elif ARDUINO_ARCH_ESP32
            // ESP32 eFuseMac is 48-bit (6 bytes)
            snprintf(buffer, sizeof(buffer), "%012llX", (unsigned long long)(deviceid & 0xFFFFFFFFFFFFULL));
        #endif
        
        return buffer;
    }
}
