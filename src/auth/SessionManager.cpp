#include "SessionManager.h"
#include "../storage/StorageManager.h"
#include "../utils/Utils.h"

// ── Static member definitions ─────────────────────────────────────────────────
SessionEntry  SessionManager::s_sessions[Config::MAX_SESSIONS];
char          SessionManager::s_boundSub[64]          = {};
bool          SessionManager::s_hasBoundSub            = false;
char          SessionManager::s_challengeString[9]    = {};   // 8 chars + NUL
unsigned long SessionManager::s_challengeGeneratedTime = 0;
bool          SessionManager::s_pendingBind            = false;
char          SessionManager::s_pendingBindJWT[512]   = {};
char          SessionManager::s_pendingBindSub[64]    = {};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void SessionManager::begin() {
    Utils::printSerial(F("## Initialize Session Manager."));

    // Reset all in-RAM session slots
    for (uint8_t i = 0; i < Config::MAX_SESSIONS; i++) {
        s_sessions[i] = SessionEntry();
    }

    // Generate initial challenge into the static char buffer
    generateChallengeString();
    s_challengeGeneratedTime = millis();
    Utils::printSerial(F("Challenge: "), s_challengeString);
}

void SessionManager::tick() {
    if (!s_pendingBind) return;
    s_pendingBind = false;

    // Write bound JWT + sub to flash as a fixed-size binary struct
    BoundTokenData data;
    strncpy(data.sub, s_pendingBindSub, sizeof(data.sub) - 1);
    data.sub[sizeof(data.sub) - 1] = '\0';
    strncpy(data.jwt, s_pendingBindJWT, sizeof(data.jwt) - 1);
    data.jwt[sizeof(data.jwt) - 1] = '\0';

    if (!StorageManager::saveBoundToken(data)) {
        Utils::printSerial(F("\nWarning: bound token save failed."));
    }

    // Clear the pending buffers
    s_pendingBindJWT[0] = '\0';
    s_pendingBindSub[0] = '\0';
}

// ── Session creation / validation ─────────────────────────────────────────────

String SessionManager::createSession(const char* sub) {
    int idx = findSlotBySub(sub);
    if (idx < 0) {
        idx = findFreeOrOldestSlot();
        Utils::printSerial(F("\nSession slot allocated: "), (long)idx);
    } else {
        Utils::printSerial(F("\nSession slot replaced for sub: "), sub);
    }

    SessionEntry& slot   = s_sessions[idx];
    strncpy(slot.sub, sub, sizeof(slot.sub) - 1);
    slot.sub[sizeof(slot.sub) - 1] = '\0';
    generateSessionToken(slot.token, sizeof(slot.token));
    slot.createdAtMillis = millis();
    slot.valid           = true;

    Utils::printSerial(F("\nSession created for sub: "), sub);
    return String(slot.token);
}

bool SessionManager::validateSession(const char* tok) {
    for (uint8_t i = 0; i < Config::MAX_SESSIONS; i++) {
        SessionEntry& slot = s_sessions[i];
        if (!slot.valid) continue;
        if (isSlotExpired(slot)) {
            slot.valid = false;
            continue;
        }
        if (strcmp(slot.token, tok) == 0) return true;
    }
    return false;
}

void SessionManager::invalidateAllSessions() {
    for (uint8_t i = 0; i < Config::MAX_SESSIONS; i++) {
        s_sessions[i].valid = false;
    }
    Utils::printSerial(F("All sessions invalidated."));
}

// ── Bound identity ────────────────────────────────────────────────────────────

void SessionManager::bindJWT(const char* rawJWT, const char* sub) {
    // Cache sub immediately
    setBoundSub(sub);

    // Copy JWT + sub into fixed-size buffers for deferred flash write
    strncpy(s_pendingBindJWT, rawJWT, sizeof(s_pendingBindJWT) - 1);
    s_pendingBindJWT[sizeof(s_pendingBindJWT) - 1] = '\0';
    strncpy(s_pendingBindSub, sub, sizeof(s_pendingBindSub) - 1);
    s_pendingBindSub[sizeof(s_pendingBindSub) - 1] = '\0';
    s_pendingBind = true;
    Utils::printSerial(F("Bind JWT queued for flash write (sub: "), sub);
    Utils::printSerial(F(")"));
}

bool SessionManager::hasBoundSub() {
    return s_hasBoundSub;
}

const char* SessionManager::getBoundSub() {
    return s_boundSub;
}

void SessionManager::setBoundSub(const char* sub) {
    strncpy(s_boundSub, sub, sizeof(s_boundSub) - 1);
    s_boundSub[sizeof(s_boundSub) - 1] = '\0';
    s_hasBoundSub = (s_boundSub[0] != '\0');
}

// ── Challenge management ──────────────────────────────────────────────────────

const char* SessionManager::getCurrentChallenge() {
    updateChallenge();
    return s_challengeString;
}

void SessionManager::updateChallenge() {
    const unsigned long now      = millis();
    const bool         rollover  = (now < s_challengeGeneratedTime);
    const bool         stale     = (!rollover) &&
                                   ((now - s_challengeGeneratedTime) >= CHALLENGE_REFRESH_INTERVAL);
    if (rollover || stale) {
        generateChallengeString();
        s_challengeGeneratedTime = now;
        Utils::printSerial(F("\nChallenge refreshed: "), s_challengeString);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void SessionManager::generateSessionToken(char* buf, size_t bufLen) {
    if (bufLen < 41) return;
    snprintf(buf, 9, "%08lX", millis());
    for (int i = 0; i < 16; i++) {
        snprintf(buf + 8 + i * 2, 3, "%02X", (uint8_t)random(256));
    }
    buf[40] = '\0';
}

// Fills s_challengeString with 8 random alphanumeric chars + NUL.
// No heap allocation — operates entirely on the static char array.
void SessionManager::generateChallengeString() {
    static const char kCharset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static const int  kCharsetLen = sizeof(kCharset) - 1; // 62
    for (int i = 0; i < 8; i++) {
        s_challengeString[i] = kCharset[random(kCharsetLen)];
    }
    s_challengeString[8] = '\0';
}

int SessionManager::findSlotBySub(const char* sub) {
    for (uint8_t i = 0; i < Config::MAX_SESSIONS; i++) {
        if (s_sessions[i].valid && strcmp(s_sessions[i].sub, sub) == 0) return (int)i;
    }
    return -1;
}

int SessionManager::findFreeOrOldestSlot() {
    // Prefer an invalid or expired slot
    for (uint8_t i = 0; i < Config::MAX_SESSIONS; i++) {
        if (!s_sessions[i].valid || isSlotExpired(s_sessions[i])) return (int)i;
    }
    // All slots occupied — evict the oldest
    int   oldest     = 0;
    unsigned long min = s_sessions[0].createdAtMillis;
    for (uint8_t i = 1; i < Config::MAX_SESSIONS; i++) {
        if (s_sessions[i].createdAtMillis < min) {
            min    = s_sessions[i].createdAtMillis;
            oldest = (int)i;
        }
    }
    Utils::printSerial(F("Session pool full — evicting oldest slot: "), (long)oldest);
    return oldest;
}

bool SessionManager::isSlotExpired(const SessionEntry& e) {
    // Unsigned subtraction handles millis() rollover correctly
    return (millis() - e.createdAtMillis) >= Config::SESSION_EXPIRY_MS;
}
