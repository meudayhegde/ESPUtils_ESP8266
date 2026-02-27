#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include "../config/Config.h"

// Session structure
struct AuthSession {
    String token;
    unsigned long expiryTime; // Unix timestamp
    bool isValid;
    
    AuthSession() : token(""), expiryTime(0), isValid(false) {}
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
    static AuthSession getCurrentSession();
    
    /**
     * @brief Get current challenge string
     * @return Current challenge string (auto-refreshes every 5 minutes)
     */
    static String getCurrentChallenge();
    
    /**
     * @brief Check if challenge needs refresh and update if needed
     * Called periodically to maintain fresh challenge
     */
    static void updateChallenge();
    
private:
    static void   initPublicKey();
    static bool   verifyAndParseJWT(const char* jwt, size_t len);
    
    /**
     * @brief Generate a random session token
     * @return Random session token string
     */
    static String generateSessionToken();
    
    /**
     * @brief Generate a random challenge string
     * @return Random 8-character challenge string
     */
    static String generateChallengeString();
    
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
    
    static AuthSession currentSession;
    static String challengeString;
    static unsigned long challengeGeneratedTime;
    static const unsigned long CHALLENGE_REFRESH_INTERVAL = 300000; // 5 minutes in milliseconds
};

#endif // SESSION_MANAGER_H
