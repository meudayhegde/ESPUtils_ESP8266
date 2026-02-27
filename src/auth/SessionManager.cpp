#include "SessionManager.h"
#include "../storage/StorageManager.h"
#include "../utils/Utils.h"
#include <ArduinoJson.h>

AuthSession SessionManager::currentSession;
String SessionManager::challengeString = "";
unsigned long SessionManager::challengeGeneratedTime = 0;
bool SessionManager::pendingSave = false;

void SessionManager::begin() {
    Utils::printSerial(F("## Initialize Session Manager."));
    
    // Generate initial challenge string
    challengeString = generateChallengeString();
    challengeGeneratedTime = millis();
    Utils::printSerial(F("Initial challenge generated: "), challengeString.c_str());
    
    // Load existing session from flash
    if (loadSession()) {
        if (isSessionExpired()) {
            Utils::printSerial(F("Stored session expired. Invalidating."));
            invalidateSession();
        } else {
            Utils::printSerial(F("Valid session loaded from storage."));
        }
    } else {
        Utils::printSerial(F("No stored session found."));
    }
}

String SessionManager::createSession() {
    String sessionToken       = generateSessionToken();
    currentSession.token      = sessionToken;
    currentSession.expiryTime = millis() / 1000 + Config::SESSION_EXPIRY_SECONDS;
    currentSession.isValid    = true;
    
    // Defer flash write to loop() — LittleFS uses too much stack
    // for the ESP8266 cont stack (4 KB) when called inside an HTTP handler.
    pendingSave = true;
    Utils::printSerial(F("Session created (save pending)."));
    
    return sessionToken;
}

void SessionManager::tick() {
    if (!pendingSave) return;
    pendingSave = false;
    if (!saveSession()) {
        Utils::printSerial(F("Warning: session save failed."));
    } else {
        Utils::printSerial(F("Session persisted to flash."));
    }
}

bool SessionManager::validateSession(const String& sessionToken) {
    // Check if session exists and is valid
    if (!currentSession.isValid || currentSession.token.length() == 0) {
        return false;
    }
    
    // Check if token matches
    if (currentSession.token != sessionToken) {
        return false;
    }
    
    // Check if session has expired
    if (isSessionExpired()) {
        invalidateSession();
        return false;
    }
    
    return true;
}

void SessionManager::invalidateSession() {
    currentSession.token = "";
    currentSession.expiryTime = 0;
    currentSession.isValid = false;
    
    // Delete from flash storage
    StorageManager::deleteFile(Config::SESSION_FILE);
    
    Utils::printSerial(F("Session invalidated."));
}

AuthSession SessionManager::getCurrentSession() {
    return currentSession;
}

String SessionManager::generateSessionToken() {
    // Single stack buffer — one String allocation, no intermediate heap copies
    char token[41];  // 8 hex (timestamp) + 32 hex (16 random bytes) + '\0'
    snprintf(token, 9, "%08lX", millis());
    for (int i = 0; i < 16; i++) {
        snprintf(token + 8 + i * 2, 3, "%02X", (uint8_t)random(256));
    }
    token[40] = '\0';
    return String(token);
}

bool SessionManager::saveSession() {
    JsonDocument doc;
    
    doc["token"] = currentSession.token;
    doc["expiry"] = currentSession.expiryTime;
    doc["valid"] = currentSession.isValid;

    return StorageManager::writeJson(Config::SESSION_FILE, doc);
}

bool SessionManager::loadSession() {
    JsonDocument doc;
    
    if (!StorageManager::readJson(Config::SESSION_FILE, doc)) {
        return false;
    }
    
    currentSession.token = doc["token"] | "";
    currentSession.expiryTime = doc["expiry"] | 0UL;
    currentSession.isValid = doc["valid"] | false;
    
    return currentSession.token.length() > 0;
}

bool SessionManager::isSessionExpired() {
    if (!currentSession.isValid) {
        return true;
    }
    
    unsigned long currentTime = millis() / 1000;
    
    // Handle millis() rollover (occurs after ~49 days)
    // If currentTime < expiryTime but the difference is huge, we've rolled over
    if (currentTime < currentSession.expiryTime) {
        unsigned long diff = currentSession.expiryTime - currentTime;
        // If difference is still reasonable (less than session expiry time), not expired
        if (diff <= Config::SESSION_EXPIRY_SECONDS) {
            return false;
        }
    }
    
    // Current time >= expiry time, session expired
    return currentTime >= currentSession.expiryTime;
}

String SessionManager::getCurrentChallenge() {
    // Check if challenge needs refresh
    updateChallenge();
    return challengeString;
}

void SessionManager::updateChallenge() {
    unsigned long currentTime = millis();
    
    // Handle millis() rollover
    bool needsRefresh = false;
    if (currentTime >= challengeGeneratedTime) {
        needsRefresh = (currentTime - challengeGeneratedTime) >= CHALLENGE_REFRESH_INTERVAL;
    } else {
        // Rollover occurred
        needsRefresh = true;
    }
    
    if (needsRefresh) {
        challengeString = generateChallengeString();
        challengeGeneratedTime = currentTime;
        Utils::printSerial(F("Challenge refreshed: "), challengeString.c_str());
        Utils::printSerial(F(""));
    }
}

String SessionManager::generateChallengeString() {
    // Generate an 8-character random challenge string using alphanumeric characters
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const int charsetLength = sizeof(charset) - 1;
    const int challengeLength = 8;
    
    String challenge = "";
    
    for (int i = 0; i < challengeLength; i++) {
        challenge += charset[random(charsetLength)];
    }
    
    return challenge;
}
