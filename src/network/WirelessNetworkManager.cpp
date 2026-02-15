#include "WirelessNetworkManager.h"
#include "src/utils/Utils.h"
#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266mDNS.h>
#elif ARDUINO_ARCH_ESP32
    #include <ESPmDNS.h>
#endif

WirelessConfig WirelessNetworkManager::wirelessConfig;
bool WirelessNetworkManager::wirelessUpdatePending = false;
String WirelessNetworkManager::cachedMacAddress;

void WirelessNetworkManager::begin() {
    Utils::printSerial(F("## Initialize Network Manager."));
    
    // Cache MAC address (it never changes)
    if (cachedMacAddress.length() == 0) {
        cachedMacAddress = WiFi.macAddress();
    }
}

void WirelessNetworkManager::initWireless() {
    Utils::printSerial(F("## Begin wireless network."));
    
    // Load wireless configuration
    if (!StorageManager::loadWirelessConfig(wirelessConfig)) {
        Utils::printSerial(F("Using default wireless configuration."));
    }
    
    const char* mode = wirelessConfig.mode.c_str();
    const char* wifiName = wirelessConfig.stationSSID.c_str();
    const char* wifiPassword = wirelessConfig.stationPSK.c_str();
    const char* apName = wirelessConfig.apSSID.c_str();
    const char* apPassword = wirelessConfig.apPSK.c_str();
    
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

bool WirelessNetworkManager::initMDNS(const String& deviceID) {
    Utils::printSerial(F("## Setting up mDNS responder..."));
    
    // Stop any existing mDNS instance
    #ifdef ARDUINO_ARCH_ESP32
        MDNS.end();
        delay(100);
    #endif
    
    // mDNS hostname will be: <deviceID>.local
    if (!MDNS.begin(deviceID.c_str())) {
        Utils::printSerial(F("Error setting up mDNS responder!"));
        return false;
    }
    
    Utils::printSerial(F("mDNS responder started: "), "");
    Utils::printSerial(deviceID.c_str(), ".local");
    Serial.println();
    
    // Add HTTP service with instance name
    MDNS.addService("http", "tcp", Config::HTTP_PORT);
    
    Utils::printSerial(F("HTTP service advertised via mDNS."));
    
    // Force initial announcement
    #ifdef ARDUINO_ARCH_ESP8266
        MDNS.announce();
    #endif
    
    return true;
}

bool WirelessNetworkManager::updateWirelessConfig(const char* mode, const char* ssid, const char* password) {
    if (!mode || !ssid || !password) {
        return false;
    }
    
    // Update appropriate fields based on mode
    if (strcmp(mode, "WIFI") == 0) {
        wirelessConfig.mode = F("WIFI");
        wirelessConfig.stationSSID = ssid;
        wirelessConfig.stationPSK = password;
    } else {
        wirelessConfig.mode = F("AP");
        wirelessConfig.apSSID = ssid;
        wirelessConfig.apPSK = password;
    }
    
    // Save to storage
    if (!StorageManager::saveWirelessConfig(wirelessConfig)) {
        Utils::printSerial(F("Failed to save wireless configuration."));
        return false;
    }
    
    wirelessUpdatePending = true;
    Utils::printSerial(F("Wireless configuration updated successfully."));
    return true;
}

WirelessConfig WirelessNetworkManager::getWirelessConfig() {
    return wirelessConfig;
}

String WirelessNetworkManager::getWirelessConfigJson() {
    JsonDocument doc;
    
    doc["wireless_mode"] = wirelessConfig.mode;
    doc["station_ssid"] = wirelessConfig.stationSSID;
    doc["station_psk"] = wirelessConfig.stationPSK;
    doc["ap_ssid"] = wirelessConfig.apSSID;
    doc["ap_psk"] = wirelessConfig.apPSK;
    
    String result;
    result.reserve(256); // Pre-allocate estimated size
    serializeJson(doc, result);
    return result;
}

bool WirelessNetworkManager::isWirelessUpdatePending() {
    return wirelessUpdatePending;
}

void WirelessNetworkManager::clearWirelessUpdateFlag() {
    wirelessUpdatePending = false;
}

const String& WirelessNetworkManager::getMacAddress() {
    return cachedMacAddress;
}

String WirelessNetworkManager::getIPAddress() {
    if (WiFi.getMode() == WIFI_STA) {
        return WiFi.localIP().toString();
    } else {
        return WiFi.softAPIP().toString();
    }
}
