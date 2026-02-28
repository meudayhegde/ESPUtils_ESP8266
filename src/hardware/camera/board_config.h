#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================
// Camera Board Selection
//
// Auto-detected from the board chosen in Arduino IDE / PlatformIO
// when possible.  If your board isn't matched below, uncomment
// exactly ONE of the CAMERA_MODEL_* defines manually.
// ============================================================

// ── Auto-detect from Arduino board macros ──
#if !defined(CAMERA_MODEL_WROVER_KIT) && !defined(CAMERA_MODEL_ESP_EYE) && \
    !defined(CAMERA_MODEL_ESP32S3_EYE) && !defined(CAMERA_MODEL_M5STACK_PSRAM) && \
    !defined(CAMERA_MODEL_M5STACK_V2_PSRAM) && !defined(CAMERA_MODEL_M5STACK_WIDE) && \
    !defined(CAMERA_MODEL_M5STACK_CAMS3_UNIT) && !defined(CAMERA_MODEL_AI_THINKER) && \
    !defined(CAMERA_MODEL_ESP32S3_CAM_LCD) && !defined(CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3) && \
    !defined(CAMERA_MODEL_DFRobot_Romeo_ESP32S3) && !defined(CAMERA_MODEL_XIAO_ESP32S3) && \
    !defined(CAMERA_MODEL_M5STACK_ESP32CAM) && !defined(CAMERA_MODEL_M5STACK_UNITCAM) && \
    !defined(CAMERA_MODEL_TTGO_T_JOURNAL) && !defined(CAMERA_MODEL_ESP32_CAM_BOARD) && \
    !defined(CAMERA_MODEL_ESP32S2_CAM_BOARD)

  // Seeed XIAO ESP32S3 (Sense variant with camera)
  #if defined(ARDUINO_XIAO_ESP32S3)
    #define CAMERA_MODEL_XIAO_ESP32S3

  // Espressif ESP-WROVER-KIT
  #elif defined(ARDUINO_ESP_WROVER_KIT) || defined(ARDUINO_ESP32_WROVER_KIT)
    #define CAMERA_MODEL_WROVER_KIT

  // Espressif ESP-EYE
  #elif defined(ARDUINO_ESP32S3_EYE) || defined(ARDUINO_ESP32_S3_EYE)
    #define CAMERA_MODEL_ESP32S3_EYE

  // Freenove ESP32-S3 CAM
  #elif defined(ARDUINO_FREENOVE_ESP32S3_CAM)
    #define CAMERA_MODEL_ESP32S3_CAM_LCD

  // M5Stack boards
  #elif defined(ARDUINO_M5STACK_CAMS3_UNIT) || defined(ARDUINO_M5STACK_ATOMS3R)
    #define CAMERA_MODEL_M5STACK_CAMS3_UNIT
  #elif defined(ARDUINO_M5STACK_TIMER_CAM) || defined(ARDUINO_M5STACK_UNITCAM)
    #define CAMERA_MODEL_M5STACK_UNITCAM
  #elif defined(ARDUINO_M5STACK_ESP32CAM)
    #define CAMERA_MODEL_M5STACK_ESP32CAM

  // DFRobot FireBeetle 2 / Romeo ESP32-S3
  #elif defined(ARDUINO_DFROBOT_FIREBEETLE_2_ESP32S3)
    #define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3
  #elif defined(ARDUINO_DFROBOT_ROMEO_ESP32S3)
    #define CAMERA_MODEL_DFRobot_Romeo_ESP32S3

  // TTGO T-Journal
  #elif defined(ARDUINO_TTGO_T_JOURNAL)
    #define CAMERA_MODEL_TTGO_T_JOURNAL

  // AI-Thinker ESP32-CAM (most common generic ESP32 camera)
  #elif defined(ARDUINO_ESP32_AI_THINKER) || defined(ARDUINO_ESP32CAM_AI_THINKER)
    #define CAMERA_MODEL_AI_THINKER
  #endif

  #if defined(CAMERA_MODEL_WROVER_KIT) || defined(CAMERA_MODEL_ESP_EYE) || \
    defined(CAMERA_MODEL_ESP32S3_EYE) || defined(CAMERA_MODEL_M5STACK_PSRAM) || \
    defined(CAMERA_MODEL_M5STACK_V2_PSRAM) || defined(CAMERA_MODEL_M5STACK_WIDE) || \
    defined(CAMERA_MODEL_M5STACK_CAMS3_UNIT) || defined(CAMERA_MODEL_AI_THINKER) || \
    defined(CAMERA_MODEL_ESP32S3_CAM_LCD) || defined(CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3) || \
    defined(CAMERA_MODEL_DFRobot_Romeo_ESP32S3) || defined(CAMERA_MODEL_XIAO_ESP32S3) || \
    defined(CAMERA_MODEL_M5STACK_ESP32CAM) || defined(CAMERA_MODEL_M5STACK_UNITCAM) || \
    defined(CAMERA_MODEL_TTGO_T_JOURNAL) || defined(CAMERA_MODEL_ESP32_CAM_BOARD) || \
    defined(CAMERA_MODEL_ESP32S2_CAM_BOARD)

    #define ESP_CAM_ENABLED
  #endif

#endif  // auto-detect guard

#include "camera_pins.h"

#endif  // BOARD_CONFIG_H
