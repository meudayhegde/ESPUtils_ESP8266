#include "WirelessNetworkManager.h"
#include "../utils/Utils.h"
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266mDNS.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <ESPmDNS.h>
#endif

WirelessConfig WirelessNetworkManager::wirelessConfig;
bool           WirelessNetworkManager::wirelessUpdatePending = false;
char           WirelessNetworkManager::cachedMacAddress[18]  = {};

void WirelessNetworkManager::begin() {
    Utils::printSerial(F("## Initialize Network Manager."));

    // Cache MAC address once — it never changes after boot
    if (cachedMacAddress[0] == '\0') {
        WiFi.macAddress().toCharArray(cachedMacAddress, sizeof(cachedMacAddress));
    }
}

void WirelessNetworkManager::initWireless() {
    Utils::printSerial(F("## Begin wireless network."));
    
    // Load wireless configuration
    if (!StorageManager::loadWirelessConfig(wirelessConfig)) {
        Utils::printSerial(F("Using default wireless configuration."));
    }
    
    const char* mode = wirelessConfig.mode;
    const char* wifiName = wirelessConfig.stationSSID;
    const char* wifiPassword = wirelessConfig.stationPSK;
    const char* apName = wirelessConfig.apSSID;
    const char* apPassword = wirelessConfig.apPSK;
    
    Utils::printSerial(F("Wireless mode:"), F(""));
    Utils::printSerial(mode);
    // Try WiFi Station mode
    if (strcmp(mode, "WIFI") == 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiName, wifiPassword);
        
        Utils::printSerial(F("Connecting to WiFi network \""), "");
        Utils::printSerial(wifiName, "\"");
        Serial.println();
        
        // Wait for connection with timeout
        for (int i = 0; i < Config::WIRELESS_TIMEOUT_SEC * 2; i++) {
            Utils::setLED(LOW);
            
            if (WiFi.status() == WL_CONNECTED) {
                Utils::printSerial(F("WiFi Connection established..."));
                Utils::printSerial(F("IP Address: "), "");
                if (Config::SERIAL_MONITOR_ENABLED) {
                    Serial.println(WiFi.localIP());
                }
                // Wait for network stack to stabilize (important for mDNS)
                delay(500);
                Utils::setLED(HIGH);
                delay(1500);
                return;
            }
            
            Utils::printSerial(F("."), "");
            delay(50);
            Utils::setLED(HIGH);
            delay(450);
        }
        
        Utils::printSerial(F("."));
        Utils::printSerial(F("WiFi connection timeout."));
    }
    
    // Range Extender: STA connected to router + NATed soft-AP
    if (strcmp(mode, "AP_STA") == 0) {
        initRangeExtender();
        return;
    }

    // Fall back to AP mode or if mode is already AP
    WiFi.mode(WIFI_AP);
    Utils::printSerial(F("Beginning SoftAP \""), "");
    Utils::printSerial(apName, "\"");
    Serial.println();
    WiFi.softAP(apName, apPassword, 1, 0, 5);
    
    Utils::printSerial(F("IP Address: "), "");
    if (Config::SERIAL_MONITOR_ENABLED) {
        Serial.println(WiFi.softAPIP());
    }
    
    Utils::ledPulse(1000, 2000, 3);
}

// ================================
// Range Extender
// ================================

#ifdef ARDUINO_ARCH_ESP32
void WirelessNetworkManager::onRangeExtenderEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Utils::printSerial(F("[RangeExt] STA got IP — enabling NAPT on AP."));
            WiFi.AP.enableNAPT(true);
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Utils::printSerial(F("[RangeExt] STA disconnected — disabling NAPT."));
            WiFi.AP.enableNAPT(false);
            break;
        default:
            break;
    }
}
#endif

void WirelessNetworkManager::initRangeExtender() {
    Utils::printSerial(F("## Initializing Range Extender (STA + NATed AP)."));

    const char* staSsid = wirelessConfig.stationSSID;
    const char* staPsk  = wirelessConfig.stationPSK;
    const char* apSsid  = wirelessConfig.apSSID;
    const char* apPsk   = wirelessConfig.apPSK;

#if defined(ARDUINO_ARCH_ESP8266) && defined(RANGE_EXTENDER_NAPT_SUPPORTED)
    // ----- ESP8266 approach: lwIP NAPT -----

    // Step 1: Connect STA to the upstream router first so we can read the
    //         real DNS server before the DHCP server is configured.
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSsid, staPsk);

    Utils::printSerial(F("[RangeExt] Connecting STA to \""), "");
    Utils::printSerial(staSsid, "\"...");
    Serial.println();

    bool staConnected = false;
    for (int i = 0; i < Config::WIRELESS_TIMEOUT_SEC * 2; i++) {
        Utils::setLED(LOW);
        if (WiFi.status() == WL_CONNECTED) {
            staConnected = true;
            break;
        }
        Utils::printSerial(F("."), "");
        delay(50);
        Utils::setLED(HIGH);
        delay(450);
    }
    Serial.println();

    if (!staConnected) {
        Utils::printSerial(F("[RangeExt] STA connection timed out — falling back to AP mode."));
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSsid, apPsk, 1, 0, 5);
        Utils::ledPulse(1000, 2000, 3);
        return;
    }

    Utils::printSerial(F("[RangeExt] STA connected. IP: "), "");
    if (Config::SERIAL_MONITOR_ENABLED) Serial.println(WiFi.localIP());

    // Step 2: Point the soft-AP DHCP server at the upstream DNS so that
    //         devices connected to the extender get real name resolution.
    auto& dhcp = WiFi.softAPDhcpServer();
    dhcp.setDns(WiFi.dnsIP(0));

    // Step 3: Bring up the soft-AP (WIFI_AP_STA mode is set implicitly).
    WiFi.softAPConfig(
        IPAddress(Config::RANGE_EXT_AP_IP[0],     Config::RANGE_EXT_AP_IP[1],
                  Config::RANGE_EXT_AP_IP[2],     Config::RANGE_EXT_AP_IP[3]),
        IPAddress(Config::RANGE_EXT_AP_IP[0],     Config::RANGE_EXT_AP_IP[1],
                  Config::RANGE_EXT_AP_IP[2],     Config::RANGE_EXT_AP_IP[3]),
        IPAddress(Config::RANGE_EXT_AP_SUBNET[0], Config::RANGE_EXT_AP_SUBNET[1],
                  Config::RANGE_EXT_AP_SUBNET[2], Config::RANGE_EXT_AP_SUBNET[3])
    );
    WiFi.softAP(apSsid, apPsk);

    Utils::printSerial(F("[RangeExt] Soft-AP started. AP IP: "), "");
    if (Config::SERIAL_MONITOR_ENABLED) Serial.println(WiFi.softAPIP());

    // Step 4: Initialise NAPT and enable it on the soft-AP interface.
    Utils::printSerial(F("[RangeExt] Initialising NAPT..."));
    err_t ret = ip_napt_init(Config::NAPT_TABLE_SIZE, Config::NAPT_PORT_TABLE_SIZE);
    if (ret == ERR_OK) {
        ret = ip_napt_enable_no(SOFTAP_IF, 1);
    }

    if (ret == ERR_OK) {
        Utils::printSerial(F("[RangeExt] NAPT enabled. Network '"), "");
        Utils::printSerial(apSsid, "' is now NATed behind '");
        Utils::printSerial(staSsid, "'.");
        Serial.println();
    } else {
        Utils::printSerial(F("[RangeExt] NAPT initialisation failed — repeater will not forward traffic."));
    }

    Utils::setLED(HIGH);

#elif defined(ARDUINO_ARCH_ESP32)
    // ----- ESP32 approach: WiFi.AP.enableNAPT via event callback -----

    // Register for WiFi events before starting anything.
    WiFi.onEvent(onRangeExtenderEvent);

    // Configure and start the soft-AP side.
    WiFi.AP.begin();
    WiFi.AP.config(
        IPAddress(Config::RANGE_EXT32_AP_IP[0],    Config::RANGE_EXT32_AP_IP[1],
                  Config::RANGE_EXT32_AP_IP[2],    Config::RANGE_EXT32_AP_IP[3]),
        IPAddress(Config::RANGE_EXT32_AP_IP[0],    Config::RANGE_EXT32_AP_IP[1],
                  Config::RANGE_EXT32_AP_IP[2],    Config::RANGE_EXT32_AP_IP[3]),
        IPAddress(255, 255, 255, 0),
        IPAddress(Config::RANGE_EXT32_AP_LEASE[0], Config::RANGE_EXT32_AP_LEASE[1],
                  Config::RANGE_EXT32_AP_LEASE[2], Config::RANGE_EXT32_AP_LEASE[3]),
        IPAddress(Config::RANGE_EXT32_AP_DNS[0],   Config::RANGE_EXT32_AP_DNS[1],
                  Config::RANGE_EXT32_AP_DNS[2],   Config::RANGE_EXT32_AP_DNS[3])
    );
    WiFi.AP.create(apSsid, apPsk);

    if (!WiFi.AP.waitStatusBits(ESP_NETIF_STARTED_BIT, 1000)) {
        Utils::printSerial(F("[RangeExt] Soft-AP failed to start!"));
    } else {
        Utils::printSerial(F("[RangeExt] Soft-AP started. AP IP: "), "");
        if (Config::SERIAL_MONITOR_ENABLED) Serial.println(WiFi.softAPIP());
    }

    // Connect STA — NAPT will be enabled in the event handler once IP is obtained.
    Utils::printSerial(F("[RangeExt] Connecting STA to \""), "");
    Utils::printSerial(staSsid, "\"...");
    Serial.println();
    WiFi.begin(staSsid, staPsk);

    // Wait for connection with a visible indicator.
    bool staConnected = false;
    for (int i = 0; i < Config::WIRELESS_TIMEOUT_SEC * 2; i++) {
        Utils::setLED(LOW);
        if (WiFi.status() == WL_CONNECTED) {
            staConnected = true;
            break;
        }
        Utils::printSerial(F("."), "");
        delay(50);
        Utils::setLED(HIGH);
        delay(450);
    }
    Serial.println();

    if (staConnected) {
        Utils::printSerial(F("[RangeExt] STA connected. IP: "), "");
        if (Config::SERIAL_MONITOR_ENABLED) Serial.println(WiFi.localIP());
    } else {
        Utils::printSerial(F("[RangeExt] STA connection timed out — AP is up but internet will not be forwarded."));
    }

    Utils::setLED(HIGH);

#else
    // Platform does not support NAPT — fall back to a plain AP
    Utils::printSerial(F("[RangeExt] NAPT not supported on this build. Starting plain AP."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPsk, 1, 0, 5);
    Utils::ledPulse(1000, 2000, 3);
#endif
}

bool WirelessNetworkManager::initMDNS(const char* deviceID) {
    Utils::printSerial(F("## Setting up mDNS responder..."));

    // Stop any existing mDNS instance
    #if defined(ARDUINO_ARCH_ESP32)
        MDNS.end();
        delay(100);
    #endif

    // mDNS hostname will be: <deviceID>.local
    if (!MDNS.begin(deviceID)) {
        Utils::printSerial(F("Error setting up mDNS responder!"));
        return false;
    }

    Utils::printSerial(F("mDNS responder started: "), "");
    Utils::printSerial(deviceID, ".local");
    Serial.println();

    // Add HTTP service with instance name
    MDNS.addService("http", "tcp", Config::HTTP_PORT);

    Utils::printSerial(F("HTTP service advertised via mDNS."));

    // Force initial announcement
    #if defined(ARDUINO_ARCH_ESP8266)
        MDNS.announce();
    #endif

    return true;
}

bool WirelessNetworkManager::updateWirelessConfig(const WirelessConfig& config) {
    wirelessConfig = config;

    if (!StorageManager::saveWirelessConfig(wirelessConfig)) {
        Utils::printSerial(F("Failed to save wireless configuration."));
        return false;
    }

    wirelessUpdatePending = true;
    Utils::printSerial(F("Wireless configuration updated successfully."));
    return true;
}

const WirelessConfig& WirelessNetworkManager::getWirelessConfig() {
    return wirelessConfig;
}

const char* WirelessNetworkManager::getWirelessConfigJson() {
    // Build JSON once into a static buffer — no heap allocation.
    // Passwords are masked for security; only mode and SSIDs are exposed.
    static char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"wireless_mode\":\"%s\",\"station_ssid\":\"%s\","
        "\"station_psk\":\"****\",\"ap_ssid\":\"%s\",\"ap_psk\":\"****\"}",
        wirelessConfig.mode,
        wirelessConfig.stationSSID,
        wirelessConfig.apSSID
    );
    return buf;
}

void WirelessNetworkManager::getWirelessConfigBinary(BinWirelessGetResponse* out) {
    memset(out, 0, sizeof(BinWirelessGetResponse));
    strncpy(out->wireless_mode, wirelessConfig.mode, sizeof(out->wireless_mode) - 1);
    strncpy(out->station_ssid, wirelessConfig.stationSSID, sizeof(out->station_ssid) - 1);
    strncpy(out->station_psk, "****", sizeof(out->station_psk) - 1);
    strncpy(out->ap_ssid, wirelessConfig.apSSID, sizeof(out->ap_ssid) - 1);
    strncpy(out->ap_psk, "****", sizeof(out->ap_psk) - 1);
}

bool WirelessNetworkManager::isWirelessUpdatePending() {
    return wirelessUpdatePending;
}

void WirelessNetworkManager::clearWirelessUpdateFlag() {
    wirelessUpdatePending = false;
}

const char* WirelessNetworkManager::getMacAddress() {
    return cachedMacAddress;
}

const char* WirelessNetworkManager::getIPAddress() {
    static char buf[64]; // large enough for dual-mode "A.B.C.D (STA) / E.F.G.H (AP)"
    WiFiMode_t wifiMode = WiFi.getMode();
    if (wifiMode == WIFI_STA) {
        WiFi.localIP().toString().toCharArray(buf, sizeof(buf));
    } else if (wifiMode == WIFI_AP_STA) {
        // Range extender: report both IPs
        String ips = WiFi.localIP().toString();
        ips += F(" (STA) / ");
        ips += WiFi.softAPIP().toString();
        ips += F(" (AP)");
        ips.toCharArray(buf, sizeof(buf));
    } else {
        WiFi.softAPIP().toString().toCharArray(buf, sizeof(buf));
    }
    return buf;
}

// ================================
// WiFi Scanning (non-blocking)
// ================================

void WirelessNetworkManager::startScan() {
    Utils::printSerial(F("Starting async WiFi scan."));
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
}

int8_t WirelessNetworkManager::getScanResult() {
    return static_cast<int8_t>(WiFi.scanComplete());
}

void WirelessNetworkManager::clearScan() {
    WiFi.scanDelete();
}
