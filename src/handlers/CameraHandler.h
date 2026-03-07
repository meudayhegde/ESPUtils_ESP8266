#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "../platform/Platform.h"

// Camera module is only available on ESP32 boards with a camera model defined
#if defined(ESP_CAM_HW_EXIST)

#include <Arduino.h>
#include <WebServer.h>
#include "../config/Config.h"
#include "../utils/Utils.h"

typedef WebServer WebServerType;

/**
 * @brief Manages camera control routes and the MJPEG stream server.
 *
 * Camera control endpoints are registered on the main Arduino WebServer
 * (port 80) via setupRoutes().  The MJPEG stream runs on a dedicated
 * ESP-IDF httpd server on Config::CAMERA_STREAM_PORT (default 81).
 *
 * Exposed endpoints:
 *   GET  /          — Camera control web UI
 *   GET  /status    — Binary camera sensor status (BinCameraStatus + regs)
 *   POST /control   — Set sensor parameter (binary BinCameraControl body)
 *   GET  /capture   — Capture single JPEG
 *   GET  /bmp       — Capture single BMP
 *   GET  /xclk      — Set XCLK frequency
 *   GET  /reg, /greg, /pll, /resolution  — Low-level sensor registers
 *   GET  /stream    — MJPEG stream (on stream port 81)
 */
class CameraHandler {
public:
    /**
     * @brief Register camera control routes on the given WebServer.
     * Call this after CameraManager::begin() succeeds.
     */
    static void setupRoutes(WebServerType& server);

    /**
     * @brief Start the dedicated MJPEG stream server on CAMERA_STREAM_PORT.
     */
    static void startStreamServer();

    /**
     * @brief Initialise the LED flash (if a LED_GPIO_NUM is defined for the
     *        selected board model).
     */
    static void setupLedFlash();
};

#endif  // ESP_CAM_HW_EXIST
#endif  // CAMERA_HANDLER_H
