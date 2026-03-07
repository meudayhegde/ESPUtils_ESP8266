#include "StorageManager.h"
#include "../utils/Utils.h"

bool StorageManager::begin() {
    Utils::printSerial(F("## Begin flash storage."));
#if defined(ARDUINO_ARCH_ESP8266)
    if (LittleFS.begin()) {
        return true;
    }
    // Filesystem not formatted (e.g. first boot after flash) — format and retry
    Utils::printSerial(F("LittleFS mount failed, formatting..."));
    LittleFS.format();
    return LittleFS.begin();
#elif defined(ARDUINO_ARCH_ESP32)
    return LittleFS.begin(true); // true = format on failure
#else
    return false;
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

bool StorageManager::writeFile(const char* filePath, const char* content) {
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
    
    // Create empty GPIO config file (binary)
    Utils::printSerial(F("Creating File: "), "");
    Utils::printSerial(Config::GPIO_CONFIG_FILE, "...  ");
    GPIOConfigData emptyGPIO;
    if (!saveGPIOConfig(emptyGPIO)) {
        Utils::printSerial(F("Failed!"));
        return false;
    }
    
    Utils::printSerial(F("Done."));
    return true;
}

bool StorageManager::loadWirelessConfig(WirelessConfig& config) {
    File file = LittleFS.open(Config::WIFI_CONFIG_FILE, "r");
    if (!file) {
        Utils::printSerial(F("No wireless config file — using defaults."));
        return false;
    }

    if (file.size() != sizeof(WirelessConfig)) {
        Utils::printSerial(F("Wireless config size mismatch — using defaults."));
        file.close();
        return false;
    }

    bool ok = (file.read(reinterpret_cast<uint8_t*>(&config), sizeof(WirelessConfig)) == sizeof(WirelessConfig));
    file.close();

    // Ensure null-termination in case of corrupt data
    config.mode[sizeof(config.mode) - 1]               = '\0';
    config.stationSSID[sizeof(config.stationSSID) - 1] = '\0';
    config.stationPSK[sizeof(config.stationPSK) - 1]   = '\0';
    config.apSSID[sizeof(config.apSSID) - 1]           = '\0';
    config.apPSK[sizeof(config.apPSK) - 1]             = '\0';

    if (ok) {
        Utils::printSerial(F("Wireless config loaded."));
    } else {
        Utils::printSerial(F("Wireless config read failed — using defaults."));
    }
    return ok;
}

bool StorageManager::saveWirelessConfig(const WirelessConfig& config) {
    deleteFile(Config::WIFI_CONFIG_FILE);

    File file = LittleFS.open(Config::WIFI_CONFIG_FILE, "w");
    if (!file) {
        Utils::printSerial(F("Failed to open wireless config for write."));
        return false;
    }

    bool ok = (file.write(reinterpret_cast<const uint8_t*>(&config), sizeof(WirelessConfig)) == sizeof(WirelessConfig));
    file.close();

    if (ok) {
        Utils::printSerial(F("Wireless config saved."));
    } else {
        Utils::printSerial(F("Wireless config write failed."));
    }
    return ok;
}

bool StorageManager::loadBoundToken(BoundTokenData& data) {
    File file = LittleFS.open(Config::BOUND_TOKEN_FILE, "r");
    if (!file) {
        Utils::printSerial(F("\nNo bound token file found."));
        return false;
    }

    if (file.size() != sizeof(BoundTokenData)) {
        Utils::printSerial(F("\nBound token size mismatch — ignoring."));
        file.close();
        return false;
    }

    bool ok = (file.read(reinterpret_cast<uint8_t*>(&data), sizeof(BoundTokenData)) == sizeof(BoundTokenData));
    file.close();

    // Ensure null-termination in case of corrupt data
    data.sub[sizeof(data.sub) - 1] = '\0';
    data.jwt[sizeof(data.jwt) - 1] = '\0';

    if (ok) {
        Utils::printSerial(F("\nBound token loaded."));
    } else {
        Utils::printSerial(F("\nBound token read failed."));
    }
    return ok;
}

bool StorageManager::saveBoundToken(const BoundTokenData& data) {
    deleteFile(Config::BOUND_TOKEN_FILE);

    File file = LittleFS.open(Config::BOUND_TOKEN_FILE, "w");
    if (!file) {
        Utils::printSerial(F("\nFailed to open bound token for write."));
        return false;
    }

    bool ok = (file.write(reinterpret_cast<const uint8_t*>(&data), sizeof(BoundTokenData)) == sizeof(BoundTokenData));
    file.close();

    if (ok) {
        Utils::printSerial(F("\nBound token saved."));
    } else {
        Utils::printSerial(F("\nBound token write failed."));
    }
    return ok;
}

bool StorageManager::loadGPIOConfig(GPIOConfigData& data) {
    File file = LittleFS.open(Config::GPIO_CONFIG_FILE, "r");
    if (!file) {
        Utils::printSerial(F("No GPIO config file — using defaults."));
        return false;
    }

    if (file.size() != sizeof(GPIOConfigData)) {
        Utils::printSerial(F("GPIO config size mismatch — using defaults."));
        file.close();
        return false;
    }

    bool ok = (file.read(reinterpret_cast<uint8_t*>(&data), sizeof(GPIOConfigData)) == sizeof(GPIOConfigData));
    file.close();

    // Clamp count to valid range
    if (data.count > MAX_GPIO_PINS) {
        data.count = 0;
    }

    if (ok) {
        Utils::printSerial(F("GPIO config loaded."));
    } else {
        Utils::printSerial(F("GPIO config read failed — using defaults."));
    }
    return ok;
}

bool StorageManager::saveGPIOConfig(const GPIOConfigData& data) {
    deleteFile(Config::GPIO_CONFIG_FILE);

    File file = LittleFS.open(Config::GPIO_CONFIG_FILE, "w");
    if (!file) {
        Utils::printSerial(F("Failed to open GPIO config for write."));
        return false;
    }

    bool ok = (file.write(reinterpret_cast<const uint8_t*>(&data), sizeof(GPIOConfigData)) == sizeof(GPIOConfigData));
    file.close();

    if (ok) {
        Utils::printSerial(F("GPIO config saved."));
    } else {
        Utils::printSerial(F("GPIO config write failed."));
    }
    return ok;
}
