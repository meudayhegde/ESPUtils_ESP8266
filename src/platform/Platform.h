#ifndef PLATFORM_H
#define PLATFORM_H

// ── Platform detection — always use defined() in conditional expressions ──────
#if defined(ARDUINO_ARCH_ESP8266)
    #define PLATFORM_ESP8266 1
    #define PLATFORM_ESP32   0
    #include <ESP8266WiFi.h>
    #include <ESP8266WebServer.h>
    typedef ESP8266WebServer WebServerType;
#elif defined(ARDUINO_ARCH_ESP32)
    #define PLATFORM_ESP8266 0
    #define PLATFORM_ESP32   1
    #include <WiFi.h>
    #include <WebServer.h>
    typedef WebServer WebServerType;

    // ── Camera board auto-detection (ESP32 only) ─────────────────────────────
    // Auto-detect CAMERA_MODEL_* from the board selected in Arduino IDE /
    // PlatformIO when no model has been set explicitly by the user.
    #if !defined(CAMERA_MODEL_WROVER_KIT)          && !defined(CAMERA_MODEL_ESP_EYE)              && \
        !defined(CAMERA_MODEL_ESP32S3_EYE)         && !defined(CAMERA_MODEL_M5STACK_PSRAM)        && \
        !defined(CAMERA_MODEL_M5STACK_V2_PSRAM)    && !defined(CAMERA_MODEL_M5STACK_WIDE)         && \
        !defined(CAMERA_MODEL_M5STACK_CAMS3_UNIT)  && !defined(CAMERA_MODEL_AI_THINKER)           && \
        !defined(CAMERA_MODEL_ESP32S3_CAM_LCD)     && !defined(CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3) && \
        !defined(CAMERA_MODEL_DFRobot_Romeo_ESP32S3) && !defined(CAMERA_MODEL_XIAO_ESP32S3)       && \
        !defined(CAMERA_MODEL_M5STACK_ESP32CAM)    && !defined(CAMERA_MODEL_M5STACK_UNITCAM)      && \
        !defined(CAMERA_MODEL_TTGO_T_JOURNAL)      && !defined(CAMERA_MODEL_ESP32_CAM_BOARD)      && \
        !defined(CAMERA_MODEL_ESP32S2_CAM_BOARD)

        #if   defined(ARDUINO_XIAO_ESP32S3)
            #define CAMERA_MODEL_XIAO_ESP32S3
        #elif defined(ARDUINO_ESP_WROVER_KIT) || defined(ARDUINO_ESP32_WROVER_KIT)
            #define CAMERA_MODEL_WROVER_KIT
        #elif defined(ARDUINO_ESP32S3_EYE)   || defined(ARDUINO_ESP32_S3_EYE)
            #define CAMERA_MODEL_ESP32S3_EYE
        #elif defined(ARDUINO_FREENOVE_ESP32S3_CAM)
            #define CAMERA_MODEL_ESP32S3_CAM_LCD
        #elif defined(ARDUINO_M5STACK_CAMS3_UNIT) || defined(ARDUINO_M5STACK_ATOMS3R)
            #define CAMERA_MODEL_M5STACK_CAMS3_UNIT
        #elif defined(ARDUINO_M5STACK_TIMER_CAM)  || defined(ARDUINO_M5STACK_UNITCAM)
            #define CAMERA_MODEL_M5STACK_UNITCAM
        #elif defined(ARDUINO_M5STACK_ESP32CAM)
            #define CAMERA_MODEL_M5STACK_ESP32CAM
        #elif defined(ARDUINO_DFROBOT_FIREBEETLE_2_ESP32S3)
            #define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3
        #elif defined(ARDUINO_DFROBOT_ROMEO_ESP32S3)
            #define CAMERA_MODEL_DFRobot_Romeo_ESP32S3
        #elif defined(ARDUINO_TTGO_T_JOURNAL)
            #define CAMERA_MODEL_TTGO_T_JOURNAL
        #elif defined(ARDUINO_ESP32_AI_THINKER) || defined(ARDUINO_ESP32CAM_AI_THINKER)
            #define CAMERA_MODEL_AI_THINKER
        #endif
    #endif  // auto-detect guard

    // Set ESP_CAM_HW_EXIST when any known camera model is present
    #if defined(CAMERA_MODEL_WROVER_KIT)          || defined(CAMERA_MODEL_ESP_EYE)              || \
        defined(CAMERA_MODEL_ESP32S3_EYE)         || defined(CAMERA_MODEL_M5STACK_PSRAM)        || \
        defined(CAMERA_MODEL_M5STACK_V2_PSRAM)    || defined(CAMERA_MODEL_M5STACK_WIDE)         || \
        defined(CAMERA_MODEL_M5STACK_CAMS3_UNIT)  || defined(CAMERA_MODEL_AI_THINKER)           || \
        defined(CAMERA_MODEL_ESP32S3_CAM_LCD)     || defined(CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3) || \
        defined(CAMERA_MODEL_DFRobot_Romeo_ESP32S3) || defined(CAMERA_MODEL_XIAO_ESP32S3)       || \
        defined(CAMERA_MODEL_M5STACK_ESP32CAM)    || defined(CAMERA_MODEL_M5STACK_UNITCAM)      || \
        defined(CAMERA_MODEL_TTGO_T_JOURNAL)      || defined(CAMERA_MODEL_ESP32_CAM_BOARD)      || \
        defined(CAMERA_MODEL_ESP32S2_CAM_BOARD)
        #define ESP_CAM_HW_EXIST
    #endif

    #include "../hardware/camera/CameraPins.h"
#else
    #error "Unsupported platform. Only ESP8266 and ESP32 are supported."
#endif

// ── Capability flags ──────────────────────────────────────────────────────────
#if defined(ARDUINO_ARCH_ESP32S2)
    #define PLATFORM_HAS_BLUETOOTH 0
#elif defined(ARDUINO_ARCH_ESP32)
    #define PLATFORM_HAS_BLUETOOTH 1
#else
    #define PLATFORM_HAS_BLUETOOTH 0
#endif

#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM)
    #define PLATFORM_HAS_PSRAM 1
#else
    #define PLATFORM_HAS_PSRAM 0
#endif

// ── yield() compatibility ─────────────────────────────────────────────────────
// ESP32 Arduino defines yield() as a no-op mapped to vTaskDelay(0).
// ESP8266 yield() feeds the RTOS and network stack — always call it in loops.
#if !defined(ARDUINO_ARCH_ESP8266) && !defined(ARDUINO_ARCH_ESP32)
    inline void yield() {}
#endif

// ── Device model detection ────────────────────────────────────────────────────
// Resolves PLATFORM_NAME (human-readable label) and PLATFORM_KEY (short ASCII
// identifier) from the board define set by Arduino IDE / PlatformIO.
// Both are plain string literals safe to pass directly to ArduinoJson or F().
// ─────────────────────────────────────────────────────────────────────────────

#if defined(ARDUINO_ARCH_ESP8266)

    // ── ESP8266 boards ────────────────────────────────────────────────────────
    #if   defined(ARDUINO_ESP8266_ESP01)
        #define PLATFORM_NAME "ESP-01"
        #define PLATFORM_KEY  "ESP01"
    #elif defined(ARDUINO_ESP8266_ESP01S)
        #define PLATFORM_NAME "ESP-01S"
        #define PLATFORM_KEY  "ESP01S"
    #elif defined(ARDUINO_ESP8266_ESP07)
        #define PLATFORM_NAME "ESP-07"
        #define PLATFORM_KEY  "ESP07"
    #elif defined(ARDUINO_ESP8266_ESP07S)
        #define PLATFORM_NAME "ESP-07S"
        #define PLATFORM_KEY  "ESP07S"
    #elif defined(ARDUINO_ESP8266_ESP12)
        #define PLATFORM_NAME "ESP-12E"
        #define PLATFORM_KEY  "ESP12E"
    #elif defined(ARDUINO_ESP8266_ESP12F)
        #define PLATFORM_NAME "ESP-12F"
        #define PLATFORM_KEY  "ESP12F"
    #elif defined(ARDUINO_ESP8266_NODEMCU_ESP12E) || defined(ARDUINO_ESP8266_NODEMCU)
        #define PLATFORM_NAME "NodeMCU 1.0 (ESP-12E)"
        #define PLATFORM_KEY  "NODEMCU_ESP12E"
    #elif defined(ARDUINO_ESP8266_NODEMCU_ESP8266MOD)
        #define PLATFORM_NAME "NodeMCU 0.9 (ESP-12)"
        #define PLATFORM_KEY  "NODEMCU_ESP12"
    #elif defined(ARDUINO_ESP8266_WEMOS_D1MINIPRO) || defined(ARDUINO_ESP8266_D1MINI_PRO)
        #define PLATFORM_NAME "Wemos D1 Mini Pro"
        #define PLATFORM_KEY  "D1MINI_PRO"
    #elif defined(ARDUINO_ESP8266_WEMOS_D1MINILITE)
        #define PLATFORM_NAME "Wemos D1 Mini Lite"
        #define PLATFORM_KEY  "D1MINI_LITE"
    #elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
        #define PLATFORM_NAME "Wemos D1 Mini"
        #define PLATFORM_KEY  "D1MINI"
    #elif defined(ARDUINO_ESP8266_WEMOS_D1R1)
        #define PLATFORM_NAME "Wemos D1 R1"
        #define PLATFORM_KEY  "D1R1"
    #elif defined(ARDUINO_ADAFRUIT_HUZZAH_ESP8266)
        #define PLATFORM_NAME "Adafruit Feather HUZZAH"
        #define PLATFORM_KEY  "HUZZAH_ESP8266"
    #elif defined(ARDUINO_ESP8266_THING_DEV)
        #define PLATFORM_NAME "SparkFun ESP8266 Thing Dev"
        #define PLATFORM_KEY  "SF_THING_DEV_8266"
    #elif defined(ARDUINO_ESP8266_THING)
        #define PLATFORM_NAME "SparkFun ESP8266 Thing"
        #define PLATFORM_KEY  "SF_THING_8266"
    #elif defined(ARDUINO_ESP8266_ESPDUINO)
        #define PLATFORM_NAME "ESPDuino (ESP-13)"
        #define PLATFORM_KEY  "ESPDUINO"
    #elif defined(ARDUINO_ESP8266_ESPRESSO_LITE_V1)
        #define PLATFORM_NAME "ESPresso Lite 1.0"
        #define PLATFORM_KEY  "ESPRESSO_LITE_V1"
    #elif defined(ARDUINO_ESP8266_ESPRESSO_LITE_V2)
        #define PLATFORM_NAME "ESPresso Lite 2.0"
        #define PLATFORM_KEY  "ESPRESSO_LITE_V2"
    #elif defined(ARDUINO_ESP8266_ESP210)
        #define PLATFORM_NAME "SweetPea ESP-210"
        #define PLATFORM_KEY  "ESP210"
    #elif defined(ARDUINO_ESP8266_PHOENIX_V1)
        #define PLATFORM_NAME "Phoenix 1.0"
        #define PLATFORM_KEY  "PHOENIX_V1"
    #elif defined(ARDUINO_ESP8266_PHOENIX_V2)
        #define PLATFORM_NAME "Phoenix 2.0"
        #define PLATFORM_KEY  "PHOENIX_V2"
    #elif defined(ARDUINO_ESP8266_ESP8285)
        #define PLATFORM_NAME "Generic ESP8285"
        #define PLATFORM_KEY  "ESP8285"
    #elif defined(ARDUINO_ESP8266_WIFIO)
        #define PLATFORM_NAME "WifIO"
        #define PLATFORM_KEY  "WIFIO"
    #elif defined(ARDUINO_ESP8266_OAK)
        #define PLATFORM_NAME "Digistump Oak"
        #define PLATFORM_KEY  "OAK"
    #elif defined(ARDUINO_ESP8266_INVENTONE)
        #define PLATFORM_NAME "Invent One"
        #define PLATFORM_KEY  "INVENTONE"
    #elif defined(ARDUINO_MOD_WIFI_ESP8266)
        #define PLATFORM_NAME "Olimex MOD-WIFI-ESP8266"
        #define PLATFORM_KEY  "MODWIFI_ESP8266"
    #elif defined(ARDUINO_WIFI_KIT_8)
        #define PLATFORM_NAME "Heltec WiFi Kit 8"
        #define PLATFORM_KEY  "WIFI_KIT_8"
    #elif defined(ARDUINO_ESP8266_GENERIC)
        #define PLATFORM_NAME "Generic ESP8266"
        #define PLATFORM_KEY  "ESP8266_GENERIC"
    #else
        #define PLATFORM_NAME "ESP8266 (unknown board)"
        #define PLATFORM_KEY  "ESP8266"
    #endif

#elif defined(ARDUINO_ARCH_ESP32)

    // ── ESP32 original (Xtensa dual-core) ────────────────────────────────────
    #if   defined(ARDUINO_ESP32_WROVER_KIT) || defined(ARDUINO_ESP_WROVER_KIT)
        #define PLATFORM_NAME "ESP32-WROVER-KIT"
        #define PLATFORM_KEY  "ESP32_WROVER_KIT"
    #elif defined(ARDUINO_LOLIN32)
        #define PLATFORM_NAME "LOLIN32"
        #define PLATFORM_KEY  "LOLIN32"
    #elif defined(ARDUINO_LOLIN32_LITE)
        #define PLATFORM_NAME "LOLIN32 Lite"
        #define PLATFORM_KEY  "LOLIN32_LITE"
    #elif defined(ARDUINO_D1_MINI32)
        #define PLATFORM_NAME "Wemos D1 Mini32"
        #define PLATFORM_KEY  "D1_MINI32"
    #elif defined(ARDUINO_NODE32S)
        #define PLATFORM_NAME "NodeMCU-32S"
        #define PLATFORM_KEY  "NODEMCU32S"
    #elif defined(ARDUINO_MH_ET_LIVE_ESP32MINIKIT)
        #define PLATFORM_NAME "MH ET LIVE ESP32MiniKit"
        #define PLATFORM_KEY  "MH_ET_MINIKIT"
    #elif defined(ARDUINO_NANO_ESP32) || defined(ARDUINO_ARDUINO_NANO_ESP32)
        #define PLATFORM_NAME "Arduino Nano ESP32"
        #define PLATFORM_KEY  "NANO_ESP32"
    #elif defined(ARDUINO_ESP32_PICO)
        #define PLATFORM_NAME "ESP32-Pico-D4"
        #define PLATFORM_KEY  "ESP32_PICO_D4"
    #elif defined(ARDUINO_ESP32_POE_ISO)
        #define PLATFORM_NAME "Olimex ESP32-POE-ISO"
        #define PLATFORM_KEY  "ESP32_POE_ISO"
    #elif defined(ARDUINO_ESP32_POE)
        #define PLATFORM_NAME "Olimex ESP32-POE"
        #define PLATFORM_KEY  "ESP32_POE"
    #elif defined(ARDUINO_TTGO_T_OI_PLUS_DEV)
        #define PLATFORM_NAME "TTGO T-OI Plus"
        #define PLATFORM_KEY  "TTGO_T_OI_PLUS"
    #elif defined(ARDUINO_LILYGO_T_DISPLAY)
        #define PLATFORM_NAME "LilyGo TTGO T-Display"
        #define PLATFORM_KEY  "TTGO_T_DISPLAY"
    #elif defined(ARDUINO_TTGO_T1)
        #define PLATFORM_NAME "TTGO T1"
        #define PLATFORM_KEY  "TTGO_T1"
    #elif defined(ARDUINO_TTGO_T_JOURNAL)
        #define PLATFORM_NAME "TTGO T-Journal"
        #define PLATFORM_KEY  "TTGO_T_JOURNAL"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32_V2)
        #define PLATFORM_NAME "Adafruit Feather ESP32 V2"
        #define PLATFORM_KEY  "FEATHER_ESP32_V2"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32)
        #define PLATFORM_NAME "Adafruit Feather ESP32"
        #define PLATFORM_KEY  "FEATHER_ESP32"
    #elif defined(ARDUINO_ADAFRUIT_QTPY_ESP32_PICO)
        #define PLATFORM_NAME "Adafruit QT Py ESP32"
        #define PLATFORM_KEY  "QTPY_ESP32"
    #elif defined(ARDUINO_SPARKFUN_ESP32_THING_PLUS_C)
        #define PLATFORM_NAME "SparkFun ESP32 Thing Plus C"
        #define PLATFORM_KEY  "SF_ESP32_THING_PLUS_C"
    #elif defined(ARDUINO_SPARKFUN_ESP32_THING_PLUS)
        #define PLATFORM_NAME "SparkFun ESP32 Thing Plus"
        #define PLATFORM_KEY  "SF_ESP32_THING_PLUS"
    #elif defined(ARDUINO_SPARKFUN_ESP32_THING)
        #define PLATFORM_NAME "SparkFun ESP32 Thing"
        #define PLATFORM_KEY  "SF_ESP32_THING"
    #elif defined(ARDUINO_M5STACK_FIRE)
        #define PLATFORM_NAME "M5Stack Fire"
        #define PLATFORM_KEY  "M5_FIRE"
    #elif defined(ARDUINO_M5STACK_CORE2)
        #define PLATFORM_NAME "M5Stack Core2"
        #define PLATFORM_KEY  "M5_CORE2"
    #elif defined(ARDUINO_M5STACK_CORE_ESP32)
        #define PLATFORM_NAME "M5Stack Core ESP32"
        #define PLATFORM_KEY  "M5_CORE"
    #elif defined(ARDUINO_M5Stack_StickC_Plus) || defined(ARDUINO_M5STACK_STICKC_PLUS)
        #define PLATFORM_NAME "M5Stick-C Plus"
        #define PLATFORM_KEY  "M5_STICK_C_PLUS"
    #elif defined(ARDUINO_M5Stack_StickC)
        #define PLATFORM_NAME "M5Stick-C"
        #define PLATFORM_KEY  "M5_STICK_C"
    #elif defined(ARDUINO_M5STACK_ATOM)
        #define PLATFORM_NAME "M5Stack Atom"
        #define PLATFORM_KEY  "M5_ATOM"
    #elif defined(ARDUINO_HORNBILL_ESP32_DEV)
        #define PLATFORM_NAME "Hornbill ESP32 Dev"
        #define PLATFORM_KEY  "HORNBILL_ESP32_DEV"
    #elif defined(ARDUINO_HORNBILL_ESP32_MINIMA)
        #define PLATFORM_NAME "Hornbill ESP32 Minima"
        #define PLATFORM_KEY  "HORNBILL_ESP32_MINIMA"
    #elif defined(ARDUINO_HELTEC_WIFI_KIT_32)
        #define PLATFORM_NAME "Heltec WiFi Kit 32"
        #define PLATFORM_KEY  "HELTEC_WIFI_KIT_32"
    #elif defined(ARDUINO_HELTEC_WIFI_LORA_32)
        #define PLATFORM_NAME "Heltec WiFi LoRa 32"
        #define PLATFORM_KEY  "HELTEC_WIFI_LORA_32"
    #elif defined(ARDUINO_HELTEC_WIRELESS_STICK)
        #define PLATFORM_NAME "Heltec Wireless Stick"
        #define PLATFORM_KEY  "HELTEC_WIRELESS_STICK"

    // ── ESP32-S2 ──────────────────────────────────────────────────────────────
    #elif defined(ARDUINO_LOLIN_S2_MINI)
        #define PLATFORM_NAME "LOLIN S2 Mini"
        #define PLATFORM_KEY  "LOLIN_S2_MINI"
    #elif defined(ARDUINO_LOLIN_S2_PICO)
        #define PLATFORM_NAME "LOLIN S2 Pico"
        #define PLATFORM_KEY  "LOLIN_S2_PICO"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2_NOPSRAM)
        #define PLATFORM_NAME "Adafruit Feather ESP32-S2 No PSRAM"
        #define PLATFORM_KEY  "FEATHER_ESP32S2_NOPSRAM"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
        #define PLATFORM_NAME "Adafruit Feather ESP32-S2"
        #define PLATFORM_KEY  "FEATHER_ESP32S2"
    #elif defined(ARDUINO_ADAFRUIT_MAGTAG29_ESP32S2)
        #define PLATFORM_NAME "Adafruit MagTag"
        #define PLATFORM_KEY  "MAGTAG_ESP32S2"
    #elif defined(ARDUINO_ADAFRUIT_METRO_ESP32S2)
        #define PLATFORM_NAME "Adafruit Metro ESP32-S2"
        #define PLATFORM_KEY  "METRO_ESP32S2"
    #elif defined(ARDUINO_ADAFRUIT_QTPY_ESP32S2)
        #define PLATFORM_NAME "Adafruit QT Py ESP32-S2"
        #define PLATFORM_KEY  "QTPY_ESP32S2"
    #elif defined(ARDUINO_DFROBOT_FIREBEETLE_2_ESP32S2)
        #define PLATFORM_NAME "DFRobot FireBeetle 2 ESP32-S2"
        #define PLATFORM_KEY  "FIREBEETLE2_ESP32S2"
    #elif defined(ARDUINO_UM_TINYS2)
        #define PLATFORM_NAME "UM TinyS2"
        #define PLATFORM_KEY  "UM_TINYS2"
    #elif defined(ARDUINO_ESP32S2_DEV)
        #define PLATFORM_NAME "ESP32-S2-DevKitC"
        #define PLATFORM_KEY  "ESP32S2_DEVKITC"

    // ── ESP32-S3 ──────────────────────────────────────────────────────────────
    #elif defined(ARDUINO_XIAO_ESP32S3)
        #define PLATFORM_NAME "Seeed XIAO ESP32S3"
        #define PLATFORM_KEY  "XIAO_ESP32S3"
    #elif defined(ARDUINO_ESP32S3_EYE) || defined(ARDUINO_ESP32_S3_EYE)
        #define PLATFORM_NAME "ESP32-S3-EYE"
        #define PLATFORM_KEY  "ESP32S3_EYE"
    #elif defined(ARDUINO_FREENOVE_ESP32S3_CAM)
        #define PLATFORM_NAME "Freenove ESP32-S3 CAM"
        #define PLATFORM_KEY  "FREENOVE_ESP32S3_CAM"
    #elif defined(ARDUINO_M5STACK_CAMS3_UNIT)
        #define PLATFORM_NAME "M5Stack CamS3 Unit"
        #define PLATFORM_KEY  "M5_CAMS3"
    #elif defined(ARDUINO_M5STACK_ATOMS3R)
        #define PLATFORM_NAME "M5Stack AtomS3R"
        #define PLATFORM_KEY  "M5_ATOMS3R"
    #elif defined(ARDUINO_M5STACK_ATOMS3)
        #define PLATFORM_NAME "M5Stack AtomS3"
        #define PLATFORM_KEY  "M5_ATOMS3"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S3_NOPSRAM)
        #define PLATFORM_NAME "Adafruit Feather ESP32-S3 No PSRAM"
        #define PLATFORM_KEY  "FEATHER_ESP32S3_NOPSRAM"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S3)
        #define PLATFORM_NAME "Adafruit Feather ESP32-S3"
        #define PLATFORM_KEY  "FEATHER_ESP32S3"
    #elif defined(ARDUINO_ADAFRUIT_QTPY_ESP32S3_NOPSRAM)
        #define PLATFORM_NAME "Adafruit QT Py ESP32-S3"
        #define PLATFORM_KEY  "QTPY_ESP32S3"
    #elif defined(ARDUINO_DFROBOT_FIREBEETLE_2_ESP32S3)
        #define PLATFORM_NAME "DFRobot FireBeetle 2 ESP32-S3"
        #define PLATFORM_KEY  "FIREBEETLE2_ESP32S3"
    #elif defined(ARDUINO_DFROBOT_ROMEO_ESP32S3)
        #define PLATFORM_NAME "DFRobot Romeo ESP32-S3"
        #define PLATFORM_KEY  "ROMEO_ESP32S3"
    #elif defined(ARDUINO_LILYGO_T_DISPLAY_S3)
        #define PLATFORM_NAME "LilyGo T-Display S3"
        #define PLATFORM_KEY  "T_DISPLAY_S3"
    #elif defined(ARDUINO_LILYGO_T7_S3)
        #define PLATFORM_NAME "LilyGo T7 S3"
        #define PLATFORM_KEY  "LILYGO_T7_S3"
    #elif defined(ARDUINO_WAVESHARE_ESP32S3_DEV)
        #define PLATFORM_NAME "Waveshare ESP32-S3-DEV"
        #define PLATFORM_KEY  "WS_ESP32S3_DEV"
    #elif defined(ARDUINO_UM_TINYS3)
        #define PLATFORM_NAME "UM TinyS3"
        #define PLATFORM_KEY  "UM_TINYS3"
    #elif defined(ARDUINO_UM_FEATHERS3NEO)
        #define PLATFORM_NAME "UM FeatherS3 Neo"
        #define PLATFORM_KEY  "UM_FEATHERS3_NEO"
    #elif defined(ARDUINO_UM_FEATHERS3)
        #define PLATFORM_NAME "UM FeatherS3"
        #define PLATFORM_KEY  "UM_FEATHERS3"
    #elif defined(ARDUINO_UM_PROS3)
        #define PLATFORM_NAME "UM ProS3"
        #define PLATFORM_KEY  "UM_PROS3"
    #elif defined(ARDUINO_ESP32S3_DEV)
        #define PLATFORM_NAME "ESP32-S3-DevKitC"
        #define PLATFORM_KEY  "ESP32S3_DEVKITC"

    // ── ESP32-C3 ──────────────────────────────────────────────────────────────
    #elif defined(ARDUINO_XIAO_ESP32C3)
        #define PLATFORM_NAME "Seeed XIAO ESP32C3"
        #define PLATFORM_KEY  "XIAO_ESP32C3"
    #elif defined(ARDUINO_ADAFRUIT_QTPY_ESP32C3)
        #define PLATFORM_NAME "Adafruit QT Py ESP32-C3"
        #define PLATFORM_KEY  "QTPY_ESP32C3"
    #elif defined(ARDUINO_LOLIN_C3_MINI)
        #define PLATFORM_NAME "LOLIN C3 Mini"
        #define PLATFORM_KEY  "LOLIN_C3_MINI"
    #elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32_C3)
        #define PLATFORM_NAME "DFRobot FireBeetle ESP32-C3"
        #define PLATFORM_KEY  "FIREBEETLE_ESP32C3"
    #elif defined(ARDUINO_AirM2M_CORE_ESP32C3)
        #define PLATFORM_NAME "AirM2M CORE ESP32-C3"
        #define PLATFORM_KEY  "AIRM2M_ESP32C3"
    #elif defined(ARDUINO_ESP32C3_DEV)
        #define PLATFORM_NAME "ESP32-C3-DevKitM"
        #define PLATFORM_KEY  "ESP32C3_DEVKITM"

    // ── ESP32-C6 ──────────────────────────────────────────────────────────────
    #elif defined(ARDUINO_XIAO_ESP32C6)
        #define PLATFORM_NAME "Seeed XIAO ESP32C6"
        #define PLATFORM_KEY  "XIAO_ESP32C6"
    #elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32C6)
        #define PLATFORM_NAME "Adafruit Feather ESP32-C6"
        #define PLATFORM_KEY  "FEATHER_ESP32C6"
    #elif defined(ARDUINO_WAVESHARE_ESP32C6_DEV)
        #define PLATFORM_NAME "Waveshare ESP32-C6-DEV"
        #define PLATFORM_KEY  "WS_ESP32C6_DEV"
    #elif defined(ARDUINO_ESP32C6_DEV)
        #define PLATFORM_NAME "ESP32-C6-DevKitC"
        #define PLATFORM_KEY  "ESP32C6_DEVKITC"

    // ── ESP32-H2 ──────────────────────────────────────────────────────────────
    #elif defined(ARDUINO_ESP32H2_DEV)
        #define PLATFORM_NAME "ESP32-H2-DevKitM"
        #define PLATFORM_KEY  "ESP32H2_DEVKITM"

    // ── ESP32-C2 (ESP8684) ────────────────────────────────────────────────────
    #elif defined(ARDUINO_ESP32C2_DEV)
        #define PLATFORM_NAME "ESP32-C2-DevKitM"
        #define PLATFORM_KEY  "ESP32C2_DEVKITM"

    // ── Generic ESP32 fallback ────────────────────────────────────────────────
    #elif defined(ARDUINO_ESP32_DEV)
        #define PLATFORM_NAME "ESP32-WROOM-32"
        #define PLATFORM_KEY  "ESP32_WROOM32"
    #else
        // Last-resort fallback: use IDF target string if available
        #if   defined(CONFIG_IDF_TARGET_ESP32S3)
            #define PLATFORM_NAME "ESP32-S3 (unknown board)"
            #define PLATFORM_KEY  "ESP32S3"
        #elif defined(CONFIG_IDF_TARGET_ESP32S2)
            #define PLATFORM_NAME "ESP32-S2 (unknown board)"
            #define PLATFORM_KEY  "ESP32S2"
        #elif defined(CONFIG_IDF_TARGET_ESP32C3)
            #define PLATFORM_NAME "ESP32-C3 (unknown board)"
            #define PLATFORM_KEY  "ESP32C3"
        #elif defined(CONFIG_IDF_TARGET_ESP32C6)
            #define PLATFORM_NAME "ESP32-C6 (unknown board)"
            #define PLATFORM_KEY  "ESP32C6"
        #elif defined(CONFIG_IDF_TARGET_ESP32H2)
            #define PLATFORM_NAME "ESP32-H2 (unknown board)"
            #define PLATFORM_KEY  "ESP32H2"
        #else
            #define PLATFORM_NAME "ESP32 (unknown board)"
            #define PLATFORM_KEY  "ESP32"
        #endif
    #endif

#endif  // PLATFORM_NAME / PLATFORM_KEY detection

#endif // PLATFORM_H
