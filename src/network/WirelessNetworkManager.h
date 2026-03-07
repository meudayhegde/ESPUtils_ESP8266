#ifndef WIRELESS_NETWORK_MANAGER_H
#define WIRELESS_NETWORK_MANAGER_H

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266WiFi.h>
    // NAPT (network-address-and-port-translation) for range-extender mode.
    // Only available when lwIP is built with LWIP_FEATURES and without IPv6.
    #if LWIP_FEATURES && !LWIP_IPV6
        #define RANGE_EXTENDER_NAPT_SUPPORTED 1
        #include <lwip/napt.h>
        #include <lwip/dns.h>
    #endif
#elif defined(ARDUINO_ARCH_ESP32)
    #include <WiFi.h>
#endif
#include "../config/Config.h"
#include "../storage/StorageManager.h"
#include "../protocol/BinaryProtocol.h"

class WirelessNetworkManager {
private:
    static WirelessConfig wirelessConfig;
    static bool wirelessUpdatePending;
    static char cachedMacAddress[18]; // "XX:XX:XX:XX:XX:XX" + NUL (17 chars + 1)

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
     * @param deviceID Device ID for mDNS hostname (C-string)
     * @return true if successful, false otherwise
     */
    static bool initMDNS(const char* deviceID);
    
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
     * @return Pointer to a static buffer with JSON (overwritten on next call)
     */
    static const char* getWirelessConfigJson();

    /**
     * @brief Get wireless configuration as binary struct
     * @param out Pointer to BinWirelessGetResponse to fill
     */
    static void getWirelessConfigBinary(BinWirelessGetResponse* out);
    
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
     * @brief Get MAC address (cached, never changes after boot)
     * @return Pointer to a static 18-byte buffer "XX:XX:XX:XX:XX:XX"
     */
    static const char* getMacAddress();

    /**
     * @brief Get current IP address
     * @return Pointer to a static buffer (overwritten on next call)
     */
    static const char* getIPAddress();

    // ================================
    // WiFi Scanning (non-blocking)
    // ================================

    /**
     * @brief Start an asynchronous WiFi network scan.
     *        Returns immediately; poll getScanResult() to check progress.
     */
    static void startScan();

    /**
     * @brief Poll scan progress / retrieve result count.
     * @return WIFI_SCAN_RUNNING (-1) while scanning,
     *         WIFI_SCAN_FAILED  (-2) if not started or failed,
     *         count >= 0 when complete.
     */
    static int8_t getScanResult();

    /**
     * @brief Free the memory allocated by the last scan.
     *        Must be called after consuming scan results.
     */
    static void clearScan();
};

#endif // WIRELESS_NETWORK_MANAGER_H
