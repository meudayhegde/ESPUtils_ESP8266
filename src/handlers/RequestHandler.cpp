#include "RequestHandler.h"
#include "ResponseHelper.h"
#include "BinaryHelper.h"

#if defined(ESP_CAM_HW_EXIST)
#include "CameraHandler.h"
#endif

// ArduinoJson is only needed for JWT payload parsing inside AuthManager.
// No JSON is used in request/response bodies any more.

// ── Rate limiter state ────────────────────────────────────────────────────────
uint8_t       ESPCommandHandler::_rlCount       = 0;
unsigned long ESPCommandHandler::_rlWindowStart = 0;

void ESPCommandHandler::setupRoutes(WebServerType& server) {
    Utils::printSerial(F("\n## Setting up HTTP REST API routes."));

    // The ESP32 WebServer only stores request headers that are explicitly
    // registered with collectHeaders() before the first request arrives.
    // Without this, server.hasHeader("Authorization") always returns false.
    const char* headersToCollect[] = { "Authorization", "Content-Type" };
    server.collectHeaders(headersToCollect, 2);
    
    // Public endpoint - with LED indicator
    server.on("/ping", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handlePing); 
    });
    
    // Authentication endpoint - with LED indicator
    server.on("/api/auth", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleAuth); 
    }, rawBodyStub);
    
    // Protected endpoints - with LED indicator
    server.on("/api/device", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleDeviceInfo); 
    });
    server.on("/api/ir/capture", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleIRCapture); 
    }, rawBodyStub);
    server.on("/api/ir/send", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleIRSend); 
    }, rawBodyStub);
    server.on("/api/wireless", HTTP_PUT, [&server]() { 
        withLEDIndicator(server, handleSetWireless); 
    }, rawBodyStub);
    server.on("/api/wireless", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleGetWireless); 
    });
    server.on("/api/wireless/scan", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleWirelessScan); 
    });

    server.on("/api/gpio/set", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleGPIOSet); 
    }, rawBodyStub);
    server.on("/api/gpio/get", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleGPIOGet); 
    });
    server.on("/api/restart", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleRestart); 
    }, rawBodyStub);
    server.on("/api/reset", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleReset); 
    }, rawBodyStub);

#if defined(ESP_CAM_HW_EXIST)
    server.on("/api/camera/enable", HTTP_PUT, [&server]() {
        withLEDIndicator(server, handleCameraEnable);
    }, rawBodyStub);
#endif

    // ── CORS preflight handler ────────────────────────────────────────────
    // Respond to OPTIONS on any path so the browser allows cross-origin
    // requests from the separately-hosted frontend.
    server.onNotFound([&server]() {
        if (server.method() == HTTP_OPTIONS) {
            handleCorsOptions(server);
        } else {
            sendError(server, 404, "Not found");
        }
    });

    Utils::printSerial(F("HTTP REST API routes configured with LED middleware."));
}

bool ESPCommandHandler::validateSessionToken(WebServerType& server) {
    // Check for Authorization header
    if (!server.hasHeader("Authorization")) {
        Utils::printSerial(F("Missing Authorization header."));
        return false;
    }
    
    String authHeader = server.header("Authorization");
    Utils::printSerial(F("Authorization header: "), authHeader.c_str());

    // Check format: "Session <token>"
    if (!authHeader.startsWith("Session ")) {
        Utils::printSerial(F("Invalid Authorization header format."));
        return false;
    }
    
    // Extract token
    String sessionToken = authHeader.substring(8); // Skip "Session "
    
    // Validate session
    if (!AuthManager::validateSession(sessionToken)) {
        Utils::printSerial(F("Invalid or expired session token."));
        return false;
    }
    
    return true;
}

bool ESPCommandHandler::validateResetJWT(WebServerType& server) {
    // Check for Authorization header
    if (!server.hasHeader("Authorization")) {
        Utils::printSerial(F("Missing Authorization header for reset."));
        return false;
    }
    
    String authHeader = server.header("Authorization");
    Utils::printSerial(F("Reset Authorization header: "), authHeader.c_str());

    // Check format: "Bearer <jwt_token>"
    if (!authHeader.startsWith("Bearer ")) {
        Utils::printSerial(F("Invalid Authorization header format for reset. Expected 'Bearer'."));
        return false;
    }
    
    // Extract JWT token
    String jwtToken = authHeader.substring(7); // Skip "Bearer "
    
    // Validate JWT for reset using AuthManager
    return AuthManager::validateResetJWT(jwtToken.c_str(), jwtToken.length());
}

bool ESPCommandHandler::checkWiFiAuth(WebServerType& server) {
    // If device is not bound to an identity yet, allow WiFi configuration without auth
    if (!SessionManager::hasBoundSub()) {
        Utils::printSerial(F("Device not bound - allowing WiFi configuration without auth"));
        return true;
    }
    
    // Device is bound - require authentication
    return validateSessionToken(server);
}

bool ESPCommandHandler::checkRateLimit(WebServerType& server) {
    unsigned long now = millis();

    // Reset the counter when the window has expired
    if (now - _rlWindowStart >= Config::RATE_LIMIT_WINDOW_MS) {
        _rlWindowStart = now;
        _rlCount       = 0;
    }

    if (_rlCount >= Config::RATE_LIMIT_MAX_REQUESTS) {
        // Tell the client how many seconds remain in the current window
        unsigned long remaining = (Config::RATE_LIMIT_WINDOW_MS - (now - _rlWindowStart) + 999UL) / 1000UL;
        char retryAfter[4];
        snprintf(retryAfter, sizeof(retryAfter), "%u", (unsigned)remaining);
        server.sendHeader("Retry-After", retryAfter);
        sendError(server, 429, "Rate limit exceeded");
        return false;
    }

    _rlCount++;
    return true;
}

void ESPCommandHandler::sendError(WebServerType& server, int code, const char* message) {
    uint8_t binStatus = BIN_STATUS_ERROR;
    if (code == 401) binStatus = BIN_STATUS_UNAUTHORIZED;
    sendBinaryError(server, code, binStatus, message);
}

// ================================
// Public Endpoints
// ================================

void ESPCommandHandler::handlePing(WebServerType& server) {
    Utils::printSerial(F("Handling /ping request"));

    BinPingResponse resp;
    memset(&resp, 0, sizeof(resp));
    copyToField(resp.deviceID,      Utils::getDeviceIDString(), sizeof(resp.deviceID));
    copyToField(resp.ipAddress,     WirelessNetworkManager::getIPAddress(), sizeof(resp.ipAddress));
    copyToField(resp.deviceName,    Config::DEVICE_NAME, sizeof(resp.deviceName));
    copyToField(resp.challenge,     SessionManager::getCurrentChallenge(), sizeof(resp.challenge));
    resp.isBound = SessionManager::hasBoundSub() ? 1 : 0;
    copyToField(resp.platform_name, PLATFORM_NAME, sizeof(resp.platform_name));
    copyToField(resp.platform_key,  PLATFORM_KEY, sizeof(resp.platform_key));

    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

// ================================
// Authentication Endpoint
// ================================

void ESPCommandHandler::handleAuth(WebServerType& server) {
    Utils::printSerial(F("Handling /api/auth request"));

    BinAuthRequest req;
    if (readBinaryBody(server, &req, sizeof(req)) == 0) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }

    // Ensure NUL-terminated
    req.token[sizeof(req.token) - 1] = '\0';
    size_t tokenLen = strlen(req.token);
    if (tokenLen == 0) {
        sendError(server, 400, "JWT token required");
        return;
    }

    String sessionToken = AuthManager::authenticateWithJWT(req.token, tokenLen);

    BinAuthResponse resp;
    memset(&resp, 0, sizeof(resp));

    if (sessionToken.length() == 0) {
        resp.status = BIN_STATUS_UNAUTHORIZED;
        copyToField(resp.error, ResponseMsg::UNAUTHORIZED, sizeof(resp.error));
    } else {
        resp.status = BIN_STATUS_OK;
        copyToField(resp.sessionToken, sessionToken.c_str(), sizeof(resp.sessionToken));
        resp.expiresIn = Config::SESSION_EXPIRY_SECONDS;
    }

    sendBinaryResponse(server, (resp.status == BIN_STATUS_OK) ? 200 : 401,
                       &resp, sizeof(resp));
}

// ================================
// Protected Endpoints
// ================================

void ESPCommandHandler::handleDeviceInfo(WebServerType& server) {
    Utils::printSerial(F("Handling /api/device request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinDeviceInfoResponse resp;
    memset(&resp, 0, sizeof(resp));
    copyToField(resp.deviceName, Config::DEVICE_NAME, sizeof(resp.deviceName));
    copyToField(resp.deviceID,   Utils::getDeviceIDString(), sizeof(resp.deviceID));
    copyToField(resp.macAddress, WirelessNetworkManager::getMacAddress(), sizeof(resp.macAddress));
    copyToField(resp.ipAddress,  WirelessNetworkManager::getIPAddress(), sizeof(resp.ipAddress));

#if defined(ARDUINO_ARCH_ESP8266)
    copyToField(resp.platform, "ESP8266", sizeof(resp.platform));
    resp.deviceIDDecimal = (uint32_t)Utils::getDeviceID();
#elif defined(ARDUINO_ARCH_ESP32)
    copyToField(resp.platform, "ESP32", sizeof(resp.platform));
    resp.deviceIDDecimal = (uint32_t)(Utils::getDeviceID() & 0xFFFFFFFF);
#endif
    copyToField(resp.platform_name, PLATFORM_NAME, sizeof(resp.platform_name));
    copyToField(resp.platform_key,  PLATFORM_KEY,  sizeof(resp.platform_key));
    copyToField(resp.wirelessMode,  WirelessNetworkManager::getWirelessConfig().mode, sizeof(resp.wirelessMode));
    resp.isBound = SessionManager::hasBoundSub() ? 1 : 0;

    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

void ESPCommandHandler::handleIRCapture(WebServerType& server) {
    Utils::printSerial(F("\nHandling /api/ir/capture request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinIrCaptureRequest req;
    if (readBinaryBody(server, &req, sizeof(req)) == 0) {
        sendError(server, 400, "Capture mode required");
        return;
    }

    Utils::setLED(LOW);
    // IRManager handles the full SSE response internally; do NOT send any response after.
    IRManager::captureIR(req.captureMode, server);
    Utils::setLED(HIGH);
}

void ESPCommandHandler::handleIRSend(WebServerType& server) {
    Utils::printSerial(F("Handling /api/ir/send request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    // Variable-length: BinIrSendHeader followed by irCode data
    if (!server.hasArg("plain")) {
        sendError(server, 400, "IR send data required");
        return;
    }

    String body = server.arg("plain");
    if (body.length() < sizeof(BinIrSendHeader)) {
        sendError(server, 400, "IR send data too short");
        return;
    }

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(body.c_str());
    BinIrSendHeader hdr;
    memcpy(&hdr, raw, sizeof(hdr));
    hdr.protocol[sizeof(hdr.protocol) - 1] = '\0';

    // Validate irCodeLen against actual body length
    size_t expectedTotal = sizeof(BinIrSendHeader) + hdr.irCodeLen;
    if (body.length() < expectedTotal) {
        sendError(server, 400, "IR code data incomplete");
        return;
    }

    // irCode data follows the header (NUL-terminated hex string)
    static char irCodeBuf[2048];
    size_t copyLen = (hdr.irCodeLen < sizeof(irCodeBuf) - 1) ? hdr.irCodeLen : (sizeof(irCodeBuf) - 1);
    memcpy(irCodeBuf, raw + sizeof(BinIrSendHeader), copyLen);
    irCodeBuf[copyLen] = '\0';

    BinIrSendResponse resp;
    memset(&resp, 0, sizeof(resp));

    Utils::setLED(LOW);
    IRManager::sendIR(hdr.protocol, hdr.bitLength, irCodeBuf, (uint16_t)copyLen, &resp);
    Utils::setLED(HIGH);

    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

void ESPCommandHandler::handleSetWireless(WebServerType& server) {
    Utils::printSerial(F("\nHandling /api/wireless PUT request"));

    if (!checkWiFiAuth(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinWirelessSetRequest req;
    if (readBinaryBody(server, &req, sizeof(req)) == 0) {
        sendError(server, 400, "Wireless config data required");
        return;
    }

    // Ensure NUL-termination on every field
    req.wireless_mode[sizeof(req.wireless_mode) - 1] = '\0';
    req.wifi_ssid[sizeof(req.wifi_ssid) - 1]         = '\0';
    req.wifi_password[sizeof(req.wifi_password) - 1]  = '\0';
    req.ap_ssid[sizeof(req.ap_ssid) - 1]             = '\0';
    req.ap_password[sizeof(req.ap_password) - 1]      = '\0';

    WirelessConfig currentConfig = WirelessNetworkManager::getWirelessConfig();
    WirelessConfig newConfig = currentConfig;

    // Use request fields if non-empty, else keep current
    const char* wirelessMode = (req.wireless_mode[0] != '\0') ? req.wireless_mode : currentConfig.mode;
    const char* newSSID      = (req.wifi_ssid[0] != '\0')     ? req.wifi_ssid     : currentConfig.stationSSID;
    const char* newPass      = (req.wifi_password[0] != '\0') ? req.wifi_password  : currentConfig.stationPSK;
    const char* apSsid       = (req.ap_ssid[0] != '\0')      ? req.ap_ssid       : currentConfig.apSSID;
    const char* apPass       = (req.ap_password[0] != '\0')   ? req.ap_password    : currentConfig.apPSK;

    strncpy(newConfig.mode,        wirelessMode, sizeof(newConfig.mode)        - 1); newConfig.mode[sizeof(newConfig.mode)               - 1] = '\0';
    strncpy(newConfig.stationSSID, newSSID,      sizeof(newConfig.stationSSID) - 1); newConfig.stationSSID[sizeof(newConfig.stationSSID) - 1] = '\0';
    strncpy(newConfig.stationPSK,  newPass,      sizeof(newConfig.stationPSK)  - 1); newConfig.stationPSK[sizeof(newConfig.stationPSK)   - 1] = '\0';
    strncpy(newConfig.apSSID,      apSsid,       sizeof(newConfig.apSSID)      - 1); newConfig.apSSID[sizeof(newConfig.apSSID)           - 1] = '\0';
    strncpy(newConfig.apPSK,       apPass,       sizeof(newConfig.apPSK)       - 1); newConfig.apPSK[sizeof(newConfig.apPSK)             - 1] = '\0';

    BinWirelessSetResponse resp;
    memset(&resp, 0, sizeof(resp));

    if (WirelessNetworkManager::updateWirelessConfig(newConfig)) {
        resp.status = BIN_STATUS_OK;
        copyToField(resp.message, "Wireless config updated", sizeof(resp.message));
        // Human-readable mode name
        if (strcmp(newConfig.mode, "AP_STA") == 0)
            copyToField(resp.wireless_mode, "AP_STA", sizeof(resp.wireless_mode));
        else if (strcmp(newConfig.mode, "WIFI") == 0)
            copyToField(resp.wireless_mode, "WIFI", sizeof(resp.wireless_mode));
        else
            copyToField(resp.wireless_mode, "AP", sizeof(resp.wireless_mode));
    } else {
        resp.status = BIN_STATUS_ERROR;
        copyToField(resp.message, ResponseMsg::FAILURE, sizeof(resp.message));
    }

    sendBinaryResponse(server, (resp.status == BIN_STATUS_OK) ? 200 : 500,
                       &resp, sizeof(resp));
}

void ESPCommandHandler::handleGetWireless(WebServerType& server) {
    Utils::printSerial(F("\nHandling /api/wireless GET request"));

    if (!checkWiFiAuth(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinWirelessGetResponse resp;
    memset(&resp, 0, sizeof(resp));
    WirelessNetworkManager::getWirelessConfigBinary(&resp);
    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

void ESPCommandHandler::handleWirelessScan(WebServerType& server) {
    Utils::printSerial(F("\nHandling /api/wireless/scan request"));

    if (!checkWiFiAuth(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    int8_t result = WirelessNetworkManager::getScanResult();

    // WIFI_SCAN_RUNNING (-1): scan in progress — report back immediately
    if (result == WIFI_SCAN_RUNNING) {
        BinWifiScanHeader hdr;
        hdr.status = BIN_STATUS_SCANNING;
        hdr.count  = 0;
        sendBinaryResponse(server, 202, &hdr, sizeof(hdr));
        return;
    }

    // WIFI_SCAN_FAILED (-2): no scan active or previous scan failed — start one
    if (result == WIFI_SCAN_FAILED) {
        WirelessNetworkManager::startScan();
        BinWifiScanHeader hdr;
        hdr.status = BIN_STATUS_SCANNING;
        hdr.count  = 0;
        sendBinaryResponse(server, 202, &hdr, sizeof(hdr));
        return;
    }

    // result >= 0: scan complete, result is the number of networks found
    uint8_t netCount = (result > 255) ? 255 : (uint8_t)result;

    // Build response: header + array of BinNetworkInfo
    size_t totalLen = sizeof(BinWifiScanHeader) + netCount * sizeof(BinNetworkInfo);
    uint8_t* buf = new uint8_t[totalLen];
    if (!buf) {
        sendError(server, 500, "Out of memory");
        WirelessNetworkManager::clearScan();
        return;
    }
    memset(buf, 0, totalLen);

    BinWifiScanHeader* hdr = reinterpret_cast<BinWifiScanHeader*>(buf);
    hdr->status = BIN_STATUS_OK;
    hdr->count  = netCount;

    BinNetworkInfo* networks = reinterpret_cast<BinNetworkInfo*>(buf + sizeof(BinWifiScanHeader));
    for (uint8_t i = 0; i < netCount; i++) {
        copyToField(networks[i].ssid, WiFi.SSID(i).c_str(), sizeof(networks[i].ssid));

        uint8_t* bssid = WiFi.BSSID(i);
        char bssidBuf[18];
        snprintf(bssidBuf, sizeof(bssidBuf),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        copyToField(networks[i].bssid, bssidBuf, sizeof(networks[i].bssid));

        networks[i].rssi = WiFi.RSSI(i);
#if defined(ARDUINO_ARCH_ESP8266)
        networks[i].encrypted = (WiFi.encryptionType(i) != ENC_TYPE_NONE) ? 1 : 0;
#else
        networks[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
#endif
    }

    // Free scan memory before sending — heap pressure on ESP8266 is real
    WirelessNetworkManager::clearScan();

    sendBinaryResponse(server, 200, buf, totalLen);
    delete[] buf;
}

void ESPCommandHandler::handleGPIOSet(WebServerType& server) {
    Utils::printSerial(F("Handling /api/gpio/set request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinGpioSetRequest req;
    if (readBinaryBody(server, &req, sizeof(req)) == 0) {
        sendError(server, 400, "GPIO data required");
        return;
    }

    BinGpioSetResponse resp;
    memset(&resp, 0, sizeof(resp));
    GPIOManager::applyGPIO(req.pinNumber, req.pinMode, req.pinValue, &resp);

    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

void ESPCommandHandler::handleGPIOGet(WebServerType& server) {
    Utils::printSerial(F("Handling /api/gpio/get request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    int pinNumber = -1;
    if (server.hasArg("pin") && server.arg("pin").length() > 0) {
        pinNumber = server.arg("pin").toInt();
    }

    BinGpioGetHeader hdr;
    BinGpioPin pins[MAX_GPIO_PINS];
    memset(&hdr, 0, sizeof(hdr));
    memset(pins, 0, sizeof(pins));

    GPIOManager::getGPIO(pinNumber, &hdr, pins);

    size_t totalLen = sizeof(hdr) + hdr.count * sizeof(BinGpioPin);
    uint8_t* buf = new uint8_t[totalLen];
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), pins, hdr.count * sizeof(BinGpioPin));

    sendBinaryResponse(server, 200, buf, totalLen);
    delete[] buf;
}

void ESPCommandHandler::handleRestart(WebServerType& server) {
    Utils::printSerial(F("Handling /api/restart request"));

    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    BinSimpleResponse resp;
    resp.status = BIN_STATUS_RESTARTING;
    sendBinaryResponse(server, 200, &resp, sizeof(resp));

    delay(100);
    ESP.restart();
}

void ESPCommandHandler::handleReset(WebServerType& server) {
    Utils::printSerial(F("Handling /api/reset request"));

    if (!validateResetJWT(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    bool success = StorageManager::format();

    if (success) {
        // Reset credentials and wireless to defaults
        WirelessConfig defaultConfig;
        StorageManager::saveWirelessConfig(defaultConfig);

        // Invalidate all sessions and clear bound identity since device is being reset
        SessionManager::invalidateAllSessions();
        SessionManager::setBoundSub(""); // Clear bound sub in memory

        Utils::printSerial(F("Reset completed. Device is now unbound."));
    }

    BinSimpleResponse resp;
    resp.status = success ? BIN_STATUS_OK : BIN_STATUS_ERROR;
    sendBinaryResponse(server, 200, &resp, sizeof(resp));
}

#if defined(ESP_CAM_HW_EXIST)
// Track whether the stream server has been started (deferred from boot).
static bool _streamServerStarted = false;

void ESPCommandHandler::handleCameraEnable(WebServerType& server) {
    Utils::printSerial(F("Handling /api/camera/enable request"));

    if (!validateSessionToken(server)) {
        sendBinaryError(server, 401, BIN_STATUS_ERROR, "Unauthorized");
        return;
    }

    BinCameraEnableRequest req;
    if (readBinaryBody(server, &req, sizeof(req)) == 0) {
        sendBinaryError(server, 400, BIN_STATUS_ERROR, "Camera enable data required");
        return;
    }

    bool enable  = (req.enabled != 0);
    bool success = true;

    if (enable) {
        success = CameraManager::enable();
        // Start the MJPEG stream server the first time the camera is enabled.
        if (success && !_streamServerStarted) {
            CameraHandler::startStreamServer();
            _streamServerStarted = true;
        }
    } else {
        CameraManager::disable();
    }

    BinCameraEnableResponse resp;
    resp.status  = success ? BIN_STATUS_OK : BIN_STATUS_ERROR;
    resp.enabled = CameraManager::isEnabled() ? 1 : 0;

    sendBinaryResponse(server, success ? 200 : 500, &resp, sizeof(resp));
}
#endif
