#include "AuthManager.h"
#include "../utils/Utils.h"

void AuthManager::begin() {
    Utils::printSerial(F("## Load Auth module..."));
    
    // Initialize session manager
    SessionManager::begin();
}

String AuthManager::authenticateWithJWT(const String& jwtToken) {
    return SessionManager::authenticateWithJWT(jwtToken);
}

bool AuthManager::validateSession(const String& sessionToken) {
    return SessionManager::validateSession(sessionToken);
}

void AuthManager::logout() {
    SessionManager::invalidateSession();
}
