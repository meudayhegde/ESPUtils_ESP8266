#include "SessionManager.h"
#include "src/storage/StorageManager.h"
#include "src/utils/Utils.h"
#include <ArduinoJson.h>

Session SessionManager::currentSession;

void SessionManager::begin() {
    Utils::printSerial(F("## Initialize Session Manager."));
    
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

String SessionManager::authenticateWithJWT(const String& jwtToken) {
    Utils::printSerial(F("Authenticating with JWT token..."));
    
    // Verify JWT signature (ES256)
    if (!verifyJWTSignature(jwtToken)) {
        Utils::printSerial(F("JWT signature verification failed."));
        return "";
    }
    
    // Parse and validate JWT payload
    if (!parseJWTPayload(jwtToken)) {
        Utils::printSerial(F("JWT payload validation failed."));
        return "";
    }
    
    // Generate new session token
    String sessionToken = generateSessionToken();
    
    // Set session expiry to 1 week from now (604800 seconds)
    currentSession.token = sessionToken;
    currentSession.expiryTime = millis() / 1000 + Config::SESSION_EXPIRY_SECONDS;
    currentSession.isValid = true;
    
    // Save to flash for recovery after restart
    if (saveSession()) {
        Utils::printSerial(F("Session created and saved successfully."));
    } else {
        Utils::printSerial(F("Warning: Session created but save to flash failed."));
    }
    
    return sessionToken;
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
    
    // Delete from flash
    String sessionFile = String(FPSTR(Config::SESSION_FILE));
    StorageManager::deleteFile(sessionFile.c_str());
    
    Utils::printSerial(F("Session invalidated."));
}

Session SessionManager::getCurrentSession() {
    return currentSession;
}

bool SessionManager::verifyJWTSignature(const String& jwtToken) {
    // Split JWT into parts: header.payload.signature
    int firstDot = jwtToken.indexOf('.');
    int secondDot = jwtToken.indexOf('.', firstDot + 1);
    
    if (firstDot == -1 || secondDot == -1) {
        Utils::printSerial(F("Invalid JWT format."));
        return false;
    }
    
    // Extract parts
    // String header = jwtToken.substring(0, firstDot);
    // String payload = jwtToken.substring(firstDot + 1, secondDot);
    // String signature = jwtToken.substring(secondDot + 1);
    
    // TODO: Implement ES256 (ECDSA with P-256 and SHA-256) signature verification
    // This requires:
    // 1. Base64URL decode the signature
    // 2. Get the public key from Config or embedded in code
    // 3. Use uECC library or similar to verify ECDSA signature
    // 4. Verify signature against header.payload
    //
    // For now, this is a placeholder that returns true
    // In production, you must implement proper ES256 verification
    //
    // Example pseudocode:
    // String message = jwtToken.substring(0, secondDot);
    // if (ECDSA_verify(publicKey, message, signature)) {
    //     return true;
    // }
    
    Utils::printSerial(F("Warning: JWT verification not fully implemented. Using placeholder."));
    
    // Basic format validation passed
    return true;
}

bool SessionManager::parseJWTPayload(const String& jwtToken) {
    // Extract payload from JWT
    int firstDot = jwtToken.indexOf('.');
    int secondDot = jwtToken.indexOf('.', firstDot + 1);
    
    if (firstDot == -1 || secondDot == -1) {
        return false;
    }
    
    String payloadB64 = jwtToken.substring(firstDot + 1, secondDot);
    
    // TODO: Implement Base64URL decode
    // For now, we'll skip detailed payload validation
    // In production, you should:
    // 1. Base64URL decode the payload
    // 2. Parse JSON
    // 3. Verify issuer matches cloud server
    // 4. Verify expiration time
    // 5. Verify other claims as needed
    
    Utils::printSerial(F("Warning: JWT payload parsing not fully implemented."));
    
    // Basic validation passed
    return payloadB64.length() > 0;
}

String SessionManager::generateSessionToken() {
    // Generate a random session token
    // Using device ID, current time, and random values
    String token = "";
    
    // Add timestamp component
    unsigned long timestamp = millis();
    
    // Generate random bytes
    uint8_t randomBytes[16];
    for (int i = 0; i < 16; i++) {
        randomBytes[i] = random(256);
    }
    
    // Create token as hex string
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%08lX", timestamp);
    token += buffer;
    
    for (int i = 0; i < 16; i++) {
        snprintf(buffer, sizeof(buffer), "%02X", randomBytes[i]);
        token += buffer;
    }
    
    return token;
}

bool SessionManager::saveSession() {
    JsonDocument doc;
    
    doc["token"] = currentSession.token;
    doc["expiry"] = currentSession.expiryTime;
    doc["valid"] = currentSession.isValid;
    
    String sessionFile = String(FPSTR(Config::SESSION_FILE));
    return StorageManager::writeJson(sessionFile.c_str(), doc);
}

bool SessionManager::loadSession() {
    JsonDocument doc;
    
    String sessionFile = String(FPSTR(Config::SESSION_FILE));
    if (!StorageManager::readJson(sessionFile.c_str(), doc)) {
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
