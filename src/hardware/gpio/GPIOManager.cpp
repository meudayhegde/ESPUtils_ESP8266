#include "GPIOManager.h"
#include "../../utils/Utils.h"

void GPIOManager::begin() {
    Utils::printSerial(F("## Apply GPIO settings."));
    BinGpioSetResponse dummy;
    applyGPIO(-1, 0, 0, &dummy);
}

void GPIOManager::applyPinConfig(int pinNumber, uint8_t mode, int pinValue) {
    switch (mode) {
        case AP_GPIO_OUTPUT:
            pinMode(pinNumber, OUTPUT);
            digitalWrite(pinNumber, pinValue);
            break;
        case AP_GPIO_INPUT:
            pinMode(pinNumber, INPUT);
            break;
        case AP_GPIO_INPUT_PULLUP:
            pinMode(pinNumber, INPUT_PULLUP);
            break;
#ifdef INPUT_PULLDOWN
        case AP_GPIO_INPUT_PULLDOWN:
            pinMode(pinNumber, INPUT_PULLDOWN);
            break;
#endif
        default:
            break;
    }
}

void GPIOManager::applyGPIO(int pinNumber, uint8_t mode, int pinValue,
                            BinGpioSetResponse* resp) {
    Utils::printSerial(F("Applying GPIO settings..."));

    GPIOConfigData data;
    StorageManager::loadGPIOConfig(data); // ok if missing — count stays 0

    memset(resp, 0, sizeof(BinGpioSetResponse));
    bool pinConfigExists = false;
    int returnPinValue = -1;

    // Iterate through existing configs
    for (uint8_t i = 0; i < data.count; ++i) {
        GPIOConfig& cfg = data.pins[i];

        // Update settings for specific pin
        if (pinNumber != -1 && cfg.pinNumber == pinNumber) {
            pinConfigExists = true;
            cfg.mode = mode;

            // Toggle if pinValue is -1
            if (pinValue == -1) {
                cfg.pinValue = (cfg.pinValue == LOW) ? HIGH : LOW;
            } else {
                cfg.pinValue = pinValue;
            }
        }

        // Apply settings (either all if pinNumber == -1, or specific pin)
        if (pinNumber == -1 || cfg.pinNumber == pinNumber) {
            applyPinConfig(cfg.pinNumber, cfg.mode, cfg.pinValue);
            returnPinValue = cfg.pinValue;
        }
    }

    // Create new config entry if pin doesn't exist
    if (pinNumber != -1 && !pinConfigExists) {
        if (data.count < MAX_GPIO_PINS) {
            data.pins[data.count] = GPIOConfig(pinNumber, mode, pinValue);
            data.count++;

            applyPinConfig(pinNumber, mode, pinValue);
            returnPinValue = pinValue;
        } else {
            Utils::printSerial(F("Max GPIO configs reached."));
            resp->status = BIN_STATUS_ERROR;
            resp->pinValue = -1;
            strncpy(resp->error, "Max GPIO configs", sizeof(resp->error) - 1);
            return;
        }
    }

    // Save updated configuration
    if (pinNumber != -1) {
        if (!StorageManager::saveGPIOConfig(data)) {
            Utils::printSerial(F("Failed to save GPIO configuration."));
            resp->status = BIN_STATUS_ERROR;
            resp->pinValue = -1;
            strncpy(resp->error, "Failed to save config", sizeof(resp->error) - 1);
            return;
        }
    }

    resp->status = BIN_STATUS_OK;
    resp->pinValue = returnPinValue;
}

void GPIOManager::getGPIO(int pinNumber, BinGpioGetHeader* header,
                          BinGpioPin* pins) {
    GPIOConfigData data;
    StorageManager::loadGPIOConfig(data);

    header->status = BIN_STATUS_OK;

    if (pinNumber == -1) {
        // Return all pins
        header->count = data.count;
        for (uint8_t i = 0; i < data.count; ++i) {
            pins[i].pinNumber = data.pins[i].pinNumber;
            pins[i].pinMode   = data.pins[i].mode;
            pins[i].pinValue  = data.pins[i].pinValue;
        }
        return;
    }

    // Find specific pin
    for (uint8_t i = 0; i < data.count; ++i) {
        if (data.pins[i].pinNumber == pinNumber) {
            header->count = 1;
            pins[0].pinNumber = data.pins[i].pinNumber;
            pins[0].pinMode   = data.pins[i].mode;
            pins[0].pinValue  = data.pins[i].pinValue;
            return;
        }
    }

    // Pin not found — return empty
    header->count = 0;
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
