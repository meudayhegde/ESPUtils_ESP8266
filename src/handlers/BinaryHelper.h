#ifndef BINARY_HELPER_H
#define BINARY_HELPER_H

// ════════════════════════════════════════════════════════════════════════
// Binary Response / Request Helpers
//
// Replaces the JSON-based helpers in ResponseHelper.h.
// All helpers are inline to avoid a separate .cpp unit.
// ════════════════════════════════════════════════════════════════════════

#include "../platform/Platform.h"
#include "../protocol/BinaryProtocol.h"
#include <string.h>

// ── CORS (shared with JSON helpers — kept in ResponseHelper.h) ───────────────
// Re-use sendCorsHeaders() from ResponseHelper.h (still included for OPTIONS).

// Forward-declare sendCorsHeaders if ResponseHelper.h isn't included
#ifndef RESPONSE_HELPER_H
inline void sendCorsHeaders(WebServerType& server) {
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, DELETE, OPTIONS"));
    server.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, Authorization, Accept"));
    server.sendHeader(F("Access-Control-Max-Age"), F("86400"));
}
#endif

// ── Send a binary response ───────────────────────────────────────────────────

/**
 * @brief Send a packed binary struct as the HTTP response body.
 * @param server  WebServer instance
 * @param code    HTTP status code
 * @param data    Pointer to packed struct data
 * @param len     Size of data in bytes
 */
inline void sendBinaryResponse(WebServerType& server, int code,
                                const void* data, size_t len) {
    sendCorsHeaders(server);
    server.setContentLength(len);
    server.send(code, F("application/octet-stream"), "");
    server.sendContent(reinterpret_cast<const char*>(data), len);
}

/**
 * @brief Send a binary error response (BinErrorResponse struct).
 * @param server  WebServer instance
 * @param code    HTTP status code
 * @param status  BinStatus enum value
 * @param message Error message string
 */
inline void sendBinaryError(WebServerType& server, int code,
                             uint8_t status, const char* message) {
    BinErrorResponse resp;
    resp.status = status;
    memset(resp.error, 0, sizeof(resp.error));
    strncpy(resp.error, message, sizeof(resp.error) - 1);
    sendBinaryResponse(server, code, &resp, sizeof(resp));
}

// ── Read binary request body ─────────────────────────────────────────────────

/**
 * @brief Stub handler passed as the upload/raw function when registering
 *        POST/PUT routes.  Its presence causes the WebServer to capture the
 *        raw body via readBytes() into HTTPRaw.buf instead of the
 *        String(plainBuf) path which truncates binary data at null bytes.
 *
 * Usage:  server.on("/path", HTTP_POST, handler, rawBodyStub);
 */
inline void rawBodyStub() { /* intentionally empty */ }

/**
 * @brief Read the raw request body into a fixed-size struct buffer.
 *
 * Requires the route to be registered with rawBodyStub as the upload
 * function so that the WebServer uses its raw-body path (HTTPRaw) rather
 * than server.arg("plain") which truncates at embedded null bytes.
 *
 * @param server  WebServer instance
 * @param dest    Destination buffer (must be large enough)
 * @param maxLen  Maximum bytes to read (sizeof(struct))
 * @return Number of bytes actually read, or 0 on failure
 */
inline size_t readBinaryBody(WebServerType& server, void* dest, size_t maxLen) {
    memset(dest, 0, maxLen);

    // Primary path: read from HTTPRaw buffer (binary-safe, no null truncation).
    // This requires the route to have been registered with rawBodyStub as _ufn.
    HTTPRaw& raw = server.raw();
    if (raw.totalSize > 0) {
        size_t copyLen = (raw.totalSize < maxLen) ? raw.totalSize : maxLen;
        memcpy(dest, raw.buf, copyLen);
        return copyLen;
    }

    // Fallback: read from arg("plain") for routes that were not registered
    // with rawBodyStub.  This path is NOT binary-safe for data with null bytes.
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        size_t bodyLen = body.length();
        if (bodyLen == 0) return 0;
        size_t copyLen = (bodyLen < maxLen) ? bodyLen : maxLen;
        memcpy(dest, body.c_str(), copyLen);
        return copyLen;
    }

    return 0;
}

/**
 * @brief Helper to copy a C-string into a fixed-size char field with NUL padding.
 */
inline void copyToField(char* dst, const char* src, size_t fieldSize) {
    memset(dst, 0, fieldSize);
    if (src) {
        strncpy(dst, src, fieldSize - 1);
    }
}

#endif // BINARY_HELPER_H
