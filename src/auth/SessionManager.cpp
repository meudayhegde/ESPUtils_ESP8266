#include "SessionManager.h"
#include "../storage/StorageManager.h"
#include "../utils/Utils.h"
#include <ArduinoJson.h>

// Cryptographic includes
#ifdef ARDUINO_ARCH_ESP8266
    #include <bearssl/bearssl_hash.h>
    #include <bearssl/bearssl_ec.h>
    #include <bearssl/bearssl_pem.h>
    #include <WiFiClientSecureBearSSL.h>
#elif ARDUINO_ARCH_ESP32
    #include "mbedtls/ecdsa.h"
    #include "mbedtls/sha256.h"
    #include "mbedtls/pk.h"
    #include "mbedtls/error.h"
#endif

AuthSession SessionManager::currentSession;
String SessionManager::challengeString = "";
unsigned long SessionManager::challengeGeneratedTime = 0;

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
        if (c <= ' ') continue;          // skip whitespace / newlines
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

// Base64URL decode — strict (JWT signature only): rejects +// and enforces canonical trailing bits
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
    if (rawLen != 64) {
        return 0;
    }
    
    if (derMaxLen < 80) {  // Need at least 80 bytes for worst case
        return 0;
    }
    
    const uint8_t* r = rawSig;
    const uint8_t* s = rawSig + 32;
    
    // Helper function to encode an integer in ASN.1 DER
    auto encodeInteger = [](const uint8_t* value, size_t len, uint8_t* output) -> size_t {
        // Skip leading zeros but keep at least one byte
        size_t start = 0;
        while (start < len - 1 && value[start] == 0) {
            start++;
        }
        
        size_t actualLen = len - start;
        
        // Add padding byte if MSB is set (to keep number positive)
        bool needsPadding = (value[start] & 0x80) != 0;
        size_t totalLen = actualLen + (needsPadding ? 1 : 0);
        
        // Encode the INTEGER
        output[0] = 0x02;  // INTEGER tag
        output[1] = totalLen;  // Length (assuming < 128)
        
        size_t pos = 2;
        if (needsPadding) {
            output[pos++] = 0x00;
        }
        
        memcpy(&output[pos], &value[start], actualLen);
        return pos + actualLen;
    };
    
    // Encode r and s as ASN.1 integers  
    uint8_t rDer[35];  // Max: tag(1) + len(1) + padding(1) + data(32)
    uint8_t sDer[35];
    
    size_t rDerLen = encodeInteger(r, 32, rDer);
    size_t sDerLen = encodeInteger(s, 32, sDer);
    
    // Build the SEQUENCE
    size_t seqContentLen = rDerLen + sDerLen;
    
    if (seqContentLen >= 128) {
        // Need long form length encoding
        derSig[0] = 0x30;  // SEQUENCE tag
        derSig[1] = 0x81;  // Long form: 1 byte length
        derSig[2] = seqContentLen;
        
        memcpy(&derSig[3], rDer, rDerLen);
        memcpy(&derSig[3 + rDerLen], sDer, sDerLen);
        
        return 3 + seqContentLen;
    } else {
        // Short form length encoding
        derSig[0] = 0x30;  // SEQUENCE tag
        derSig[1] = seqContentLen;
        
        memcpy(&derSig[2], rDer, rDerLen);
        memcpy(&derSig[2 + rDerLen], sDer, sDerLen);
        
        return 2 + seqContentLen;
    }
}
#endif

void SessionManager::begin() {
    Utils::printSerial(F("## Initialize Session Manager."));
    
    // Parse and cache the EC public key once at boot
    initPublicKey();
    
    // Generate initial challenge string
    challengeString = generateChallengeString();
    challengeGeneratedTime = millis();
    Utils::printSerial(F("Initial challenge generated: "), challengeString.c_str());
    
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
    Utils::printSerial(F("Authenticating with JWT..."));
    
    if (!verifyAndParseJWT(jwtToken.c_str(), jwtToken.length())) {
        Utils::printSerial(F("JWT authentication failed."));
        return "";
    }
    
    String sessionToken      = generateSessionToken();
    currentSession.token      = sessionToken;
    currentSession.expiryTime = millis() / 1000 + Config::SESSION_EXPIRY_SECONDS;
    currentSession.isValid    = true;
    
    if (!saveSession()) {
        Utils::printSerial(F("Warning: session save failed."));
    } else {
        Utils::printSerial(F("Session created."));
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
    
    // Delete from flash storage
    StorageManager::deleteFile(Config::SESSION_FILE);
    
    Utils::printSerial(F("Session invalidated."));
}

AuthSession SessionManager::getCurrentSession() {
    return currentSession;
}

// ── Cached public key (parsed once at begin()) ──
#ifdef ARDUINO_ARCH_ESP8266
static bool             s_pubKeyReady = false;
static uint8_t          s_pubKeyQ[64];       // X||Y coordinates (64 bytes, no 0x04 prefix)
static br_ec_public_key s_brPubKey;
#elif ARDUINO_ARCH_ESP32
static bool               s_pubKeyReady = false;
static mbedtls_pk_context s_pkCtx;
#endif

void SessionManager::initPublicKey() {
#ifdef ARDUINO_ARCH_ESP8266
    const char* pem     = Config::JWT_PUB_KEY;
    const char* b64Start = strstr(pem, "-----BEGIN PUBLIC KEY-----");
    if (!b64Start) { Utils::printSerial(F("PEM: no begin marker")); return; }
    b64Start += 26;
    const char* b64End = strstr(pem, "-----END PUBLIC KEY-----");
    if (!b64End)  { Utils::printSerial(F("PEM: no end marker")); return; }

    uint8_t keyDer[128];
    size_t  keyLen = base64Decode(b64Start, b64End - b64Start, keyDer, sizeof(keyDer));
    if (keyLen == 0) { Utils::printSerial(F("PEM: decode failed")); return; }

    for (size_t i = 0; i + 64 < keyLen; i++) {
        if (keyDer[i] == 0x04 && (keyLen - i - 1) >= 64) {
            memcpy(s_pubKeyQ, &keyDer[i + 1], 64);
            s_brPubKey.curve = BR_EC_secp256r1;
            s_brPubKey.q     = s_pubKeyQ;
            s_brPubKey.qlen  = 64;
            s_pubKeyReady    = true;
            Utils::printSerial(F("Public key loaded."));
            return;
        }
    }
    Utils::printSerial(F("PEM: EC point not found"));

#elif ARDUINO_ARCH_ESP32
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

bool SessionManager::verifyAndParseJWT(const char* jwt, size_t jwtLen) {
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

    // ── 2. Validate header alg=ES256 (cheap — before any crypto) ──
    uint8_t hdr[96];
    size_t  hdrLen = b64urlDecodeLenient(jwt, d1, hdr, sizeof(hdr) - 1);
    if (hdrLen == 0) { Utils::printSerial(F("JWT: header decode failed")); return false; }
    hdr[hdrLen] = '\0';
    if (!strstr((char*)hdr, "ES256")) {
        Utils::printSerial(F("JWT: alg must be ES256"));
        return false;
    }

    // ── 3. Decode payload, validate challenge + exp/nbf (before SHA-256) ──
    uint8_t pay[256];
    size_t  payLen = b64urlDecodeLenient(dot1 + 1, d2 - d1 - 1, pay, sizeof(pay) - 1);
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

        // Challenge check — fast fail before expensive crypto
        const char* challenge = payDoc["challenge"] | "";
        if (challenge[0] == '\0' || challengeString != challenge) {
            Utils::printSerial(F("JWT: invalid challenge"));
            return false;
        }
    }  // payDoc freed here — before crypto

    // ── 4. SHA-256 over header.payload (d2 bytes covers header + '.' + payload) ──
    uint8_t hash[32];
#ifdef ARDUINO_ARCH_ESP8266
    br_sha256_context shaCtx;
    br_sha256_init(&shaCtx);
    br_sha256_update(&shaCtx, jwt, d2);
    br_sha256_out(&shaCtx, hash);
#elif ARDUINO_ARCH_ESP32
    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);
    mbedtls_sha256_update(&shaCtx, (const unsigned char*)jwt, d2);
    mbedtls_sha256_finish(&shaCtx, hash);
    mbedtls_sha256_free(&shaCtx);
#endif

    // ── 5. Decode signature (strict) ──
    uint8_t sig[64];
    if (b64urlDecodeStrict(sigB64, sigLen, sig, sizeof(sig)) != 64) {
        Utils::printSerial(F("JWT: invalid signature encoding"));
        return false;
    }

    // ── 6. ECDSA verify (most expensive — done last) ──
    if (!s_pubKeyReady) { Utils::printSerial(F("JWT: public key not loaded")); return false; }

#ifdef ARDUINO_ARCH_ESP8266
    const br_ec_impl* ec  = br_ec_get_default();
    br_ecdsa_vrfy     vrfy = br_ecdsa_vrfy_raw_get_default();
    if (!ec || !vrfy) { Utils::printSerial(F("JWT: no EC impl")); return false; }
    if (vrfy(ec, hash, 32, &s_brPubKey, sig, 64) != 1) {
        Utils::printSerial(F("JWT: signature invalid"));
        return false;
    }
#elif ARDUINO_ARCH_ESP32
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

String SessionManager::generateSessionToken() {
    // Single stack buffer — one String allocation, no intermediate heap copies
    char token[41];  // 8 hex (timestamp) + 32 hex (16 random bytes) + '\0'
    snprintf(token, 9, "%08lX", millis());
    for (int i = 0; i < 16; i++) {
        snprintf(token + 8 + i * 2, 3, "%02X", (uint8_t)random(256));
    }
    token[40] = '\0';
    return String(token);
}

bool SessionManager::saveSession() {
    JsonDocument doc;
    
    doc["token"] = currentSession.token;
    doc["expiry"] = currentSession.expiryTime;
    doc["valid"] = currentSession.isValid;

    return StorageManager::writeJson(Config::SESSION_FILE, doc);
}

bool SessionManager::loadSession() {
    JsonDocument doc;
    
    if (!StorageManager::readJson(Config::SESSION_FILE, doc)) {
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

String SessionManager::getCurrentChallenge() {
    // Check if challenge needs refresh
    updateChallenge();
    return challengeString;
}

void SessionManager::updateChallenge() {
    unsigned long currentTime = millis();
    
    // Handle millis() rollover
    bool needsRefresh = false;
    if (currentTime >= challengeGeneratedTime) {
        needsRefresh = (currentTime - challengeGeneratedTime) >= CHALLENGE_REFRESH_INTERVAL;
    } else {
        // Rollover occurred
        needsRefresh = true;
    }
    
    if (needsRefresh) {
        challengeString = generateChallengeString();
        challengeGeneratedTime = currentTime;
        Utils::printSerial(F("Challenge refreshed: "), challengeString.c_str());
    }
}

String SessionManager::generateChallengeString() {
    // Generate an 8-character random challenge string using alphanumeric characters
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const int charsetLength = sizeof(charset) - 1;
    const int challengeLength = 8;
    
    String challenge = "";
    
    for (int i = 0; i < challengeLength; i++) {
        challenge += charset[random(charsetLength)];
    }
    
    return challenge;
}
