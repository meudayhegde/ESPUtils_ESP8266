// ── Config.cpp ────────────────────────────────────────────────────────────────
// Single-definition translation unit for all extern string constants declared
// in Config.h.  Keeping large strings here prevents the linker from creating
// one copy per TU, saving flash on both ESP8266 and ESP32.
// ─────────────────────────────────────────────────────────────────────────────

#include "Config.h"

namespace Config {

    // Firmware version — substituted by create_ota_package.py at build time
    const char FIRMWARE_VERSION[] = "{{--@FIRMWARE_VERSION@--}}";

    // EC P-256 public key (PEM) — used to verify JWT signatures
    // Stored once here; all TUs reference this single copy via the extern decl.
    const char JWT_PUB_KEY[] =
        "-----BEGIN PUBLIC KEY-----\n"
        "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIAnXvd2yBqvGfsjTi4cAQ0hYkaRi\n"
        "/MqU8VSHyIBzErl8L2A2SERAXb6Epvjv4Zb5nu78LiIfkuB6gJvMj/fUrA==\n"
        "-----END PUBLIC KEY-----";

    // ── LittleFS file paths ───────────────────────────────────────────────────
    const char WIFI_CONFIG_FILE[]        = "/WiFiConfig.bin";
    const char LOGIN_CREDENTIAL_FILE[]   = "/LoginCredential.json";
    const char GPIO_CONFIG_FILE[]        = "/GPIOConfig.bin";
    const char SESSION_FILE[]            = "/Session.json";
    const char BOUND_TOKEN_FILE[]        = "/BoundToken.bin";
    const char SLEEP_CONFIG_FILE[]       = "/SleepConfig.bin";

} // namespace Config

// ── Response message tokens ───────────────────────────────────────────────────
namespace ResponseMsg {
    const char SUCCESS[]             = "success";
    const char FAILURE[]             = "failure";
    const char DENY[]                = "deny";
    const char UNAUTHORIZED[]        = "unauthorized";
    const char AUTHENTICATED[]       = "authenticated";
    const char INVALID_TOKEN[]       = "invalid_token";
    const char SESSION_EXPIRED[]     = "session_expired";
    const char TIMEOUT[]             = "timeout";
    const char PROGRESS[]            = "progress";
    const char UNDEFINED[]           = "undefined";
    const char INVALID_PURPOSE[]     = "Invalid Purpose";
    const char JSON_ERROR[]          = "JSON Error, failed to parse the request";
    const char PURPOSE_NOT_DEFINED[] = "Purpose not defined";
} // namespace ResponseMsg
