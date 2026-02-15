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
    server.on("/api/user", HTTP_PUT, [&server]() { 
        withLEDIndicator(server, handleSetUser); 
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
    
    String response;
    serializeJson(doc, response);
    
    server.send(code, "application/json", response);
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
    doc["deviceID"] = Utils::getDeviceIDString();
    doc["ipAddress"] = WirelessNetworkManager::getIPAddress();
    doc["deviceName"] = String(FPSTR(Config::DEVICE_NAME));
    
    String response;
    serializeJson(doc, response);
    
    sendSuccess(server, response);
}

// ================================
// Authentication Endpoint
// ================================

void ESPCommandHandler::handleAuth(WebServerType& server) {
    Utils::printSerial(F("Handling /api/auth request"));
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    const char* jwtToken = doc["token"] | "";
    
    if (strlen(jwtToken) == 0) {
        sendError(server, 400, "JWT token required");
        return;
    }
    
    // Authenticate with JWT and get session token
    String sessionToken = AuthManager::authenticateWithJWT(jwtToken);
    
    if (sessionToken.length() == 0) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    // Return session token
    JsonDocument responseDoc;
    responseDoc["sessionToken"] = sessionToken;
    responseDoc["expiresIn"] = Config::SESSION_EXPIRY_SECONDS;
    
    String response;
    serializeJson(responseDoc, response);
    
    sendSuccess(server, response);
}

// ================================
// Protected Endpoints
// ================================

void ESPCommandHandler::handleDeviceInfo(WebServerType& server) {
    Utils::printSerial(F("Handling /api/device request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    JsonDocument doc;
    doc["deviceName"] = String(FPSTR(Config::DEVICE_NAME));
    doc["deviceID"] = Utils::getDeviceIDString();
    doc["macAddress"] = WirelessNetworkManager::getMacAddress();
    doc["ipAddress"] = WirelessNetworkManager::getIPAddress();
    
#ifdef ARDUINO_ARCH_ESP8266
    doc["platform"] = F("ESP8266");
    doc["deviceIDDecimal"] = (uint32_t)Utils::getDeviceID();
#elif ARDUINO_ARCH_ESP32
    doc["platform"] = F("ESP32");
    doc["deviceIDDecimal"] = (uint32_t)(Utils::getDeviceID() & 0xFFFFFFFF);
#endif
    
    doc["wirelessMode"] = WirelessNetworkManager::getWirelessConfig().mode;
    
    String response;
    serializeJson(doc, response);
    
    sendSuccess(server, response);
}

void ESPCommandHandler::handleIRCapture(WebServerType& server) {
    Utils::printSerial(F("Handling /api/ir/capture request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
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
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
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
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    const char* wirelessMode = doc["mode"] | "AP";
    const char* newSSID = doc["ssid"] | "";
    const char* newPass = doc["password"] | "";
    
    WirelessConfig currentConfig = WirelessNetworkManager::getWirelessConfig();
    
    // Use current values if not provided
    if (strcmp(wirelessMode, "WIFI") == 0) {
        if (strlen(newSSID) == 0) {
            newSSID = currentConfig.stationSSID.c_str();
        }
        if (strlen(newPass) == 0) {
            newPass = currentConfig.stationPSK.c_str();
        }
    } else {
        if (strlen(newSSID) == 0) {
            newSSID = currentConfig.apSSID.c_str();
        }
        if (strlen(newPass) == 0) {
            newPass = currentConfig.apPSK.c_str();
        }
    }
    
    if (WirelessNetworkManager::updateWirelessConfig(wirelessMode, newSSID, newPass)) {
        JsonDocument responseDoc;
        responseDoc["response"] = FPSTR(ResponseMsg::SUCCESS);
        responseDoc["message"] = "Wireless config updated";
        
        String response;
        serializeJson(responseDoc, response);
        
        sendSuccess(server, response);
    } else {
        sendError(server, 500, FPSTR_TO_CSTR(ResponseMsg::FAILURE));
    }
}

void ESPCommandHandler::handleGetWireless(WebServerType& server) {
    Utils::printSerial(F("Handling /api/wireless GET request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    String config = WirelessNetworkManager::getWirelessConfigJson();
    sendSuccess(server, config);
}

void ESPCommandHandler::handleSetUser(WebServerType& server) {
    Utils::printSerial(F("Handling /api/user PUT request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    const char* newUsername = doc["username"] | "";
    const char* newPassword = doc["password"] | "";
    
    if (strlen(newUsername) == 0 || strlen(newPassword) == 0) {
        sendError(server, 400, "Invalid username or password");
        return;
    }
    
    if (AuthManager::updateCredentials(newUsername, newPassword)) {
        JsonDocument responseDoc;
        responseDoc["response"] = FPSTR(ResponseMsg::SUCCESS);
        responseDoc["message"] = "User credentials updated";
        
        String response;
        serializeJson(responseDoc, response);
        
        sendSuccess(server, response);
    } else {
        sendError(server, 500, FPSTR_TO_CSTR(ResponseMsg::FAILURE));
    }
}

void ESPCommandHandler::handleGPIOSet(WebServerType& server) {
    Utils::printSerial(F("Handling /api/gpio/set request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    if (!server.hasArg("plain")) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(server, 400, FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
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
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
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
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    JsonDocument responseDoc;
    responseDoc["response"] = "restarting";
    
    String response;
    serializeJson(responseDoc, response);
    
    sendSuccess(server, response);
    
    delay(100);
    ESP.restart();
}

void ESPCommandHandler::handleReset(WebServerType& server) {
    Utils::printSerial(F("Handling /api/reset request"));
    
    if (!validateSessionToken(server)) {
        sendError(server, 401, FPSTR_TO_CSTR(ResponseMsg::UNAUTHORIZED));
        return;
    }
    
    bool success = StorageManager::format();
    
    if (success) {
        // Reset credentials and wireless to defaults
        AuthManager::resetToDefault();
        
        WirelessConfig defaultConfig;
        StorageManager::saveWirelessConfig(defaultConfig);
        
        Utils::printSerial(F("Reset completed."));
    }
    
    JsonDocument responseDoc;
    responseDoc["response"] = success ? FPSTR(ResponseMsg::SUCCESS) : FPSTR(ResponseMsg::FAILURE);
    
    String response;
    serializeJson(responseDoc, response);
    
    sendSuccess(server, response);
}
