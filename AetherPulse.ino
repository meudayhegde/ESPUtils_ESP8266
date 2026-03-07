#include <Arduino.h>
#include <ArduinoOTA.h>

#include "src/platform/Platform.h"

// mDNS (platform-specific); camera board detection handled by Platform.h
#if defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266mDNS.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <ESPmDNS.h>
#endif

// Application Modules
#include "src/config/Config.h"
#include "src/utils/Utils.h"
#include "src/storage/StorageManager.h"
#include "src/auth/AuthManager.h"
#include "src/auth/SessionManager.h"
#include "src/network/WirelessNetworkManager.h"
#include "src/hardware/gpio/GPIOManager.h"
#include "src/hardware/infrared/IRManager.h"
#include "src/handlers/RequestHandler.h"

// Camera Module (only on ESP32 boards with a camera — ESP_CAM_HW_EXIST set by Platform.h)
#if defined(ESP_CAM_HW_EXIST)
#include "src/hardware/camera/CameraManager.h"
#include "src/handlers/CameraHandler.h"
#endif

// HTTP Server — single server on port 80 for both REST API and camera control
WebServerType httpServer(Config::HTTP_PORT);

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
    if (!StorageManager::begin()) {
        Utils::printSerial(F("WARNING: Storage (LittleFS) failed to mount. Config will not persist!"));
    }
    
    // Initialize GPIO manager and apply stored settings
    GPIOManager::begin();

    // Load and apply persisted sleep mode setting
#if FEATURE_SLEEP_ENABLED
    ESPCommandHandler::initSleep();
#endif

    // Initialize network
    WirelessNetworkManager::initWireless();
    WirelessNetworkManager::begin();
    
    // Initialize IR receiver and sender
    IRManager::begin();
    
    // Load auth module
    AuthManager::begin();
    
    // Get device ID for mDNS
    String deviceID = Utils::getDeviceIDString();
    deviceID.toLowerCase();
    
    // Setup HTTP REST API routes
    ESPCommandHandler::setupRoutes(httpServer);
    
    // Start HTTP server
    httpServer.begin();
    Utils::printSerial(F("## HTTP server started on port "), "");
    char portBuffer[6];
    snprintf(portBuffer, sizeof(portBuffer), "%u", Config::HTTP_PORT);
    Utils::printSerial(portBuffer);

    // Camera routes (only on boards with ESP_CAM_HW_EXIST)
    // Camera hardware is NOT initialised at boot — it will be started on
    // demand when the user enables it via PUT /api/camera/enable.
#if defined(ESP_CAM_HW_EXIST)
    CameraHandler::setupLedFlash();
    CameraHandler::setupRoutes(httpServer);
    Utils::printSerial(F("Camera routes registered (camera disabled until enabled by user)."));
#endif
    
    // Setup OTA updates
    setupOTA();
    
    Utils::printSerial(F("\n========================================"));
    Utils::printSerial(F("System Initialization Complete"));
    Utils::printSerial(F("Device: "), "");
    Utils::printSerial(Config::DEVICE_NAME);
    Utils::printSerial(F("Device ID: "), "");
    Utils::printSerial(Utils::getDeviceIDString());
    Utils::printSerial(F("IP: "), "");
    Utils::printSerial(WirelessNetworkManager::getIPAddress());
    Utils::printSerial(F("MAC: "), "");
    Utils::printSerial(WirelessNetworkManager::getMacAddress());
    Utils::printSerial(F("mDNS: "), "");
    Utils::printSerial(deviceID.c_str(), ".local");
    Serial.println();
    Utils::printSerial(F("========================================\n"));

    // Initialize mDNS with device ID
    WirelessNetworkManager::initMDNS(deviceID.c_str());

    Utils::ledPulse(10, 50, 50);
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
    
    // Handle HTTP requests
    httpServer.handleClient();
    
    // Persist bound JWT to flash if a first-login bind is pending (deferred from HTTP handler)
    SessionManager::tick();
    
    // Update challenge string (auto-refreshes every 5 minutes)
    SessionManager::updateChallenge();
    
    // Handle mDNS (both platforms benefit from periodic updates)
    #ifdef ARDUINO_ARCH_ESP8266
        MDNS.update();
    #endif
    
    // Apply wireless configuration updates if pending
    if (WirelessNetworkManager::isWirelessUpdatePending()) {
        WirelessNetworkManager::initWireless();
        WirelessNetworkManager::clearWirelessUpdateFlag();
        
        // Wait for network to stabilize before reinitializing mDNS
        delay(500);
        
        // Reinitialize mDNS after network change
        String deviceID = Utils::getDeviceIDString();
        deviceID.toLowerCase();
        WirelessNetworkManager::initMDNS(deviceID.c_str());
    }
    
    // Optional: Check for factory reset trigger
    // if (GPIOManager::checkResetState(4)) {
    //     StorageManager::format();
    //     ESP.restart();
    // }
    
    // Small delay to prevent watchdog timer issues
    yield();
}
