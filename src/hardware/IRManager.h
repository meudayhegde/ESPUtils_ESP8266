#ifndef IR_MANAGER_H
#define IR_MANAGER_H

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
#include <ArduinoJson.h>
#include "src/config/Config.h"
#include "src/utils/Utils.h"

#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
#endif

class IRManager {
private:
    static IRrecv* irRecv;
    static IRsend* irSend;
    static decode_results results;
    
public:
    /**
     * @brief Initialize IR receiver and sender
     */
    static void begin();
    
    /**
     * @brief Capture IR signal from remote control
     * @param multiCapture Enable multi-capture mode
     * @param client WiFiClient for streaming results
     * @return JSON response string
     */
    static String captureIR(bool multiCapture, WiFiClient& client);
    
    /**
     * @brief Send IR signal
     * @param size Data size
     * @param protocol Protocol name
     * @param irData IR data (hex string or array)
     * @return JSON response string
     */
    static String sendIR(uint16_t size, const char* protocol, const char* irData);
    
    /**
     * @brief Generate IR result JSON from decode_results
     * @param results Pointer to decode_results
     * @return JSON string with IR data
     */
    static String generateIRResult(const decode_results* results);
    
private:
    /**
     * @brief Send raw IR array
     * @param size Array size
     * @param irData JSON array string
     */
    static void sendRawArray(uint16_t size, const char* irData);
    
    /**
     * @brief Send IR value (non-AC protocols)
     * @param size Data size
     * @param protocol Protocol type
     * @param irData Hex string
     * @return true if successful, false otherwise
     */
    static bool sendIRValue(uint16_t size, decode_type_t protocol, const char* irData);
    
    /**
     * @brief Send IR state (AC protocols)
     * @param size State array size
     * @param protocol Protocol type
     * @param data State array as JSON string
     * @return true if successful, false otherwise
     */
    static bool sendIRState(uint16_t size, decode_type_t protocol, const char* data);
};

#endif // IR_MANAGER_H
