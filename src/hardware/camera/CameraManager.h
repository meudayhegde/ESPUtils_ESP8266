#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <Arduino.h>
#include "board_config.h"

// Camera module is ESP32-only (requires esp_camera.h)
#if defined(ESP_CAM_ENABLED)
#include "esp_camera.h"
#include "../../config/Config.h"
#include "../../utils/Utils.h"

/**
 * @brief Manages ESP32 camera sensor initialisation.
 *
 * This is an ESP32-only hardware module. It wraps esp_camera_init() with
 * sensible defaults (JPEG / PSRAM-aware) and exposes helpers used by
 * CameraHandler to start the streaming HTTP server.
 */
class CameraManager {
public:
    /**
     * @brief Initialise the camera sensor.
     * @return true on success, false on failure.
     */
    static bool begin();

    /**
     * @brief Return whether the camera was successfully initialised.
     */
    static bool isInitialised();

private:
    static bool _initialised;
};

#endif  // ESP_CAM_ENABLED
#endif  // CAMERA_MANAGER_H
