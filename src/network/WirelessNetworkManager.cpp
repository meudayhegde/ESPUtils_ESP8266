#include "WirelessNetworkManager.h"
#include "src/utils/Utils.h"

WirelessConfig WirelessNetworkManager::wirelessConfig;
WiFiServer WirelessNetworkManager::socketServer(Config::SOCKET_PORT);
WiFiUDP WirelessNetworkManager::udp;
bool WirelessNetworkManager::wirelessUpdatePending = false;
String WirelessNetworkManager::cachedMacAddress;

void WirelessNetworkManager::begin() {
    Utils::printSerial(F("## Begin TCP socket server."));
    socketServer.begin();
    
    Utils::printSerial(F("## Begin UDP on port: "), "");
    Utils::printSerial(String(Config::UDP_PORT_ESP).c_str());
    udp.begin(Config::UDP_PORT_ESP);
    
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
                delay(2000);
                Utils::setLED(HIGH);
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

void WirelessNetworkManager::handleDatagram() {
    char packet[Config::UDP_PACKET_SIZE];
    int packetSize = udp.parsePacket();
    
    if (!packetSize) {
        return;
    }
    
    int len = udp.read(packet, Config::UDP_PACKET_SIZE - 1);
    if (len > 0) {
        packet[len] = '\0';
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, packet);
    
    if (error) {
        Utils::printSerial(F("Failed to parse UDP packet."));
        return;
    }
    
    const char* request = doc["request"] | "undefined";
    IPAddress remoteIP = udp.remoteIP();
    
    Utils::printSerial(F("UDP request from "), "");
    Utils::printSerial(remoteIP.toString().c_str(), ": ");
    Utils::printSerial(request);
    
    // Handle ping request
    if (strcmp(request, "ping") == 0) {
        char response[64];
        snprintf(response, sizeof(response), "{\"MAC\": \"%s\"}", cachedMacAddress.c_str());
        udp.beginPacket(remoteIP, Config::UDP_PORT_APP);
        udp.print(response);
        udp.endPacket();
        Utils::printSerial(F("Response: "), "");
        Utils::printSerial(response);
    } else {
        Utils::printSerial(F("Request not identified, abort."));
    }
}

WiFiClient WirelessNetworkManager::handleSocket() {
    WiFiClient client = socketServer.available();
    
    if (client && client.connected()) {
        Utils::printSerial(F("Client Connected, client IP: "), "");
        if (Config::SERIAL_MONITOR_ENABLED) {
            Serial.println(client.remoteIP());
        }
    }
    
    return client;
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
