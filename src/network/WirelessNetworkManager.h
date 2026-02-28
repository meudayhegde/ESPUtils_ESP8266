#ifndef WIRELESS_NETWORK_MANAGER_H
#define WIRELESS_NETWORK_MANAGER_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
    // NAPT (network-address-and-port-translation) for range-extender mode.
    // Only available when lwIP is built with LWIP_FEATURES and without IPv6.
    #if LWIP_FEATURES && !LWIP_IPV6
        #define RANGE_EXTENDER_NAPT_SUPPORTED 1
        #include <lwip/napt.h>
        #include <lwip/dns.h>
    #endif
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
#endif
#include "../config/Config.h"
#include "../storage/StorageManager.h"

class WirelessNetworkManager {
private:
    static WirelessConfig wirelessConfig;
    static bool wirelessUpdatePending;
    static String cachedMacAddress;

    /**
     * @brief Initialize Range Extender mode (STA + NATed soft-AP)
     */
    static void initRangeExtender();

#ifdef ARDUINO_ARCH_ESP32
    /**
     * @brief WiFi event handler used by the range-extender on ESP32
     */
    static void onRangeExtenderEvent(arduino_event_id_t event, arduino_event_info_t info);
#endif

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
     * @brief Initialize mDNS with device ID
     * @param deviceID Device ID for mDNS hostname
     * @return true if successful, false otherwise
     */
    static bool initMDNS(const String& deviceID);
    
    /**
     * @brief Update wireless configuration with full config (required for AP_STA)
     * @param config Complete WirelessConfig to persist and apply
     * @return true if successful, false otherwise
     */
    static bool updateWirelessConfig(const WirelessConfig& config);
    
    /**
     * @brief Get current wireless configuration
     * @return const reference to the in-memory WirelessConfig
     */
    static const WirelessConfig& getWirelessConfig();
    
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
