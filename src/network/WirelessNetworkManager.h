#ifndef WIRELESS_NETWORK_MANAGER_H
#define WIRELESS_NETWORK_MANAGER_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
#endif
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "src/config/Config.h"
#include "src/storage/StorageManager.h"

class WirelessNetworkManager {
private:
    static WirelessConfig wirelessConfig;
    static WiFiServer socketServer;
    static WiFiUDP udp;
    static bool wirelessUpdatePending;
    static String cachedMacAddress;
    
public:
    /**
     * @brief Initialize network manager
     */
    static void begin();
    
    /**
     * @brief Initialize wireless connection (WiFi or AP mode)
     */
    static void initWireless();
    
    /**
     * @brief Handle UDP datagrams
     */
    static void handleDatagram();
    
    /**
     * @brief Handle TCP socket connections
     * @param requestHandler Callback function to handle requests
     * @return WiFiClient if connected, null client otherwise
     */
    static WiFiClient handleSocket();
    
    /**
     * @brief Update wireless configuration
     * @param mode "WIFI" or "AP"
     * @param ssid Network name
     * @param password Network password
     * @return true if successful, false otherwise
     */
    static bool updateWirelessConfig(const char* mode, const char* ssid, const char* password);
    
    /**
     * @brief Get current wireless configuration
     * @return WirelessConfig structure
     */
    static WirelessConfig getWirelessConfig();
    
    /**
     * @brief Get wireless configuration as JSON string
     * @return JSON string with wireless configuration
     */
    static String getWirelessConfigJson();
    
    /**
     * @brief Check if wireless update is pending
     * @return true if update pending, false otherwise
     */
    static bool isWirelessUpdatePending();
    
    /**
     * @brief Clear wireless update pending flag
     */
    static void clearWirelessUpdateFlag();
    
    /**
     * @brief Get MAC address (cached)
     * @return MAC address as String reference
     */
    static const String& getMacAddress();
    
    /**
     * @brief Get IP address
     * @return IP address as String
     */
    static String getIPAddress();
};

#endif // WIRELESS_NETWORK_MANAGER_H
