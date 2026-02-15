#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266WebServer.h>
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
    #include <WebServer.h>
#endif
#include "src/config/Config.h"
#include "src/auth/AuthManager.h"
#include "src/network/WirelessNetworkManager.h"
#include "src/hardware/GPIOManager.h"
#include "src/hardware/IRManager.h"
#include "src/storage/StorageManager.h"
#include "src/utils/Utils.h"

#ifdef ARDUINO_ARCH_ESP8266
    typedef ESP8266WebServer WebServerType;
#else
    typedef WebServer WebServerType;
#endif

class ESPCommandHandler {
public:
    /**
     * @brief Initialize HTTP REST API endpoints
     * @param server WebServer instance
     */
    static void setupRoutes(WebServerType& server);
    
private:
    // Middleware
    /**
     * @brief LED indicator middleware wrapper
     * @param server WebServer instance
     * @param handler Request handler function
     */
    template<typename HandlerFunc>
    static void withLEDIndicator(WebServerType& server, HandlerFunc handler) {
        Utils::setLED(LOW);  // Turn LED on (active LOW)
        handler(server);
        Utils::setLED(HIGH); // Turn LED off
    }
    
    /**
     * @brief Validate session token from Authorization header
     * @param server WebServer instance
     * @return true if valid, false otherwise
     */
    static bool validateSessionToken(WebServerType& server);
    
    /**
     * @brief Send JSON error response
     * @param server WebServer instance
     * @param code HTTP status code
     * @param message Error message
     */
    static void sendError(WebServerType& server, int code, const char* message);
    
    /**
     * @brief Send JSON success response
     * @param server WebServer instance
     * @param data JSON data to send
     */
    static void sendSuccess(WebServerType& server, const String& data);
    
    // Public endpoints (no auth required)
    static void handlePing(WebServerType& server);
    
    // Authentication endpoint
    static void handleAuth(WebServerType& server);
    
    // Protected endpoints (require session token)
    static void handleDeviceInfo(WebServerType& server);
    static void handleIRCapture(WebServerType& server);
    static void handleIRSend(WebServerType& server);
    static void handleSetWireless(WebServerType& server);
    static void handleGetWireless(WebServerType& server);
    static void handleSetUser(WebServerType& server);
    static void handleGPIOSet(WebServerType& server);
    static void handleGPIOGet(WebServerType& server);
    static void handleRestart(WebServerType& server);
    static void handleReset(WebServerType& server);
};

#endif // REQUEST_HANDLER_H
