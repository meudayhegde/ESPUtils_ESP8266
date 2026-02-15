#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include "src/config/Config.h"
#include "src/storage/StorageManager.h"
#include "SessionManager.h"

class AuthManager {
private:
    static UserConfig userConfig;
    
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
    
    /**
     * @brief Authenticate user with username and password (legacy)
     * @param username Username to verify
     * @param password Password to verify
     * @return true if authenticated, false otherwise
     */
    static bool authenticate(const char* username, const char* password);
    
    /**
     * @brief Update user credentials
     * @param username New username
     * @param password New password
     * @return true if successful, false otherwise
     */
    static bool updateCredentials(const char* username, const char* password);
    
    /**
     * @brief Get current username
     * @return Current username
     */
    static const String& getUsername();
    
    /**
     * @brief Reset credentials to default
     * @return true if successful, false otherwise
     */
    static bool resetToDefault();
};

#endif // AUTH_MANAGER_H
