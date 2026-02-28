#include "RequestHandler.h"

void ESPCommandHandler::setupRoutes(WebServerType& server) {
    Utils::printSerial(F("## Setting up HTTP REST API routes."));
    
    // Public endpoint - with LED indicator
    server.on("/ping", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handlePing); 
    });
    
    // Authentication endpoint - with LED indicator
    server.on("/api/auth", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleAuth); 
    });
    
    // Protected endpoints - with LED indicator
    server.on("/api/device", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleDeviceInfo); 
    });
    server.on("/api/ir/capture", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleIRCapture); 
    });
    server.on("/api/ir/send", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleIRSend); 
    });
    server.on("/api/wireless", HTTP_PUT, [&server]() { 
        withLEDIndicator(server, handleSetWireless); 
    });
    server.on("/api/wireless", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleGetWireless); 
    });

    server.on("/api/gpio/set", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleGPIOSet); 
    });
    server.on("/api/gpio/get", HTTP_GET, [&server]() { 
        withLEDIndicator(server, handleGPIOGet); 
    });
    server.on("/api/restart", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleRestart); 
    });
    server.on("/api/reset", HTTP_POST, [&server]() { 
        withLEDIndicator(server, handleReset); 
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

void ESPCommandHandler::sendError(WebServerType& server, int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    sendJsonResponse(server, code, doc);
}

void ESPCommandHandler::sendJsonResponse(WebServerType& server, int code, JsonDocument& doc) {
    // measureJson counts bytes without allocating; serializeJson streams directly
    // to the TCP client — no intermediate String ever touches the heap.
    size_t len = measureJson(doc);
    server.setContentLength(len);
    server.send(code, "application/json", "");
    serializeJson(doc, server.client());
}

void ESPCommandHandler::sendSuccess(WebServerType& server, const String& data) {
    server.send(200, "application/json", data);
}

// ================================
// Public Endpoints
// ================================

void ESPCommandHandler::handlePing(WebServerType& server) {
    Utils::printSerial(F("Handling /ping request"));
    
    JsonDocument doc;
    doc["deviceID"]   = Utils::getDeviceIDString();
    doc["ipAddress"]  = WirelessNetworkManager::getIPAddress();
    doc["deviceName"] = Config::DEVICE_NAME;
    doc["challenge"]  = SessionManager::getCurrentChallenge();
    
    sendJsonResponse(server, 200, doc);
}

// ================================
// Authentication Endpoint
// ================================

void ESPCommandHandler::handleAuth(WebServerType& server) {
    Utils::printSerial(F("Handling /api/auth request"));

    if (!server.hasArg("plain")) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }

    // Scope the request body + JsonDocument so they are freed from heap
    // before the crypto call.
    String jwtTokenStr;
    {
        String body = server.arg("plain");
        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            sendError(server, 400, ResponseMsg::JSON_ERROR);
            return;
        }
        const char* jwtToken = doc["token"] | "";
        if (strlen(jwtToken) == 0) {
            sendError(server, 400, "JWT token required");
            return;
        }
        jwtTokenStr = jwtToken;
    }  // body + doc freed here

    String sessionToken = AuthManager::authenticateWithJWT(
        jwtTokenStr.c_str(), jwtTokenStr.length());
    jwtTokenStr = "";  // free heap

    if (sessionToken.length() == 0) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }

    JsonDocument respDoc;
    respDoc["sessionToken"] = sessionToken;
    respDoc["expiresIn"]    = Config::SESSION_EXPIRY_SECONDS;
    sessionToken = "";
    sendJsonResponse(server, 200, respDoc);
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
    
    JsonDocument doc;
    doc["deviceName"]     = Config::DEVICE_NAME;
    doc["deviceID"]       = Utils::getDeviceIDString();
    doc["macAddress"]     = WirelessNetworkManager::getMacAddress();
    doc["ipAddress"]      = WirelessNetworkManager::getIPAddress();
    
#ifdef ARDUINO_ARCH_ESP8266
    doc["platform"]       = F("ESP8266");
    doc["deviceIDDecimal"] = (uint32_t)Utils::getDeviceID();
#elif ARDUINO_ARCH_ESP32
    doc["platform"]       = F("ESP32");
    doc["deviceIDDecimal"] = (uint32_t)(Utils::getDeviceID() & 0xFFFFFFFF);
#endif
    
    doc["wirelessMode"]   = WirelessNetworkManager::getWirelessConfig().mode;
    
    sendJsonResponse(server, 200, doc);
}

void ESPCommandHandler::handleIRCapture(WebServerType& server) {
    Utils::printSerial(F("Handling /api/ir/capture request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    const int captureMode = doc["captureMode"] | 0;
    
    Utils::setLED(LOW);
    
    // Note: IR capture might need streaming response
    // For now, using basic capture without client streaming
    WiFiClient dummyClient; // Placeholder
    String result = IRManager::captureIR(captureMode, dummyClient);
    
    Utils::setLED(HIGH);
    
    sendSuccess(server, result);
}

void ESPCommandHandler::handleIRSend(WebServerType& server) {
    Utils::printSerial(F("Handling /api/ir/send request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    const char* irData = doc["irCode"] | "";
    const char* lengthStr = doc["length"] | "0";
    const char* protocol = doc["protocol"] | "UNKNOWN";
    
    uint16_t size = strtol(lengthStr, NULL, 16);
    
    Utils::setLED(LOW);
    String result = IRManager::sendIR(size, protocol, irData);
    Utils::setLED(HIGH);
    
    sendSuccess(server, result);
}

void ESPCommandHandler::handleSetWireless(WebServerType& server) {
    Utils::printSerial(F("Handling /api/wireless PUT request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    WirelessConfig currentConfig = WirelessNetworkManager::getWirelessConfig();

    const char* wirelessMode = doc["wireless_mode"] | currentConfig.mode;
    const char* newSSID = doc["wifi_ssid"] | currentConfig.stationSSID;
    const char* newPass = doc["wifi_password"] | currentConfig.stationPSK;
    const char* apSsid = doc["ap_ssid"] | currentConfig.apSSID;
    const char* apPass = doc["ap_password"] | currentConfig.apPSK;

    WirelessConfig newConfig = currentConfig; // Start with current config as base
    
    strncpy(newConfig.mode,        wirelessMode, sizeof(newConfig.mode)        - 1); newConfig.mode[sizeof(newConfig.mode)               - 1] = '\0';
    strncpy(newConfig.stationSSID, newSSID,      sizeof(newConfig.stationSSID) - 1); newConfig.stationSSID[sizeof(newConfig.stationSSID) - 1] = '\0';
    strncpy(newConfig.stationPSK,  newPass,      sizeof(newConfig.stationPSK)  - 1); newConfig.stationPSK[sizeof(newConfig.stationPSK)   - 1] = '\0';
    strncpy(newConfig.apSSID,      apSsid,       sizeof(newConfig.apSSID)      - 1); newConfig.apSSID[sizeof(newConfig.apSSID)           - 1] = '\0';
    strncpy(newConfig.apPSK,       apPass,       sizeof(newConfig.apPSK)       - 1); newConfig.apPSK[sizeof(newConfig.apPSK)             - 1] = '\0';
    
    if (WirelessNetworkManager::updateWirelessConfig(newConfig)) {
        JsonDocument responseDoc;
        responseDoc["response"] = ResponseMsg::SUCCESS;
        responseDoc["message"]  = "Wireless config updated";
        responseDoc["wireless_mode"] = (strcmp(newConfig.mode, "AP_STA") == 0) ? "Range Extender" : (strcmp(newConfig.mode, "WIFI") == 0 ? "WiFi" : "Access Point");
        sendJsonResponse(server, 200, responseDoc);
    } else {
        sendError(server, 500, ResponseMsg::FAILURE);
    }
}

void ESPCommandHandler::handleGetWireless(WebServerType& server) {
    Utils::printSerial(F("Handling /api/wireless GET request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    String config = WirelessNetworkManager::getWirelessConfigJson();
    sendSuccess(server, config);
}

void ESPCommandHandler::handleGPIOSet(WebServerType& server) {
    Utils::printSerial(F("Handling /api/gpio/set request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, ResponseMsg::JSON_ERROR);
        return;
    }
    
    const int pinNumber = doc["pinNumber"] | -1;
    const char* pinMode = doc["pinMode"] | "";
    const int pinValue = doc["pinValue"] | 0;
    
    String result = GPIOManager::applyGPIO(pinNumber, pinMode, pinValue);
    
    sendSuccess(server, result);
}

void ESPCommandHandler::handleGPIOGet(WebServerType& server) {
    Utils::printSerial(F("Handling /api/gpio/get request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    if (!server.hasArg("pin")) {
        sendError(server, 400, "Pin number required");
        return;
    }
    
    int pinNumber = server.arg("pin").toInt();
    String result = GPIOManager::getGPIO(pinNumber);
    
    sendSuccess(server, result);
}

void ESPCommandHandler::handleRestart(WebServerType& server) {
    Utils::printSerial(F("Handling /api/restart request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    JsonDocument responseDoc;
    responseDoc["response"] = "restarting";
    sendJsonResponse(server, 200, responseDoc);
    
    delay(100);
    ESP.restart();
}

void ESPCommandHandler::handleReset(WebServerType& server) {
    Utils::printSerial(F("Handling /api/reset request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, ResponseMsg::UNAUTHORIZED);
        return;
    }
    
    bool success = StorageManager::format();
    
    if (success) {
        // Reset credentials and wireless to defaults
        WirelessConfig defaultConfig;
        StorageManager::saveWirelessConfig(defaultConfig);
        
        Utils::printSerial(F("Reset completed."));
    }
    
    JsonDocument responseDoc;
    responseDoc["response"] = success ? ResponseMsg::SUCCESS : ResponseMsg::FAILURE;
    sendJsonResponse(server, 200, responseDoc);
}
