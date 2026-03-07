#include "AuthManager.h"
#include "../utils/Utils.h"
#include "../storage/StorageManager.h"
#include <ArduinoJson.h>

// Cryptographic includes
#if defined(ARDUINO_ARCH_ESP8266)
    #include <bearssl/bearssl_hash.h>
    #include <bearssl/bearssl_ec.h>
    #include <bearssl/bearssl_pem.h>
    #include <WiFiClientSecureBearSSL.h>
    #include <StackThunk.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include "mbedtls/ecdsa.h"
    #include "mbedtls/sha256.h"
    #include "mbedtls/pk.h"
    #include "mbedtls/error.h"
#endif

// Base64 lookup table — accepts both standard (+/) and URL-safe (-_). 0xFF = invalid.
static const uint8_t kB64Lut[128] = {
/* 00 */ 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
/* 10 */ 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
/* 20 */ 255,255,255,255,255,255,255,255,255,255,255, 62,255, 62,255, 63,  // + - /
/* 30 */  52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,255,255,255,  // 0-9
/* 40 */ 255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  // A-O
/* 50 */  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255, 63,  // P-Z _
/* 60 */ 255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // a-o
/* 70 */  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,  // p-z
};

// Standard base64 decode — PEM keys only (skips whitespace, accepts + and /)
static size_t base64Decode(const char* in, size_t inLen, uint8_t* out, size_t outLen) {
    uint32_t v = 0; int bits = -8; size_t n = 0;
    for (size_t i = 0; i < inLen; i++) {
        uint8_t c = (uint8_t)in[i];
        if (c == '=') break;
        if (c <= ' ') continue;
        if (c >= 128 || kB64Lut[c] == 255) return 0;
        v = (v << 6) | kB64Lut[c]; bits += 6;
        if (bits >= 0 && n < outLen) { out[n++] = (v >> bits) & 0xFF; bits -= 8; }
    }
    return n;
}

// Base64URL decode — lenient (JWT header / payload)
static size_t b64urlDecodeLenient(const char* in, size_t inLen, uint8_t* out, size_t outLen) {
    uint32_t v = 0; int bits = -8; size_t n = 0;
    for (size_t i = 0; i < inLen; i++) {
        uint8_t c = (uint8_t)in[i];
        if (c == '=') break;
        if (c >= 128 || kB64Lut[c] == 255) return 0;
        v = (v << 6) | kB64Lut[c]; bits += 6;
        if (bits >= 0 && n < outLen) { out[n++] = (v >> bits) & 0xFF; bits -= 8; }
    }
    return n;
}

// Base64URL decode — strict (JWT signature only): rejects + / = and enforces canonical trailing bits
static size_t b64urlDecodeStrict(const char* in, size_t inLen, uint8_t* out, size_t outLen) {
    for (size_t i = 0; i < inLen; i++) {
        if (in[i] == '+' || in[i] == '/' || in[i] == '=') return 0;
    }
    if (inLen % 4 == 1) return 0;
    if (inLen > 0) {
        uint8_t c = (uint8_t)in[inLen - 1];
        if (c >= 128 || kB64Lut[c] == 255) return 0;
        uint8_t lv = kB64Lut[c];
        if (inLen % 4 == 2 && (lv & 0x0F) != 0) return 0;
        if (inLen % 4 == 3 && (lv & 0x03) != 0) return 0;
    }
    uint32_t v = 0; int bits = -8; size_t n = 0;
    for (size_t i = 0; i < inLen; i++) {
        uint8_t c = (uint8_t)in[i];
        if (c >= 128 || kB64Lut[c] == 255) return 0;
        v = (v << 6) | kB64Lut[c]; bits += 6;
        if (bits >= 0 && n < outLen) { out[n++] = (v >> bits) & 0xFF; bits -= 8; }
    }
    return n;
}

#ifdef ARDUINO_ARCH_ESP32
// Convert raw ECDSA signature (r||s) to ASN.1 DER format for mbedTLS
static size_t rawToAsn1Der(const uint8_t* rawSig, size_t rawLen, uint8_t* derSig, size_t derMaxLen) {
    if (rawLen != 64) return 0;
    if (derMaxLen < 80) return 0;

    const uint8_t* r = rawSig;
    const uint8_t* s = rawSig + 32;

    auto encodeInteger = [](const uint8_t* value, size_t len, uint8_t* output) -> size_t {
        size_t start = 0;
        while (start < len - 1 && value[start] == 0) start++;
        size_t actualLen = len - start;
        bool needsPadding = (value[start] & 0x80) != 0;
        size_t totalLen = actualLen + (needsPadding ? 1 : 0);
        output[0] = 0x02;
        output[1] = totalLen;
        size_t pos = 2;
        if (needsPadding) output[pos++] = 0x00;
        memcpy(&output[pos], &value[start], actualLen);
        return pos + actualLen;
    };

    uint8_t rDer[35], sDer[35];
    size_t rDerLen = encodeInteger(r, 32, rDer);
    size_t sDerLen = encodeInteger(s, 32, sDer);
    size_t seqContentLen = rDerLen + sDerLen;

    if (seqContentLen >= 128) {
        derSig[0] = 0x30; derSig[1] = 0x81; derSig[2] = seqContentLen;
        memcpy(&derSig[3], rDer, rDerLen);
        memcpy(&derSig[3 + rDerLen], sDer, sDerLen);
        return 3 + seqContentLen;
    } else {
        derSig[0] = 0x30; derSig[1] = seqContentLen;
        memcpy(&derSig[2], rDer, rDerLen);
        memcpy(&derSig[2 + rDerLen], sDer, sDerLen);
        return 2 + seqContentLen;
    }
}
#endif

// ── Cached public key (parsed once at begin()) ──
#if defined(ARDUINO_ARCH_ESP8266)
static bool             s_pubKeyReady = false;
static uint8_t          s_pubKeyQ[65];       // 0x04 || X || Y (65 bytes, uncompressed point)
static br_ec_public_key s_brPubKey;

// ── StackThunk ECDSA trampoline ──
// BearSSL ECDSA P-256 verification uses ~3 KB of internal stack for
// big-number math, overflowing the ESP8266 4 KB cont stack.  The ESP8266
// core provides StackThunk: a heap-allocated 5.6 KB alternate stack.
// We use a void(void) trampoline with static args so the thunk macro
// doesn't need to pass registers through.
static const br_ec_impl*      s_vrfyEc;
static const uint8_t*         s_vrfyHash;
static const br_ec_public_key* s_vrfyKey;
static const uint8_t*         s_vrfySig;
static uint32_t               s_vrfyResult;

extern "C" void ecdsa_vrfy_on_heap_stack() {
    br_ecdsa_vrfy vrfy = br_ecdsa_vrfy_raw_get_default();
    s_vrfyResult = vrfy(s_vrfyEc, s_vrfyHash, 32, s_vrfyKey, s_vrfySig, 64);
}
make_stack_thunk(ecdsa_vrfy_on_heap_stack)
extern "C" void thunk_ecdsa_vrfy_on_heap_stack();

#elif defined(ARDUINO_ARCH_ESP32)
static bool               s_pubKeyReady = false;
static mbedtls_pk_context s_pkCtx;
#endif

// ── Parsed JWT claims (populated by verifyAndParseJWT, cleared each call) ──
static char s_parsedSub[64]    = {};
static char s_parsedFamily[64] = {};

void AuthManager::begin() {
    Utils::printSerial(F("## Load Auth module..."));

#if defined(ARDUINO_ARCH_ESP8266)
    // Pre-allocate the 5.6 KB heap stack used by the BearSSL ECDSA thunk.
    stack_thunk_add_ref();
#endif

    // Parse and cache the EC public key once at boot
    initPublicKey();

    // Initialize session manager (challenge + clear RAM sessions)
    SessionManager::begin();

    // Load and re-verify the bound JWT from flash (signature only, no challenge)
    loadAndVerifyBoundToken();
}

String AuthManager::authenticateWithJWT(const char* jwt, size_t jwtLen) {
    Utils::printSerial(F("Authenticating with JWT..."));

    // Verify signature + challenge for every login attempt
    if (!verifyAndParseJWT(jwt, jwtLen, true)) {
        Utils::printSerial(F("JWT authentication failed."));
        return "";
    }

    const char* incomingSub    = s_parsedSub;     // may be empty string
    const char* incomingFamily = s_parsedFamily;  // may be empty string

    if (!SessionManager::hasBoundSub()) {
        // ── First-time login ─────────────────────────────────────────────────
        // The token MUST carry a "sub" claim so we can bind this device to an identity.

        // Persist the raw JWT + sub to flash (deferred write via tick())
        SessionManager::bindJWT(jwt, incomingSub);
        Utils::printSerial(F("Identity bound to sub: "), incomingSub);
        return SessionManager::createSession(incomingSub);

    } else {
        // ── Subsequent login ─────────────────────────────────────────────────
        // Accept if:  sub == bound_sub   OR   family == bound_sub
        const char* bound     = SessionManager::getBoundSub();
        bool subMatch         = (incomingSub[0]    != '\0') && (strcmp(incomingSub,    bound) == 0);
        bool familyMatch      = (incomingFamily[0] != '\0') && (strcmp(incomingFamily, bound) == 0);

        if (!subMatch && !familyMatch) {
            Utils::printSerial(F("JWT: sub/family does not match bound identity"));
            Utils::printSerial(F("  bound   : "), bound);
            Utils::printSerial(F("  sub     : "), incomingSub);
            Utils::printSerial(F("  family  : "), incomingFamily);
            return "";
        }

        // Key the session slot by the token's own sub when present;
        // fall back to the family value (which equals bound_sub) otherwise.
        const char* sessionSub = (incomingSub[0] != '\0') ? incomingSub : incomingFamily;
        return SessionManager::createSession(sessionSub);
    }
}

bool AuthManager::validateSession(const String& sessionToken) {
    return SessionManager::validateSession(sessionToken.c_str());
}

void AuthManager::logout() {
    SessionManager::invalidateSession();
}

bool AuthManager::validateResetJWT(const char* jwt, size_t jwtLen) {
    Utils::printSerial(F("Validating JWT for reset..."));

    // First, verify the JWT with challenge check
    if (!verifyAndParseJWT(jwt, jwtLen, true)) {
        Utils::printSerial(F("JWT validation failed for reset."));
        return false;
    }

    // Check if device is bound
    if (!SessionManager::hasBoundSub()) {
        Utils::printSerial(F("Reset attempted on unbound device."));
        return false;
    }

    // Get the bound sub
    const char* boundSub = SessionManager::getBoundSub();
    
    // For reset, we require exact sub match (not family)
    // s_parsedSub is populated by verifyAndParseJWT
    if (strcmp(s_parsedSub, boundSub) != 0) {
        Utils::printSerial(F("Reset JWT sub does not match bound sub."));
        Utils::printSerial(F("  bound sub: "), boundSub);
        Utils::printSerial(F("  JWT sub  : "), s_parsedSub);
        return false;
    }

    Utils::printSerial(F("Reset JWT validated successfully."));
    return true;
}

void AuthManager::loadAndVerifyBoundToken() {
    Utils::printSerial(F("\nChecking for persisted bound JWT..."));

    BoundTokenData data;
    if (!StorageManager::loadBoundToken(data)) {
        Utils::printSerial(F("\nNo bound JWT found — first-login not yet performed."));
        return;
    }

    if (data.sub[0] == '\0' || data.jwt[0] == '\0') {
        Utils::printSerial(F("\nBound token data empty — ignoring."));
        return;
    }

    // Verify signature only (no challenge check — the challenge is not stored)
    if (!verifyAndParseJWT(data.jwt, strlen(data.jwt), false)) {
        Utils::printSerial(F("\nBound JWT signature invalid — ignoring persisted identity."));
        return;
    }

    // Trust the sub that was written at bind time (already validated then)
    SessionManager::setBoundSub(data.sub);
    Utils::printSerial(F("\nBound identity restored — sub: "), data.sub);
}

void AuthManager::initPublicKey() {
#if defined(ARDUINO_ARCH_ESP8266)
    const char* pem      = Config::JWT_PUB_KEY;
    const char* b64Start = strstr(pem, "-----BEGIN PUBLIC KEY-----");
    if (!b64Start) { Utils::printSerial(F("\nPEM: no begin marker")); return; }
    b64Start += 26;
    const char* b64End = strstr(pem, "-----END PUBLIC KEY-----");
    if (!b64End)  { Utils::printSerial(F("\nPEM: no end marker")); return; }

    uint8_t keyDer[128];
    size_t  keyLen = base64Decode(b64Start, b64End - b64Start, keyDer, sizeof(keyDer));
    if (keyLen == 0) { Utils::printSerial(F("\nPEM: decode failed")); return; }
    Utils::printSerial(F("\nPEM: DER length="), keyLen);
    Utils::printSerial(F(""));

    for (size_t i = 0; i + 64 < keyLen; i++) {
        if (keyDer[i] == 0x04 && (keyLen - i - 1) >= 64) {
            s_pubKeyQ[0] = 0x04;
            memcpy(s_pubKeyQ + 1, &keyDer[i + 1], 64);
            s_brPubKey.curve = BR_EC_secp256r1;
            s_brPubKey.q     = s_pubKeyQ;
            s_brPubKey.qlen  = 65;
            s_pubKeyReady    = true;
            Utils::printSerial(F("PEM: key loaded, qlen="));
            Utils::printSerial(s_brPubKey.qlen);
            return;
        }
    }
    Utils::printSerial(F("PEM: EC point not found"));

#elif defined(ARDUINO_ARCH_ESP32)
    mbedtls_pk_init(&s_pkCtx);
    int ret = mbedtls_pk_parse_public_key(
        &s_pkCtx,
        (const unsigned char*)Config::JWT_PUB_KEY,
        strlen(Config::JWT_PUB_KEY) + 1
    );
    if (ret != 0) {
        char buf[80];
        mbedtls_strerror(ret, buf, sizeof(buf));
        Utils::printSerial(F("PEM parse failed: "));
        Utils::printSerial(buf);
        return;
    }
    s_pubKeyReady = true;
    Utils::printSerial(F("Public key loaded."));
#endif
}

bool AuthManager::verifyAndParseJWT(const char* jwt, size_t jwtLen, bool verifyChallenge) {
    // ── 1. Locate the two dots (once, no String allocations) ──
    const char* dot1 = (const char*)memchr(jwt, '.', jwtLen);
    if (!dot1) { Utils::printSerial(F("JWT: missing first dot")); return false; }
    size_t d1 = dot1 - jwt;

    const char* dot2 = (const char*)memchr(dot1 + 1, '.', jwtLen - d1 - 1);
    if (!dot2) { Utils::printSerial(F("JWT: missing second dot")); return false; }
    size_t d2 = dot2 - jwt;

    const char* sigB64 = jwt + d2 + 1;
    size_t      sigLen  = jwtLen - d2 - 1;
    while (sigLen > 0 && (uint8_t)sigB64[sigLen - 1] <= ' ') sigLen--;

    // All large buffers are static — ESP8266 cont stack is ~4 KB, requests are serial.
    // ── 2. Validate header alg=ES256 ──
    static uint8_t hdr[96];
    size_t hdrLen = b64urlDecodeLenient(jwt, d1, hdr, sizeof(hdr) - 1);
    if (hdrLen == 0) { Utils::printSerial(F("JWT: header decode failed")); return false; }
    hdr[hdrLen] = '\0';
    if (!strstr((char*)hdr, "ES256")) {
        Utils::printSerial(F("JWT: alg must be ES256"));
        return false;
    }

    // ── 3. Decode payload, extract claims (challenge + sub + family) ──────────
    //       Fast-fail challenge check happens before the expensive crypto.
    static uint8_t pay[256];
    // Clear parsed-claim output buffers
    s_parsedSub[0]    = '\0';
    s_parsedFamily[0] = '\0';

    size_t payLen = b64urlDecodeLenient(dot1 + 1, d2 - d1 - 1, pay, sizeof(pay) - 1);
    if (payLen == 0 || payLen >= sizeof(pay) - 1) {
        Utils::printSerial(F("JWT: payload decode failed"));
        return false;
    }
    pay[payLen] = '\0';

    {
        JsonDocument payDoc;
        if (deserializeJson(payDoc, (char*)pay) != DeserializationError::Ok) {
            Utils::printSerial(F("JWT: payload parse failed"));
            return false;
        }

        // Extract sub and family (may be absent)
        const char* rawSub    = payDoc["sub"]    | "";
        const char* rawFamily = payDoc["family"]  | "";
        strncpy(s_parsedSub,    rawSub,    sizeof(s_parsedSub)    - 1);
        strncpy(s_parsedFamily, rawFamily, sizeof(s_parsedFamily) - 1);
        s_parsedSub[sizeof(s_parsedSub)       - 1] = '\0';
        s_parsedFamily[sizeof(s_parsedFamily) - 1] = '\0';

        if (s_parsedSub[0] == '\0') {
            Utils::printSerial(F("JWT: requires a 'sub' claim"));
            return false;
        }

        // Challenge check — fast fail before expensive ECDSA
        if (verifyChallenge) {
            const char* challenge        = payDoc["challenge"] | "";
            const char* currentChallenge = SessionManager::getCurrentChallenge();
            Utils::printSerial(F("  token challenge : ["), challenge);
            Utils::printSerial(F("]\n  device challenge: ["), currentChallenge);
            Utils::printSerial(F("]"));

            if (challenge[0] == '\0' || strcmp(currentChallenge, challenge) != 0) {
                Utils::printSerial(F("JWT: invalid challenge"));
                return false;
            }
        }
    }  // payDoc freed here — before crypto

    // ── 4. SHA-256 over header.payload ──
    static uint8_t hash[32];
#if defined(ARDUINO_ARCH_ESP8266)
    static br_sha256_context shaCtx;  // ~220 bytes — must NOT be on stack
    br_sha256_init(&shaCtx);
    br_sha256_update(&shaCtx, jwt, d2);
    br_sha256_out(&shaCtx, hash);
#elif defined(ARDUINO_ARCH_ESP32)
    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);
    mbedtls_sha256_update(&shaCtx, (const unsigned char*)jwt, d2);
    mbedtls_sha256_finish(&shaCtx, hash);
    mbedtls_sha256_free(&shaCtx);
#endif

    // ── 5. Decode signature (strict) ──
    static uint8_t sig[64];
    size_t sigDecLen = b64urlDecodeStrict(sigB64, sigLen, sig, sizeof(sig));
    DEBUG_LOG_VAL("sig b64len", sigLen);
    DEBUG_LOG_VAL("sig decoded bytes", sigDecLen);

    if (sigDecLen != 64) {
        Utils::printSerial(F("JWT: invalid signature encoding"));
        return false;
    }

    // ── 6. ECDSA verify (most expensive — done last) ──
    DEBUG_LOG_VAL("pubKeyReady", s_pubKeyReady ? "true" : "false");
    if (!s_pubKeyReady) { Utils::printSerial(F("JWT: public key not loaded")); return false; }

#if defined(ARDUINO_ARCH_ESP8266)
    {
        char dbuf[32];
        snprintf(dbuf, sizeof(dbuf), "hash0..3: %02X%02X%02X%02X", hash[0], hash[1], hash[2], hash[3]);
        DEBUG_LOG(dbuf);
        snprintf(dbuf, sizeof(dbuf), "sig0..3:  %02X%02X%02X%02X", sig[0], sig[1], sig[2], sig[3]);
        DEBUG_LOG(dbuf);
        snprintf(dbuf, sizeof(dbuf), "pubQ0..3: %02X%02X%02X%02X", s_pubKeyQ[0], s_pubKeyQ[1], s_pubKeyQ[2], s_pubKeyQ[3]);
        DEBUG_LOG(dbuf);
    }
    const br_ec_impl* ec  = br_ec_get_default();
    if (!ec) { Utils::printSerial(F("JWT: no EC impl")); return false; }
    // Run BearSSL ECDSA verify on the heap-allocated thunk stack (5.6 KB)
    // to avoid overflowing the 4 KB cont stack.
    s_vrfyEc   = ec;
    s_vrfyHash = hash;
    s_vrfyKey  = &s_brPubKey;
    s_vrfySig  = sig;
    thunk_ecdsa_vrfy_on_heap_stack();
    uint32_t vrfyResult = s_vrfyResult;
    DEBUG_LOG_VAL("ECDSA verify result", vrfyResult == 1 ? "OK" : "FAILED");
    if (vrfyResult != 1) {
        Utils::printSerial(F("JWT: signature invalid"));
        return false;
    }
#elif defined(ARDUINO_ARCH_ESP32)
    uint8_t der[80];
    size_t  derLen = rawToAsn1Der(sig, 64, der, sizeof(der));
    if (derLen == 0) { Utils::printSerial(F("JWT: DER conversion failed")); return false; }
    if (mbedtls_pk_verify(&s_pkCtx, MBEDTLS_MD_SHA256, hash, 32, der, derLen) != 0) {
        Utils::printSerial(F("JWT: signature invalid"));
        return false;
    }
#endif

    Utils::printSerial(F("JWT: verified OK"));
    return true;
}
