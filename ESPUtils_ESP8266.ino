#include <Arduino.h>
#include <ArduinoOTA.h>

#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
#elif ARDUINO_ARCH_ESP32
    #include <WiFi.h>
#endif

// Application Modules
#include "src/config/Config.h"
#include "src/utils/Utils.h"
#include "src/storage/StorageManager.h"
#include "src/auth/AuthManager.h"
#include "src/network/WirelessNetworkManager.h"
#include "src/hardware/GPIOManager.h"
#include "src/hardware/IRManager.h"
#include "src/handlers/RequestHandler.h"

/**
 * @brief Initialize all system components
 */
void setup() {
    // Initialize serial communication
    #if defined(ESP8266)
        Serial.begin(Config::BAUD_RATE, SERIAL_8N1, SERIAL_TX_ONLY);
    #else
        Serial.begin(Config::BAUD_RATE);
    #endif
    
    delay(1000);

    Utils::printSerial(F("\n\n========================================"));
    Utils::printSerial(F("ESPUtils - Starting System Initialization"));
    Utils::printSerial(F("========================================\n"));
    
    // Initialize LED indicator
    Utils::printSerial(F("## Set Board Indicator."));
    Utils::initLED();
    
    // Initialize file system
    StorageManager::begin();
    
    // Initialize GPIO manager and apply stored settings
    GPIOManager::begin();
    
    // Initialize network
    WirelessNetworkManager::initWireless();
    WirelessNetworkManager::begin();
    
    // Initialize IR receiver and sender
    IRManager::begin();
    
    // Load user credentials
    AuthManager::begin();
    
    // Setup OTA updates
    setupOTA();
    
    Utils::printSerial(F("\n========================================"));
    Utils::printSerial(F("System Initialization Complete"));
    Utils::printSerial(F("Device: "), "");
    Utils::printSerial(FPSTR(Config::DEVICE_NAME));
    Utils::printSerial(F("Chip ID: "), "");
    Utils::printSerial(Utils::getChipIDString().c_str());
    Utils::printSerial(F("IP: "), "");
    Utils::printSerial(WirelessNetworkManager::getIPAddress().c_str());
    Utils::printSerial(F("MAC: "), "");
    Utils::printSerial(WirelessNetworkManager::getMacAddress().c_str());
    Utils::printSerial(F("========================================\n"));
}

/**
 * @brief Configure Arduino OTA for remote updates
 */
void setupOTA() {
    Utils::printSerial(F("## Begin ArduinoOTA service."));
    
    ArduinoOTA.onStart([]() {
        Utils::printSerial(F("## Begin OTA Update process."));
    });
    
    ArduinoOTA.onEnd([]() {
        Utils::printSerial(F("\n## OTA Update process End."));
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if (Config::SERIAL_MONITOR_ENABLED) {
            Serial.printf("==> Progress: %u%%\r", (progress / (total / 100)));
        }
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        if (Config::SERIAL_MONITOR_ENABLED) {
            Serial.printf("Error[%u]: ", error);
        }
        
        switch (error) {
            case OTA_AUTH_ERROR:
                Utils::printSerial(F("OTA Auth Failed"));
                break;
            case OTA_BEGIN_ERROR:
                Utils::printSerial(F("OTA Begin Failed"));
                break;
            case OTA_CONNECT_ERROR:
                Utils::printSerial(F("OTA Connect Failed"));
                break;
            case OTA_RECEIVE_ERROR:
                Utils::printSerial(F("OTA Receive Failed"));
                break;
            case OTA_END_ERROR:
                Utils::printSerial(F("OTA End Failed"));
                break;
            default:
                Utils::printSerial(F("OTA Unknown Error"));
        }
    });
    
    ArduinoOTA.setPort(Config::OTA_PORT);
    ArduinoOTA.begin();
}

/**
 * @brief Main event loop
 */
void loop() {
    // Handle OTA updates
    ArduinoOTA.handle();
    
    // Handle UDP datagrams
    WirelessNetworkManager::handleDatagram();
    
    // Handle TCP socket connections
    WiFiClient client = WirelessNetworkManager::handleSocket();
    
    if (client && client.connected()) {
        while (client.connected()) {
            while (client.available()) {
                String request = client.readStringUntil('\n');
                String response = RequestHandler::handleRequest(request, client);
                
                response.replace("\n", "");
                client.println(response);
                client.flush();
                client.stop();
            }
        }
        Utils::printSerial(F("Client disconnected."));
    }
    
    // Apply wireless configuration updates if pending
    if (WirelessNetworkManager::isWirelessUpdatePending()) {
        WirelessNetworkManager::initWireless();
        WirelessNetworkManager::clearWirelessUpdateFlag();
    }
    
    // Optional: Check for factory reset trigger
    // if (GPIOManager::checkResetState(4)) {
    //     RequestHandler::handleReset();
    //     ESP.restart();
    // }
    
    // Small delay to prevent watchdog timer issues
    yield();
}
