#include "RequestHandler.h"

String RequestHandler::handleRequest(const String& request, WiFiClient& client) {
    JsonDocument doc;
    
    Utils::printSerial(F("Parsing request from socket client..."), "  ");
    DeserializationError error = deserializeJson(doc, request.c_str());
    
    if (error) {
        Utils::printSerial(F("Failed."));
        char response[64];
        snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::JSON_ERROR));
        return String(response);
    }
    
    Utils::printSerial(F("Done."));
    
    const char* requestType = doc["request"] | "";
    
    Utils::printSerial(F("Incoming request: "), "");
    Utils::printSerial(requestType);
    
    // Route request to appropriate handler
    if (strlen(requestType) == 0 || strcmp(requestType, "undefined") == 0) {
        char response[64];
        snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::PURPOSE_NOT_DEFINED));
        return String(response);
    }
    else if (strcmp(requestType, "ping") == 0) {
        return handlePing();
    }
    else if (strcmp(requestType, "device_info") == 0) {
        return handleDeviceInfo();
    }
    else if (strcmp(requestType, "authenticate") == 0) {
        return handleAuthenticate(doc);
    }
    else if (strcmp(requestType, "ir_capture") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleIRCapture(doc, client);
    }
    else if (strcmp(requestType, "ir_send") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleIRSend(doc);
    }
    else if (strcmp(requestType, "set_wireless") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleSetWireless(doc);
    }
    else if (strcmp(requestType, "set_user") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleSetUser(doc);
    }
    else if (strcmp(requestType, "get_wireless") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleGetWireless();
    }
    else if (strcmp(requestType, "gpio_set") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleGPIOSet(doc);
    }
    else if (strcmp(requestType, "gpio_get") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleGPIOGet(doc);
    }
    else if (strcmp(requestType, "restart") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        handleRestart();
        return F("{\"response\":\"restarting\"}");
    }
    else if (strcmp(requestType, "reset") == 0) {
        if (!verifyAuth(doc)) {
            char response[32];
            snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
            return String(response);
        }
        return handleReset();
    }
    else {
        char response[64];
        snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::INVALID_PURPOSE));
        return String(response);
    }
}

String RequestHandler::handlePing() {
    char response[128];
    snprintf(response, sizeof(response), "{\"MAC\":\"%s\",\"chipID\":\"%s\"}", 
             WirelessNetworkManager::getMacAddress().c_str(), 
             Utils::getChipIDString().c_str());
    return String(response);
}

String RequestHandler::handleDeviceInfo() {
    return buildDeviceInfoJson();
}

String RequestHandler::handleAuthenticate(const JsonDocument& doc) {
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    
    if (AuthManager::authenticate(username, password)) {
        char response[64];
        snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::AUTHENTICATED));
        return String(response);
    }
    
    char response[32];
    snprintf(response, sizeof(response), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::DENY));
    return String(response);
}

String RequestHandler::handleIRCapture(const JsonDocument& doc, WiFiClient& client) {
    const int captureMode = doc["capture_mode"] | 0;
    return IRManager::captureIR(captureMode, client);
}

String RequestHandler::handleIRSend(const JsonDocument& doc) {
    const char* irData = doc["irCode"] | "";
    const char* lengthStr = doc["length"] | "0";
    const char* protocol = doc["protocol"] | "UNKNOWN";
    
    uint16_t size = strtol(lengthStr, NULL, 16);
    
    Utils::setLED(LOW);
    String result = IRManager::sendIR(size, protocol, irData);
    Utils::setLED(HIGH);
    
    return result;
}

String RequestHandler::handleSetWireless(const JsonDocument& doc) {
    const char* wirelessMode = doc["wireless_mode"] | "AP";
    const char* newSSID = doc["new_ssid"] | "";
    const char* newPass = doc["new_pass"] | "";
    
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
        return F("{\"response\":\"Wireless config successfully applied\"}");
    }
    
    return F("{\"response\":\"Config update failed\"}");
}

String RequestHandler::handleSetUser(const JsonDocument& doc) {
    const char* newUsername = doc["new_username"] | "";
    const char* newPassword = doc["new_password"] | "";
    
    if (strlen(newUsername) == 0 || strlen(newPassword) == 0) {
        return F("{\"response\":\"Invalid username or password\"}");
    }
    
    if (AuthManager::updateCredentials(newUsername, newPassword)) {
        return F("{\"response\":\"User config successfully applied\"}");
    }
    
    return F("{\"response\":\"User config update failed\"}");
}

String RequestHandler::handleGetWireless() {
    return WirelessNetworkManager::getWirelessConfigJson();
}

String RequestHandler::handleGPIOSet(const JsonDocument& doc) {
    const int pinNumber = doc["pinNumber"] | -1;
    const char* pinMode = doc["pinMode"] | "";
    const int pinValue = doc["pinValue"] | 0;
    
    return GPIOManager::applyGPIO(pinNumber, pinMode, pinValue);
}

String RequestHandler::handleGPIOGet(const JsonDocument& doc) {
    const int pinNumber = doc["pinNumber"] | -1;
    return GPIOManager::getGPIO(pinNumber);
}

void RequestHandler::handleRestart() {
    Utils::printSerial(F("Restarting device..."));
    delay(100);
    ESP.restart();
}

String RequestHandler::handleReset() {
    bool success = StorageManager::format();
    
    if (success) {
        // Reset credentials and wireless to defaults
        AuthManager::resetToDefault();
        
        WirelessConfig defaultConfig;
        StorageManager::saveWirelessConfig(defaultConfig);
        
        Utils::printSerial(F("Reset completed."));
    }
    
    char response[32];
    snprintf(response, sizeof(response), "{\"response\":\"%s\"}", 
             success ? FPSTR_TO_CSTR(ResponseMsg::SUCCESS) : FPSTR_TO_CSTR(ResponseMsg::FAILURE));
    return String(response);
}

bool RequestHandler::verifyAuth(const JsonDocument& doc) {
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    
    return AuthManager::authenticate(username, password);
}

String RequestHandler::buildDeviceInfoJson() {
    JsonDocument doc;
    
    doc["device_name"] = String(FPSTR(Config::DEVICE_NAME));
    doc["chip_id"] = Utils::getChipIDString();
    doc["mac_address"] = WirelessNetworkManager::getMacAddress();
    doc["ip_address"] = WirelessNetworkManager::getIPAddress();
    
#ifdef ARDUINO_ARCH_ESP8266
    doc["platform"] = F("ESP8266");
    doc["chip_id_decimal"] = (uint32_t)Utils::getChipID();
#elif ARDUINO_ARCH_ESP32
    doc["platform"] = F("ESP32");
    doc["chip_id_decimal"] = (uint32_t)(Utils::getChipID() & 0xFFFFFFFF);
#endif
    
    doc["wireless_mode"] = WirelessNetworkManager::getWirelessConfig().mode;
    
    String response;
    response.reserve(256); // Pre-allocate estimated size
    serializeJson(doc, response);
    return response;
}
