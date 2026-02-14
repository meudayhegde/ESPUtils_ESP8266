#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
#endif
#include "src/config/Config.h"
#include "src/auth/AuthManager.h"
#include "src/network/WirelessNetworkManager.h"
#include "src/hardware/GPIOManager.h"
#include "src/hardware/IRManager.h"
#include "src/storage/StorageManager.h"
#include "src/utils/Utils.h"

class RequestHandler {
public:
    /**
     * @brief Handle incoming request from client
     * @param request Request string (JSON format)
     * @param client WiFiClient for bidirectional communication
     * @return JSON response string
     */
    static String handleRequest(const String& request, WiFiClient& client);
    
private:
    /**
     * @brief Handle ping request
     * @return JSON response with MAC address and chip ID
     */
    static String handlePing();
    
    /**
     * @brief Handle device info request
     * @return JSON response with comprehensive device details
     */
    static String handleDeviceInfo();
    
    /**
     * @brief Handle authentication request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleAuthenticate(const JsonDocument& doc);
    
    /**
     * @brief Handle IR capture request
     * @param doc JsonDocument containing request data
     * @param client WiFiClient for streaming results
     * @return JSON response
     */
    static String handleIRCapture(const JsonDocument& doc, WiFiClient& client);
    
    /**
     * @brief Handle IR send request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleIRSend(const JsonDocument& doc);
    
    /**
     * @brief Handle wireless configuration update request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleSetWireless(const JsonDocument& doc);
    
    /**
     * @brief Handle user credentials update request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleSetUser(const JsonDocument& doc);
    
    /**
     * @brief Handle get wireless configuration request
     * @return JSON response with wireless configuration
     */
    static String handleGetWireless();
    
    /**
     * @brief Handle GPIO set request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleGPIOSet(const JsonDocument& doc);
    
    /**
     * @brief Handle GPIO get request
     * @param doc JsonDocument containing request data
     * @return JSON response
     */
    static String handleGPIOGet(const JsonDocument& doc);
    
    /**
     * @brief Handle restart request
     */
    static void handleRestart();
    
    /**
     * @brief Handle factory reset request
     * @return JSON response
     */
    static String handleReset();
    
    /**
     * @brief Verify authentication from request
     * @param doc JsonDocument containing credentials
     * @return true if authenticated, false otherwise
     */
    static bool verifyAuth(const JsonDocument& doc);
    
    /**
     * @brief Build device info JSON response
     * @return JSON string with device details
     */
    static String buildDeviceInfoJson();
};

#endif // REQUEST_HANDLER_H
