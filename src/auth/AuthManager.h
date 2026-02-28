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
    static String authenticateWithJWT(const char* jwt, size_t jwtLen);
    
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

private:
    static void initPublicKey();

    /**
     * @brief Parse and cryptographically verify a JWT (ES256).
     *
     * Populates the static s_parsedSub and s_parsedFamily buffers on success.
     *
     * @param jwt             Raw JWT string (header.payload.sig)
     * @param len             Length of @p jwt
     * @param verifyChallenge When true the "challenge" claim is checked against
     *                        the current device challenge (required at login).
     *                        When false only the signature is checked (used at
     *                        boot to re-verify the persisted bound JWT).
     * @return true on success, false on any failure.
     */
    static bool verifyAndParseJWT(const char* jwt, size_t len, bool verifyChallenge);

    /**
     * @brief Load the persisted bound JWT from flash and verify its signature.
     *        On success calls SessionManager::setBoundSub() with the stored sub.
     *        Called once from begin() after the public key is initialised.
     */
    static void loadAndVerifyBoundToken();
};

#endif // AUTH_MANAGER_H
