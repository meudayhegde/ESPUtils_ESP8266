#include "StorageManager.h"
#include "../utils/Utils.h"

bool StorageManager::begin() {
    Utils::printSerial(F("## Begin flash storage."));
#ifdef ARDUINO_ARCH_ESP8266
    return LittleFS.begin();
#elif ARDUINO_ARCH_ESP32
    return LittleFS.begin(true);
#endif
}

bool StorageManager::readJson(const char* filePath, JsonDocument& doc) {
    Utils::printSerial(F("Reading File: "), "");
    Utils::printSerial(filePath, "...  ");
    
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Utils::printSerial(F(" Failed!"));
        return false;
    }
    
    Utils::printSerial(F(" Done."));
    Utils::printSerial(F("Parsing File...  "), "");
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Utils::printSerial(F("Failed!"));
        return false;
    }
    
    Utils::printSerial(F("Done."));
    return true;
}

bool StorageManager::writeJson(const char* filePath, const JsonDocument& doc) {
    Utils::printSerial(F("Writing File: "), "");
    Utils::printSerial(filePath, "...  ");
    
    // Remove existing file
    deleteFile(filePath);
    
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        Utils::printSerial(F(" Failed!"));
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        Utils::printSerial(F(" Failed!"));
        file.close();
        return false;
    }
    
    Utils::printSerial(F(" Done."));
    file.close();
    return true;
}

String StorageManager::readFile(const char* filePath) {
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return String("");
    }
    
    String content = file.readString();
    file.close();
    return content;
}

bool StorageManager::writeFile(const char* filePath, const String& content) {
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        return false;
    }
    
    size_t bytesWritten = file.print(content);
    file.close();
    return bytesWritten > 0;
}

bool StorageManager::deleteFile(const char* filePath) {
    if (fileExists(filePath)) {
        return LittleFS.remove(filePath);
    }
    return true;
}

bool StorageManager::fileExists(const char* filePath) {
    return LittleFS.exists(filePath);
}

bool StorageManager::format() {
    Utils::printSerial(F("Factory reset begin..."));
    
    if (!LittleFS.format()) {
        Utils::printSerial(F("Factory reset failed."));
        return false;
    }
    
    Utils::printSerial(F("Factory reset successful."));
    
    // Create empty GPIO config file
    Utils::printSerial(F("Creating File: "), "");
    Utils::printSerial(Config::GPIO_CONFIG_FILE, "...  ");
    if (!writeFile(Config::GPIO_CONFIG_FILE, "[]")) {
        Utils::printSerial(F("Failed!"));
        return false;
    }
    
    Utils::printSerial(F("Done."));
    return true;
}

bool StorageManager::loadWirelessConfig(WirelessConfig& config) {
    JsonDocument doc;
    
    if (!readJson(Config::WIFI_CONFIG_FILE, doc)) {
        // Use defaults
        return false;
    }
    
    config.mode = doc["mode"] | Config::DEFAULT_MODE;
    config.stationSSID = doc["wifi_name"] | Config::DEVICE_NAME;
    config.stationPSK = doc["wifi_pass"] | Config::DEVICE_PASSWORD;
    config.apSSID = doc["ap_name"] | Config::DEVICE_NAME;
    config.apPSK = doc["ap_pass"] | Config::DEVICE_PASSWORD;
    
    return true;
}

bool StorageManager::saveWirelessConfig(const WirelessConfig& config) {
    JsonDocument doc;
    
    doc["mode"] = config.mode;
    doc["wifi_name"] = config.stationSSID;
    doc["wifi_pass"] = config.stationPSK;
    doc["ap_name"] = config.apSSID;
    doc["ap_pass"] = config.apPSK;
    
    return writeJson(Config::WIFI_CONFIG_FILE, doc);
}
