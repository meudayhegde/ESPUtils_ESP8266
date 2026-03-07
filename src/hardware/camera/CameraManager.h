#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <Arduino.h>
#include "../../platform/Platform.h"

// Camera module is ESP32-only (requires esp_camera.h)
#if defined(ESP_CAM_HW_EXIST)
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
     * @brief Return whether the camera was successfully initialised at least once.
     */
    static bool isInitialised();

    /**
     * @brief Re-initialise the camera sensor after it has been disabled.
     *        The camera must have been successfully initialised via begin() first.
     * @return true on success, false on failure.
     */
    static bool enable();

    /**
     * @brief De-initialise the camera sensor to save power and CPU.
     *        Call enable() to bring it back online.
     */
    static void disable();

    /**
     * @brief Return true if the camera is currently active (not de-initialised).
     */
    static bool isEnabled();

private:
    static bool _initialised;  ///< Set once when begin() succeeds
    static bool _enabled;      ///< Tracks current power/active state
};

#endif  // ESP_CAM_HW_EXIST
#endif  // CAMERA_MANAGER_H
