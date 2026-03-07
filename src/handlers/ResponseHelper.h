#ifndef RESPONSE_HELPER_H
#define RESPONSE_HELPER_H

#include "../platform/Platform.h"

// ── Inline HTTP response helpers ─────────────────────────────────────────────
//
// Provides CORS and OPTIONS handling shared by all endpoint handlers.
// Binary response helpers are in BinaryHelper.h.

// ── CORS ─────────────────────────────────────────────────────────────────────
// The frontend is hosted separately (not on the ESP), so every REST response
// must include permissive CORS headers for browser-based access.

inline void sendCorsHeaders(WebServerType& server) {
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, DELETE, OPTIONS"));
    server.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, Authorization, Accept"));
    server.sendHeader(F("Access-Control-Max-Age"), F("86400"));
}

// Handle CORS preflight OPTIONS request.
inline void handleCorsOptions(WebServerType& server) {
    sendCorsHeaders(server);
    server.send(204);
}

#endif // RESPONSE_HELPER_H
