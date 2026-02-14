#include "AuthManager.h"
#include "src/utils/Utils.h"

UserConfig AuthManager::userConfig;

void AuthManager::begin() {
    Utils::printSerial(F("## Load user credentials."));
    
    if (!StorageManager::loadUserConfig(userConfig)) {
        Utils::printSerial(F("Using default credentials."));
        // Default values already set in UserConfig constructor
    }
}

bool AuthManager::authenticate(const char* username, const char* password) {
    if (!username || !password) {
        return false;
    }
    
    return (userConfig.username == username && userConfig.password == password);
}

bool AuthManager::updateCredentials(const char* username, const char* password) {
    if (!username || !password) {
        return false;
    }
    
    userConfig.username = username;
    userConfig.password = password;
    
    if (!StorageManager::saveUserConfig(userConfig)) {
        Utils::printSerial(F("Failed to save user credentials."));
        return false;
    }
    
    Utils::printSerial(F("User credentials updated successfully."));
    return true;
}

const String& AuthManager::getUsername() {
    return userConfig.username;
}

bool AuthManager::resetToDefault() {
    userConfig.username = String(FPSTR(Config::DEVICE_NAME));
    userConfig.password = String(FPSTR(Config::DEVICE_PASSWORD));
    
    return StorageManager::saveUserConfig(userConfig);
}
