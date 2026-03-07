#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config/Config.h"

class StorageManager {
public:
    /**
     * @brief Initialize LittleFS file system
     * @return true if successful, false otherwise
     */
    static bool begin();
    
    /**
     * @brief Read JSON document from file
     * @param filePath Path to the file
     * @param doc JsonDocument to store the result
     * @return true if successful, false otherwise
     */
    static bool readJson(const char* filePath, JsonDocument& doc);
    
    /**
     * @brief Write JSON document to file
     * @param filePath Path to the file
     * @param doc JsonDocument to write
     * @return true if successful, false otherwise
     */
    static bool writeJson(const char* filePath, const JsonDocument& doc);
    
    /**
     * @brief Read entire file as string
     * @param filePath Path to the file
     * @return File contents as String, empty String if failed
     */
    static String readFile(const char* filePath);

    /**
     * @brief Write a C-string to a file (creates or overwrites)
     * @param filePath Path to the file
     * @param content  Null-terminated content to write
     * @return true if successful, false otherwise
     */
    static bool writeFile(const char* filePath, const char* content);
    
    /**
     * @brief Delete a file
     * @param filePath Path to the file
     * @return true if successful, false otherwise
     */
    static bool deleteFile(const char* filePath);
    
    /**
     * @brief Check if file exists
     * @param filePath Path to the file
     * @return true if file exists, false otherwise
     */
    static bool fileExists(const char* filePath);
    
    /**
     * @brief Format the file system (factory reset)
     * @return true if successful, false otherwise
     */
    static bool format();
    
    /**
     * @brief Load wireless configuration from file
     * @param config WirelessConfig struct to populate
     * @return true if successful, false otherwise
     */
    static bool loadWirelessConfig(WirelessConfig& config);
    
    /**
     * @brief Save wireless configuration to file
     * @param config WirelessConfig struct to save
     * @return true if successful, false otherwise
     */
    static bool saveWirelessConfig(const WirelessConfig& config);

    /**
     * @brief Load bound token data from binary file
     * @param data BoundTokenData struct to populate
     * @return true if successful, false otherwise
     */
    static bool loadBoundToken(BoundTokenData& data);

    /**
     * @brief Save bound token data to binary file
     * @param data BoundTokenData struct to save
     * @return true if successful, false otherwise
     */
    static bool saveBoundToken(const BoundTokenData& data);

    /**
     * @brief Load GPIO configuration from binary file
     * @param data GPIOConfigData struct to populate
     * @return true if successful, false otherwise
     */
    static bool loadGPIOConfig(GPIOConfigData& data);

    /**
     * @brief Save GPIO configuration to binary file
     * @param data GPIOConfigData struct to save
     * @return true if successful, false otherwise
     */
    static bool saveGPIOConfig(const GPIOConfigData& data);

    /**
     * @brief Load sleep mode enabled flag from flash.
     * @param enabled  Output: true if sleep mode was previously enabled.
     * @return true if a persisted value was found, false if no file exists.
     */
    static bool loadSleepEnabled(bool& enabled);

    /**
     * @brief Save sleep mode enabled flag to flash.
     * @param enabled  true to enable sleep mode, false to disable.
     * @return true if successful, false otherwise.
     */
    static bool saveSleepEnabled(bool enabled);
};

#endif // STORAGE_MANAGER_H
