#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <Arduino.h>
#include "../platform/Platform.h"
#include "../config/Config.h"
#include "../protocol/BinaryProtocol.h"
#include "../auth/AuthManager.h"
#include "../auth/SessionManager.h"
#include "../network/WirelessNetworkManager.h"
#include "../hardware/gpio/GPIOManager.h"
#include "../hardware/infrared/IRManager.h"
#include "../storage/StorageManager.h"
#include "../utils/Utils.h"

// Camera enable/disable API (ESP32 camera boards only)
#if defined(ESP_CAM_HW_EXIST)
#include "../hardware/camera/CameraManager.h"
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
     * @brief LED indicator middleware wrapper.
     *        Also enforces the global rate limit before dispatching.
     * @param server WebServer instance
     * @param handler Request handler function
     */
    template<typename HandlerFunc>
    static void withLEDIndicator(WebServerType& server, HandlerFunc handler) {
        if (!checkRateLimit(server)) return;  // 429 already sent
        Utils::toggleLED();  // Toggle LED
        handler(server);
        Utils::toggleLED(); // Toggle LED back
    }

    /**
     * @brief Global fixed-window rate limiter.
     *        Allows at most Config::RATE_LIMIT_MAX_REQUESTS per
     *        Config::RATE_LIMIT_WINDOW_MS milliseconds across all endpoints.
     * @param server WebServer instance (used to send 429 on violation)
     * @return true if the request is within the limit, false if rejected
     */
    static bool checkRateLimit(WebServerType& server);

    // Rate limiter state (fixed-window counters)
    static uint8_t       _rlCount;        // requests in the current window
    static unsigned long _rlWindowStart;  // millis() when the current window began
    
    /**
     * @brief Validate session token from Authorization header
     * @param server WebServer instance
     * @return true if valid, false otherwise
     */
    static bool validateSessionToken(WebServerType& server);
    
    /**
     * @brief Check if authentication is required for WiFi endpoints
     *        Authentication is only required if device is already bound to an identity
     * @param server WebServer instance
     * @return true if authentication check passed, false otherwise (error sent)
     */
    static bool checkWiFiAuth(WebServerType& server);
    
    /**
     * @brief Validate JWT token from Authorization: Bearer header for reset endpoint
     *        Validates challenge, signature, and ensures sub matches bound sub (owner only)
     * @param server WebServer instance
     * @return true if valid, false otherwise
     */
    static bool validateResetJWT(WebServerType& server);
    
    /**
     * @brief Send binary error response
     * @param server WebServer instance
     * @param code HTTP status code
     * @param message Error message
     */
    static void sendError(WebServerType& server, int code, const char* message);

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
    static void handleWirelessScan(WebServerType& server);
    static void handleSetUser(WebServerType& server);
    static void handleGPIOSet(WebServerType& server);
    static void handleGPIOGet(WebServerType& server);
    static void handleRestart(WebServerType& server);
    static void handleReset(WebServerType& server);

#if defined(ESP_CAM_HW_EXIST)
    /**
     * @brief PUT /api/camera/enable — enable or disable the camera on demand.
     *        Body: BinCameraEnableRequest (1 byte)
     */
    static void handleCameraEnable(WebServerType& server);
#endif
};

#endif // REQUEST_HANDLER_H
