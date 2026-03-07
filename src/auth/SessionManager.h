#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include "../config/Config.h"

// ── One in-RAM session slot ──────────────────────────────────────────────────
// Fixed-size char arrays avoid heap fragmentation on ESP8266.
struct SessionEntry {
    char          sub[64];            // subject identifier (JWT "sub" or "family")
    char          token[41];          // 40 hex-char opaque session token + NUL
    unsigned long createdAtMillis;    // millis() at creation — used for 1-week expiry
    bool          valid;

    SessionEntry() : createdAtMillis(0), valid(false) {
        sub[0]   = '\0';
        token[0] = '\0';
    }
};

// ── SessionManager ────────────────────────────────────────────────────────────
//
// Policy summary:
//   • Up to MAX_SESSIONS (5) active sessions kept purely in RAM — cleared on reboot.
//   • Each slot is keyed by "sub"; re-login for the same sub replaces that slot.
//   • Session tokens expire after SESSION_EXPIRY_MS (1 week) or on reboot.
//   • The "bound JWT" (first successful login) is persisted to flash so the
//     embedded sub can be re-verified on every boot (signature check only).
//
class SessionManager {
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Initialise challenge and clear all in-RAM session slots.
     *        Does NOT touch flash — AuthManager::begin() handles bound-token load.
     */
    static void begin();

    /**
     * @brief Run deferred tasks from loop() — writes bound token to flash when
     *        a bind is pending (avoids LittleFS stack overflow in HTTP handler).
     */
    static void tick();

    // ── Session creation / validation ─────────────────────────────────────────

    /**
     * @brief Create (or replace) an in-RAM session for @p sub.
     *        If a valid slot for this sub already exists it is overwritten.
     *        When all 5 slots are occupied the oldest is evicted.
     * @return The new opaque session token.
     */
    static String createSession(const char* sub);

    /**
     * @brief Validate an opaque session token against all active slots.
     * @return true if a matching, non-expired slot exists.
     */
    static bool validateSession(const char* sessionToken);

    /**
     * @brief Invalidate all active in-RAM sessions (logout-all / factory reset).
     */
    static void invalidateAllSessions();

    /**
     * @brief Compatibility shim — calls invalidateAllSessions().
     */
    static void invalidateSession() { invalidateAllSessions(); }

    // ── Bound identity (persisted to flash) ──────────────────────────────────

    /**
     * @brief Schedule a flush of @p rawJWT + @p sub to flash (deferred to tick()).
     *        Also immediately caches the sub in s_boundSub.
     */
    static void bindJWT(const char* rawJWT, const char* sub);

    /** @return true if a bound sub has been loaded / set. */
    static bool hasBoundSub();

    /** @return The cached bound sub string (empty string if none). */
    static const char* getBoundSub();

    /**
     * @brief Directly set the bound sub (called by AuthManager after boot-time
     *        signature verification of the persisted JWT).
     */
    static void setBoundSub(const char* sub);

    // ── Challenge management ──────────────────────────────────────────────────

    /** @return Current challenge C-string (8 chars + NUL), refreshed automatically. */
    static const char* getCurrentChallenge();

    /** @brief Refresh challenge if the refresh interval has elapsed. */
    static void updateChallenge();

private:
    static SessionEntry  s_sessions[Config::MAX_SESSIONS];

    // Bound identity (populated from flash at boot by AuthManager)
    static char s_boundSub[64];
    static bool s_hasBoundSub;

    // Challenge — 8-char alphanumeric + NUL  (no heap allocation)
    static char          s_challengeString[9];
    static unsigned long s_challengeGeneratedTime;
    static const unsigned long CHALLENGE_REFRESH_INTERVAL = 300000UL; // 5 min

    // Deferred flash write for bound JWT — fixed-size buffers prevent fragmentation
    static bool s_pendingBind;
    static char s_pendingBindJWT[512]; // sized for max ES256 JWT
    static char s_pendingBindSub[64];  // sized for sub claim

    // ── Helpers ───────────────────────────────────────────────────────────────

    /** Fill @p buf (41 bytes) with a new 40-character hex session token. */
    static void generateSessionToken(char* buf, size_t bufLen);

    /** Fill s_challengeString (8 chars + NUL) with random alphanumeric chars. */
    static void generateChallengeString();

    /** Index of slot whose sub matches @p sub, or -1. */
    static int findSlotBySub(const char* sub);

    /**
     * @brief Index of a free (invalid or expired) slot.
     *        If all slots are occupied returns the index of the oldest one.
     */
    static int findFreeOrOldestSlot();

    /** @return true if the slot has lived longer than SESSION_EXPIRY_MS. */
    static bool isSlotExpired(const SessionEntry& e);
};

#endif // SESSION_MANAGER_H
