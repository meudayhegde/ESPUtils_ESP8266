#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include "src/config/Config.h"

// Session structure
struct Session {
    String token;
    unsigned long expiryTime; // Unix timestamp
    bool isValid;
    
    Session() : token(""), expiryTime(0), isValid(false) {}
};

class SessionManager {
public:
    /**
     * @brief Initialize session manager and load from storage
     */
    static void begin();
    
    /**
     * @brief Verify JWT token (ES256) and create session
     * @param jwtToken JWT token string from cloud server
     * @return Session token if valid, empty string otherwise
     */
    static String authenticateWithJWT(const String& jwtToken);
    
    /**
     * @brief Validate session token
     * @param sessionToken Session token to validate
     * @return true if valid and not expired, false otherwise
     */
    static bool validateSession(const String& sessionToken);
    
    /**
     * @brief Invalidate current session
     */
    static void invalidateSession();
    
    /**
     * @brief Get current session info
     * @return Current session structure
     */
    static Session getCurrentSession();
    
private:
    /**
     * @brief Verify ES256 JWT token signature
     * @param jwtToken JWT token to verify
     * @return true if signature is valid, false otherwise
     */
    static bool verifyJWTSignature(const String& jwtToken);
    
    /**
     * @brief Parse and validate JWT payload
     * @param jwtToken JWT token to parse
     * @return true if payload is valid, false otherwise
     */
    static bool parseJWTPayload(const String& jwtToken);
    
    /**
     * @brief Generate a random session token
     * @return Random session token string
     */
    static String generateSessionToken();
    
    /**
     * @brief Save session to flash storage
     * @return true if successful, false otherwise
     */
    static bool saveSession();
    
    /**
     * @brief Load session from flash storage
     * @return true if successful, false otherwise
     */
    static bool loadSession();
    
    /**
     * @brief Check if session has expired
     * @return true if expired, false otherwise
     */
    static bool isSessionExpired();
    
    static Session currentSession;
};

#endif // SESSION_MANAGER_H
