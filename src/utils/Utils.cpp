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
        #if defined(ARDUINO_ARCH_ESP8266)
            // ESP8266 chip ID is a 32-bit value derived from the MAC address
            return (uint64_t)ESP.getChipId();
        #elif defined(ARDUINO_ARCH_ESP32)
            // ESP32 eFuseMac is a 48-bit factory-programmed unique identifier
            return ESP.getEfuseMac();
        #else
            return 0ULL;
        #endif
    }

    // Returns a pointer to a static 16-byte buffer — copy immediately if persistence needed.
    const char* getDeviceIDString() {
        uint64_t deviceid = getDeviceID();
        static char buffer[16];
        #if defined(ARDUINO_ARCH_ESP8266)
            snprintf(buffer, sizeof(buffer), "%08llX", (unsigned long long)deviceid);
        #elif defined(ARDUINO_ARCH_ESP32)
            snprintf(buffer, sizeof(buffer), "%012llX",
                     (unsigned long long)(deviceid & 0xFFFFFFFFFFFFULL));
        #else
            snprintf(buffer, sizeof(buffer), "%016llX", (unsigned long long)deviceid);
        #endif
        return buffer;
    }
}
