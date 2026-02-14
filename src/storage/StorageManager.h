#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "src/config/Config.h"

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
     * @return File contents as String, empty if failed
     */
    static String readFile(const char* filePath);
    
    /**
     * @brief Write string to file
     * @param filePath Path to the file
     * @param content Content to write
     * @return true if successful, false otherwise
     */
    static bool writeFile(const char* filePath, const String& content);
    
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
     * @brief Load user credentials from file
     * @param config UserConfig struct to populate
     * @return true if successful, false otherwise
     */
    static bool loadUserConfig(UserConfig& config);
    
    /**
     * @brief Save user credentials to file
     * @param config UserConfig struct to save
     * @return true if successful, false otherwise
     */
    static bool saveUserConfig(const UserConfig& config);
};

#endif // STORAGE_MANAGER_H
