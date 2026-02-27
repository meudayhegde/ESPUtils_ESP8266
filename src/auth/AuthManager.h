#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include "../config/Config.h"
#include "../storage/StorageManager.h"
#include "SessionManager.h"

class AuthManager {

public:
    /**
     * @brief Initialize authentication manager and load credentials
     */
    static void begin();
    
    /**
     * @brief Authenticate with JWT token and create session
     * @param jwtToken JWT token from cloud server
     * @return Session token if successful, empty string otherwise
     */
    static String authenticateWithJWT(const String& jwtToken);
    
    /**
     * @brief Validate session token for protected endpoints
     * @param sessionToken Session token to validate
     * @return true if valid, false otherwise
     */
    static bool validateSession(const String& sessionToken);
    
    /**
     * @brief Invalidate current session (logout)
     */
    static void logout();
};

#endif // AUTH_MANAGER_H
