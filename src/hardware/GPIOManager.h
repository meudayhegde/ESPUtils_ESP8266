#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "src/config/Config.h"
#include "src/storage/StorageManager.h"

class GPIOManager {
public:
    /**
     * @brief Initialize GPIO manager and apply stored settings
     */
    static void begin();
    
    /**
     * @brief Apply GPIO settings for a specific pin
     * @param pinNumber GPIO pin number (-1 to apply all stored settings)
     * @param pinMode Pin mode ("OUTPUT", "INPUT", etc.)
     * @param pinValue Pin value (HIGH/LOW, -1 to toggle)
     * @return JSON response string
     */
    static String applyGPIO(int pinNumber, const char* pinMode, int pinValue);
    
    /**
     * @brief Get GPIO configuration for a pin
     * @param pinNumber GPIO pin number (-1 to get all pins)
     * @return JSON response string
     */
    static String getGPIO(int pinNumber);
    
    /**
     * @brief Check if reset button is held for factory reset
     * @param pinNumber Pin number connected to reset button
     * @return true if factory reset triggered, false otherwise
     */
    static bool checkResetState(int pinNumber);
    
private:
    /**
     * @brief Apply pin configuration
     * @param pinNumber Pin number
     * @param pinMode Pin mode
     * @param pinValue Pin value
     */
    static void applyPinConfig(int pinNumber, const char* pinMode, int pinValue);
    
    /**
     * @brief Load GPIO configurations from storage
     * @return JsonDocument with GPIO configs
     */
    static JsonDocument loadGPIOConfigs();
    
    /**
     * @brief Save GPIO configurations to storage
     * @param doc JsonDocument with GPIO configs
     * @return true if successful, false otherwise
     */
    static bool saveGPIOConfigs(const JsonDocument& doc);
};

#endif // GPIO_MANAGER_H
