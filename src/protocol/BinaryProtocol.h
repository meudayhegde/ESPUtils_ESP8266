#ifndef BINARY_PROTOCOL_H
#define BINARY_PROTOCOL_H

// ════════════════════════════════════════════════════════════════════════
// AetherPulse Binary Wire Protocol
//
// All structs are little-endian, packed (no alignment padding).
// These definitions are the single source of truth for the on-the-wire
// binary format between the Next.js client and ESP device.
//
// Client-side: mirrored in lib/api/binary-protocol.ts using DataView.
// ════════════════════════════════════════════════════════════════════════

#include <stdint.h>

#pragma pack(push, 1)

// ── Response status byte (first byte of every response) ──────────────────────

enum BinStatus : uint8_t {
    BIN_STATUS_OK           = 0,
    BIN_STATUS_ERROR        = 1,
    BIN_STATUS_UNAUTHORIZED = 2,
    BIN_STATUS_SCANNING     = 3,
    BIN_STATUS_TIMEOUT      = 4,
    BIN_STATUS_PROGRESS     = 5,
    BIN_STATUS_RESTARTING   = 6,
};

// ── Error response (any endpoint on failure) ─────────────────────────────────

struct BinErrorResponse {
    uint8_t status;      // BinStatus (non-zero)
    char    error[64];   // NUL-terminated error message
};

// ── GET /ping ────────────────────────────────────────────────────────────────

struct BinPingResponse {
    char    deviceID[20];       // hex chip-ID string
    char    ipAddress[64];      // IP address string
    char    deviceName[12];     // "AetherPulse"
    char    challenge[9];       // 8 alphanumeric chars + NUL
    uint8_t isBound;            // 0 = false, 1 = true
    char    platform_name[32];  // e.g. "ESP32-C3 SuperMini"
    char    platform_key[24];   // e.g. "esp32c3-supermini"
};
// Total: 20+64+12+9+1+32+24 = 162 bytes

// ── POST /api/auth ───────────────────────────────────────────────────────────

struct BinAuthRequest {
    char token[512];  // JWT (ES256), NUL-terminated
};

struct BinAuthResponse {
    uint8_t  status;            // BIN_STATUS_OK or BIN_STATUS_ERROR
    char     sessionToken[41];  // 40 hex chars + NUL
    uint32_t expiresIn;         // seconds (604800 = 1 week)
    char     error[64];         // NUL-terminated (empty on success)
};
// Total: 1+41+4+64 = 110 bytes

// ── GET /api/device ──────────────────────────────────────────────────────────

struct BinDeviceInfoResponse {
    char     deviceName[12];
    char     deviceID[20];
    char     macAddress[18];    // "XX:XX:XX:XX:XX:XX" + NUL
    char     ipAddress[64];
    char     platform[8];       // "ESP8266" or "ESP32"
    uint32_t deviceIDDecimal;
    char     platform_name[32];
    char     platform_key[24];
    char     wirelessMode[8];   // "AP", "WIFI", "AP_STA"
    uint8_t  isBound;
    uint8_t  sleepEnabled;      // 0 = disabled, 1 = enabled
};
// Total: 12+20+18+64+8+4+32+24+8+1+1 = 192 bytes

// ── GPIO ─────────────────────────────────────────────────────────────────────

struct BinGpioPin {
    int32_t pinNumber;
    uint8_t pinMode;    // GPIOPinMode enum (0-3)
    int32_t pinValue;
};
// Total: 4+1+4 = 9 bytes (matches GPIOConfig memory layout)

struct BinGpioSetRequest {
    int32_t pinNumber;
    uint8_t pinMode;
    int32_t pinValue;
};
// Total: 9 bytes

struct BinGpioSetResponse {
    uint8_t status;     // BIN_STATUS_OK or BIN_STATUS_ERROR
    int32_t pinValue;
    char    error[64];  // NUL-terminated (empty on success)
};

// BinGpioGetResponse is variable-length:
//   uint8_t status;   (1 byte)
//   uint8_t count;    (1 byte — 0 for single pin, N for all)
//   BinGpioPin pins[count];  (count × 9 bytes)

struct BinGpioGetHeader {
    uint8_t status;
    uint8_t count;
};

// ── Wireless ─────────────────────────────────────────────────────────────────

struct BinWirelessGetResponse {
    char wireless_mode[8];  // "AP", "WIFI", "AP_STA"
    char station_ssid[33];
    char station_psk[8];    // masked "****"
    char ap_ssid[33];
    char ap_psk[8];         // masked "****"
};
// Total: 8+33+8+33+8 = 90 bytes

struct BinWirelessSetRequest {
    char wireless_mode[8];
    char wifi_ssid[33];
    char wifi_password[64];
    char ap_ssid[33];
    char ap_password[64];
};
// Total: 202 bytes (same as WirelessConfig)

struct BinWirelessSetResponse {
    uint8_t status;
    char    message[64];
    char    wireless_mode[8];
};

// ── WiFi Scan ────────────────────────────────────────────────────────────────

struct BinNetworkInfo {
    char    ssid[33];
    char    bssid[18];
    int32_t rssi;
    uint8_t encrypted;  // 0 = open, 1 = encrypted
};
// Total: 33+18+4+1 = 56 bytes

// BinWifiScanResponse is variable-length:
//   uint8_t status;   (BIN_STATUS_OK = complete, BIN_STATUS_SCANNING = in-progress)
//   uint8_t count;    (number of networks, 0 if scanning)
//   BinNetworkInfo networks[count];

struct BinWifiScanHeader {
    uint8_t status;
    uint8_t count;
};

// ── IR Capture (SSE events — base64-encoded in data: field) ──────────────────

enum BinIrEventType : uint8_t {
    BIN_IR_EVENT_PROGRESS = 0,
    BIN_IR_EVENT_CAPTURE  = 1,
    BIN_IR_EVENT_TIMEOUT  = 2,
    BIN_IR_EVENT_ERROR    = 3,
};

// Progress event (sent during countdown)
struct BinIrProgressEvent {
    uint8_t eventType;  // BIN_IR_EVENT_PROGRESS
    uint8_t value;      // countdown seconds remaining
};

// Timeout event
struct BinIrTimeoutEvent {
    uint8_t eventType;  // BIN_IR_EVENT_TIMEOUT
};

// Capture result header (variable-length: irCode data follows)
// Wire format: BinIrCaptureEventHeader + irCodeLen bytes of irCode data
struct BinIrCaptureEventHeader {
    uint8_t  eventType;      // BIN_IR_EVENT_CAPTURE
    char     protocol[16];   // NUL-terminated protocol name
    uint16_t bitLength;      // bit count or raw length
    uint16_t irCodeLen;      // length of irCode data that follows (bytes)
};

struct BinIrCaptureRequest {
    uint8_t captureMode;  // 0 = single, 1 = multi
};

// ── IR Send ──────────────────────────────────────────────────────────────────

// Wire format: BinIrSendHeader + irCodeLen bytes of irCode data
struct BinIrSendHeader {
    char     protocol[16];  // NUL-terminated protocol name
    uint16_t bitLength;     // parsed as hex on old API; now direct uint16
    uint16_t irCodeLen;     // length of irCode string data that follows
};

struct BinIrSendResponse {
    uint8_t status;
    char    response[80];  // e.g. "NEC success"
};

// ── Camera ───────────────────────────────────────────────────────────────────

struct BinCameraEnableRequest {
    uint8_t enabled;  // 0 = disable, 1 = enable
};

struct BinCameraEnableResponse {
    uint8_t status;
    uint8_t enabled;
};

// Camera status: fixed sensor settings + variable register entries
struct BinCameraStatus {
    uint16_t xclk;
    uint8_t  pixformat;
    uint8_t  framesize;
    uint8_t  quality;
    int8_t   brightness;
    int8_t   contrast;
    int8_t   saturation;
    int8_t   sharpness;
    uint8_t  special_effect;
    uint8_t  wb_mode;
    uint8_t  awb;
    uint8_t  awb_gain;
    uint8_t  aec;
    uint8_t  aec2;
    int8_t   ae_level;
    uint16_t aec_value;
    uint8_t  agc;
    uint8_t  agc_gain;
    uint8_t  gainceiling;
    uint8_t  bpc;
    uint8_t  wpc;
    uint8_t  raw_gma;
    uint8_t  lenc;
    uint8_t  hmirror;
    uint8_t  vflip;
    uint8_t  dcw;
    uint8_t  colorbar;
    int16_t  led_intensity;   // -1 if LED not available
    uint8_t  regCount;        // number of BinCameraRegEntry that follow
};
// Total: ~32 bytes + regCount × BinCameraRegEntry

struct BinCameraRegEntry {
    uint16_t addr;
    int32_t  value;
};
// Total: 6 bytes each

// Camera control: set a sensor parameter by ID
enum BinCameraVarId : uint8_t {
    CAM_VAR_FRAMESIZE     = 0,
    CAM_VAR_QUALITY       = 1,
    CAM_VAR_CONTRAST      = 2,
    CAM_VAR_BRIGHTNESS    = 3,
    CAM_VAR_SATURATION    = 4,
    CAM_VAR_GAINCEILING   = 5,
    CAM_VAR_COLORBAR      = 6,
    CAM_VAR_AWB           = 7,
    CAM_VAR_AGC           = 8,
    CAM_VAR_AEC           = 9,
    CAM_VAR_HMIRROR       = 10,
    CAM_VAR_VFLIP         = 11,
    CAM_VAR_AWB_GAIN      = 12,
    CAM_VAR_AGC_GAIN      = 13,
    CAM_VAR_AEC_VALUE     = 14,
    CAM_VAR_AEC2          = 15,
    CAM_VAR_DCW           = 16,
    CAM_VAR_BPC           = 17,
    CAM_VAR_WPC           = 18,
    CAM_VAR_RAW_GMA       = 19,
    CAM_VAR_LENC          = 20,
    CAM_VAR_SPECIAL_EFFECT = 21,
    CAM_VAR_WB_MODE       = 22,
    CAM_VAR_AE_LEVEL      = 23,
    CAM_VAR_LED_INTENSITY = 24,
};

struct BinCameraControl {
    uint8_t varId;   // BinCameraVarId
    int32_t value;
};
// Total: 5 bytes

// ── Sleep mode ───────────────────────────────────────────────────────────────

struct BinSleepRequest {
    uint8_t enabled;  // 0 = disable sleep, 1 = enable sleep
};

struct BinSleepResponse {
    uint8_t status;   // BIN_STATUS_OK or BIN_STATUS_ERROR
    uint8_t enabled;  // current sleep mode state after applying
};

// ── Simple responses (restart, reset) ────────────────────────────────────────

struct BinSimpleResponse {
    uint8_t status;  // BIN_STATUS_OK, BIN_STATUS_ERROR, BIN_STATUS_RESTARTING
};

#pragma pack(pop)

// ── Base64 utilities (for SSE binary events) ─────────────────────────────────

namespace Base64 {

static const char TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Encode binary data to base64 in-place into a caller-provided buffer.
 * @param src  Source binary data
 * @param len  Length of source data
 * @param dst  Destination buffer (must be at least ((len+2)/3)*4 + 1 bytes)
 * @return Length of base64 string (not including NUL terminator)
 */
inline size_t encode(const uint8_t* src, size_t len, char* dst) {
    size_t i = 0, j = 0;
    for (; i + 2 < len; i += 3) {
        dst[j++] = TABLE[(src[i] >> 2) & 0x3F];
        dst[j++] = TABLE[((src[i] & 0x03) << 4) | ((src[i+1] >> 4) & 0x0F)];
        dst[j++] = TABLE[((src[i+1] & 0x0F) << 2) | ((src[i+2] >> 6) & 0x03)];
        dst[j++] = TABLE[src[i+2] & 0x3F];
    }
    if (i < len) {
        dst[j++] = TABLE[(src[i] >> 2) & 0x3F];
        if (i + 1 < len) {
            dst[j++] = TABLE[((src[i] & 0x03) << 4) | ((src[i+1] >> 4) & 0x0F)];
            dst[j++] = TABLE[((src[i+1] & 0x0F) << 2)];
        } else {
            dst[j++] = TABLE[((src[i] & 0x03) << 4)];
            dst[j++] = '=';
        }
        dst[j++] = '=';
    }
    dst[j] = '\0';
    return j;
}

} // namespace Base64

#endif // BINARY_PROTOCOL_H
