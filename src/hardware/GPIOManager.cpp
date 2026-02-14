#include "GPIOManager.h"
#include "src/utils/Utils.h"

void GPIOManager::begin() {
    Utils::printSerial(F("## Apply GPIO settings."));
    applyGPIO(-1, "", 0);
}

void GPIOManager::applyPinConfig(int pinNumber, const char* mode, int pinValue) {
    if (strcmp(mode, "OUTPUT") == 0) {
        pinMode(pinNumber, OUTPUT);
        digitalWrite(pinNumber, pinValue);
    } else if (strcmp(mode, "INPUT") == 0) {
        pinMode(pinNumber, INPUT);
    } else if (strcmp(mode, "INPUT_PULLUP") == 0) {
        pinMode(pinNumber, INPUT_PULLUP);
    }
}

JsonDocument GPIOManager::loadGPIOConfigs() {
    JsonDocument doc;
    
    File file = LittleFS.open(FPSTR_TO_CSTR(Config::GPIO_CONFIG_FILE), "r");
    if (!file) {
        Utils::printSerial(F("GPIO config file not found, using defaults."));
        // Return empty array
        doc.to<JsonArray>();
        return doc;
    }
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Utils::printSerial(F("Failed to parse GPIO config, using defaults."));
        doc.clear();
        doc.to<JsonArray>();
    }
    
    return doc;
}

bool GPIOManager::saveGPIOConfigs(const JsonDocument& doc) {
    return StorageManager::writeJson(Config::GPIO_CONFIG_FILE, doc);
}

String GPIOManager::applyGPIO(int pinNumber, const char* mode, int pinValue) {
    JsonDocument doc = loadGPIOConfigs();
    JsonArray array = doc.as<JsonArray>();
    
    bool pinConfigExists = false;
    int returnPinValue = -1;
    
    // Iterate through existing configs
    for (JsonVariant gpio : array) {
        int pin = gpio["pinNumber"];
        int currentValue = gpio["pinValue"];
        
        // Update settings for specific pin
        if (pinNumber != -1 && pin == pinNumber) {
            pinConfigExists = true;
            gpio["pinMode"] = mode;
            
            // Toggle if pinValue is -1
            if (pinValue == -1) {
                gpio["pinValue"] = (currentValue == LOW) ? HIGH : LOW;
            } else {
                gpio["pinValue"] = pinValue;
            }
        }
        
        // Apply settings (either all if pinNumber == -1, or specific pin)
        if (pinNumber == -1 || pin == pinNumber) {
            const char* pinMode = gpio["pinMode"];
            int value = gpio["pinValue"];
            
            applyPinConfig(pin, pinMode, value);
            returnPinValue = value;
        }
    }
    
    // Create new config entry if pin doesn't exist
    if (pinNumber != -1 && !pinConfigExists) {
        JsonObject newPin = array.createNestedObject();
        newPin["pinNumber"] = pinNumber;
        newPin["pinMode"] = mode;
        newPin["pinValue"] = pinValue;
        
        applyPinConfig(pinNumber, mode, pinValue);
        returnPinValue = pinValue;
    }
    
    // Save updated configuration
    if (pinNumber != -1) {
        if (!saveGPIOConfigs(doc)) {
            Utils::printSerial(F("Failed to save GPIO configuration."));
            return F("{\"response\":\"failure\",\"error\":\"Failed to save config\"}");
        }
    }
    
    char response[64];
    snprintf(response, sizeof(response), "{\"response\":\"success\",\"pinValue\":%d}", returnPinValue);
    return String(response);
}

String GPIOManager::getGPIO(int pinNumber) {
    if (pinNumber == -1) {
        // Return all GPIO configs
        String configs = StorageManager::readFile(FPSTR_TO_CSTR(Config::GPIO_CONFIG_FILE));
        return configs.length() > 0 ? configs : F("[]");
    }
    
    JsonDocument doc = loadGPIOConfigs();
    JsonArray array = doc.as<JsonArray>();
    
    // Find specific pin configuration
    for (JsonVariant gpio : array) {
        int pin = gpio["pinNumber"];
        
        if (pin == pinNumber) {
            String output;
            output.reserve(128); // Pre-allocate estimated size
            serializeJson(gpio, output);
            return output;
        }
    }
    
    return F("{}");
}

bool GPIOManager::checkResetState(int pinNumber) {
    pinMode(pinNumber, INPUT);
    int pinState = digitalRead(pinNumber);
    
    if (pinState != HIGH) {
        return false;
    }
    
    Utils::printSerial(F("Reset button clicked, will reset config if held for 10 seconds"));
    
    uint32_t startTime = millis();
    constexpr uint32_t RESET_DURATION_MS = 10000;
    
    while ((millis() - startTime) < RESET_DURATION_MS) {
        pinState = digitalRead(pinNumber);
        
        if (pinState == LOW) {
            Utils::printSerial(F("Reset cancelled."));
            return false;
        }
        
        delay(100);
    }
    
    Utils::printSerial(F("Confirmed, begin Reset..."));
    return true;
}
