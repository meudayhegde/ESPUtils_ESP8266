#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <Arduino.h>
#include "../../config/Config.h"
#include "../../storage/StorageManager.h"
#include "../../protocol/BinaryProtocol.h"

class GPIOManager {
public:
    /**
     * @brief Initialize GPIO manager and apply stored settings
     */
    static void begin();
    
    /**
     * @brief Apply GPIO settings for a specific pin (binary response)
     * @param pinNumber GPIO pin number (-1 to apply all stored settings)
     * @param mode      Pin mode (GPIOPinMode enum value: 0-3)
     * @param pinValue  Pin value (HIGH/LOW, -1 to toggle)
     * @param resp      Pointer to BinGpioSetResponse to fill
     */
    static void applyGPIO(int pinNumber, uint8_t mode, int pinValue,
                          BinGpioSetResponse* resp);

    /**
     * @brief Get GPIO configuration for a pin (binary response)
     * @param pinNumber GPIO pin number (-1 to get all pins)
     * @param header    Pointer to BinGpioGetHeader to fill
     * @param pins      Pointer to BinGpioPin array to fill (max MAX_GPIO_PINS)
     */
    static void getGPIO(int pinNumber, BinGpioGetHeader* header,
                        BinGpioPin* pins);
    
    /**
     * @brief Check if reset button is held for factory reset
     * @param pinNumber Pin number connected to reset button
     * @return true if factory reset triggered, false otherwise
     */
    static bool checkResetState(int pinNumber);
    
private:
    /**
     * @brief Apply pin configuration (maps enum to Arduino constant)
     * @param pinNumber Pin number
     * @param mode      GPIOPinMode enum value
     * @param pinValue  Pin value
     */
    static void applyPinConfig(int pinNumber, uint8_t mode, int pinValue);
};

#endif // GPIO_MANAGER_H
