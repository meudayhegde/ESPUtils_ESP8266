#ifndef SAFE_STRING_H
#define SAFE_STRING_H

#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

// ── safe_strncpy ──────────────────────────────────────────────────────────────
// Copies src into dst, writing at most (size - 1) characters and ALWAYS
// null-terminating dst.  Safer than strncpy which may leave dst unterminated.
//
// @param dst   Destination buffer
// @param src   Source C-string (may be NULL — treated as empty)
// @param size  Total size of destination buffer INCLUDING the null terminator
inline void safe_strncpy(char* dst, const char* src, size_t size) {
    if (!dst || size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

// ── safe_snprintf ─────────────────────────────────────────────────────────────
// Wrapper around vsnprintf that ALWAYS null-terminates dst and handles
// negative return values from vsnprintf on failure.
//
// @return Number of bytes written (excluding null), capped at size - 1.
inline int safe_snprintf(char* dst, size_t size, const char* fmt, ...) {
    if (!dst || size == 0) return 0;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(dst, size, fmt, args);
    va_end(args);
    if (n < 0) n = 0;
    dst[size - 1] = '\0';   // guarantee termination even if buffer too small
    return (n < (int)(size - 1)) ? n : (int)(size - 1);
}

#endif // SAFE_STRING_H
