// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// This file is an adapted version of the ESP32 CameraWebServer example
// (app_httpd.cpp), wrapped in the CameraHandler class and integrated with
// the ESPUtils modular architecture.
//
// Camera control routes are registered on the main Arduino WebServer
// (port 80).  Only the MJPEG stream uses a dedicated ESP-IDF httpd server
// on CAMERA_STREAM_PORT (81) because the chunked multipart streaming
// requires the low-level httpd API.

#include "CameraHandler.h"
#include "../hardware/camera/CameraManager.h"
#include "BinaryHelper.h"

// CameraHandler.h sets
// ESP_CAM_HW_EXIST when a recognised camera board is selected.
// camera-equipped board is selected.  No camera → entire file is compiled out.
#if defined(ESP_CAM_HW_EXIST)

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "../platform/Platform.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// -----------------------------------------------------------------------
// LED Flash support
// -----------------------------------------------------------------------
#if defined(LED_GPIO_NUM)
#define CONFIG_LED_MAX_INTENSITY 255
static int  led_duty    = 0;
static bool isStreaming = false;
#endif

// -----------------------------------------------------------------------
// Stream constants (MJPEG — used only by stream_handler on port 81)
// -----------------------------------------------------------------------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

// Only the stream server uses the ESP-IDF httpd handle
static httpd_handle_t stream_httpd = NULL;

// -----------------------------------------------------------------------
// Running-average filter (used to smooth FPS display)
// -----------------------------------------------------------------------
typedef struct {
    size_t size;
    size_t index;
    size_t count;
    int    sum;
    int   *values;
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size) {
    memset(filter, 0, sizeof(ra_filter_t));
    filter->values = (int *)malloc(sample_size * sizeof(int));
    if (!filter->values) return NULL;
    memset(filter->values, 0, sample_size * sizeof(int));
    filter->size = sample_size;
    return filter;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t *filter, int value) {
    if (!filter->values) return value;
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index = (filter->index + 1) % filter->size;
    if (filter->count < filter->size) filter->count++;
    return filter->sum / filter->count;
}
#endif

// -----------------------------------------------------------------------
// LED helpers
// -----------------------------------------------------------------------
#if defined(LED_GPIO_NUM)
static void enable_led(bool en) {
    int duty = en ? led_duty : 0;
    if (en && isStreaming && (led_duty > CONFIG_LED_MAX_INTENSITY))
        duty = CONFIG_LED_MAX_INTENSITY;
    ledcWrite(LED_GPIO_NUM, duty);
    log_i("Set LED intensity to %d", duty);
}
#endif

// -----------------------------------------------------------------------
// Helper: read an integer query-parameter with a default
// -----------------------------------------------------------------------
static int getQueryInt(WebServerType& server, const char* key, int defaultVal) {
    if (!server.hasArg(key)) return defaultVal;
    return server.arg(key).toInt();
}

// -----------------------------------------------------------------------
// Helper: print_reg (used by status handler)
// -----------------------------------------------------------------------
static int print_reg(char *p, size_t remaining, sensor_t *s, uint16_t reg, uint32_t mask) {
    return snprintf(p, remaining, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

// -----------------------------------------------------------------------
// Camera control handlers (Arduino WebServer, port 80)
// -----------------------------------------------------------------------

static void handleStatus(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    // Build binary camera status + register entries
    // Max registers: 46 (OV5640/OV3660) + 3 (OV2640) = 49 max
    static uint8_t statusBuf[sizeof(BinCameraStatus) + 50 * sizeof(BinCameraRegEntry)];
    
    sensor_t *s = esp_camera_sensor_get();
    BinCameraStatus* status = reinterpret_cast<BinCameraStatus*>(statusBuf);
    memset(status, 0, sizeof(BinCameraStatus));
    
    status->xclk           = s->xclk_freq_hz / 1000000;
    status->pixformat      = (uint8_t)s->pixformat;
    status->framesize      = (uint8_t)s->status.framesize;
    status->quality        = (uint8_t)s->status.quality;
    status->brightness     = (int8_t)s->status.brightness;
    status->contrast       = (int8_t)s->status.contrast;
    status->saturation     = (int8_t)s->status.saturation;
    status->sharpness      = (int8_t)s->status.sharpness;
    status->special_effect = (uint8_t)s->status.special_effect;
    status->wb_mode        = (uint8_t)s->status.wb_mode;
    status->awb            = (uint8_t)s->status.awb;
    status->awb_gain       = (uint8_t)s->status.awb_gain;
    status->aec            = (uint8_t)s->status.aec;
    status->aec2           = (uint8_t)s->status.aec2;
    status->ae_level       = (int8_t)s->status.ae_level;
    status->aec_value      = (uint16_t)s->status.aec_value;
    status->agc            = (uint8_t)s->status.agc;
    status->agc_gain       = (uint8_t)s->status.agc_gain;
    status->gainceiling    = (uint8_t)s->status.gainceiling;
    status->bpc            = (uint8_t)s->status.bpc;
    status->wpc            = (uint8_t)s->status.wpc;
    status->raw_gma        = (uint8_t)s->status.raw_gma;
    status->lenc           = (uint8_t)s->status.lenc;
    status->hmirror        = (uint8_t)s->status.hmirror;
    status->vflip          = (uint8_t)s->status.vflip;
    status->dcw            = (uint8_t)s->status.dcw;
    status->colorbar       = (uint8_t)s->status.colorbar;
#if defined(LED_GPIO_NUM)
    status->led_intensity  = (int16_t)led_duty;
#else
    status->led_intensity  = -1;
#endif
    
    // Append sensor-specific register entries
    BinCameraRegEntry* regs = reinterpret_cast<BinCameraRegEntry*>(statusBuf + sizeof(BinCameraStatus));
    uint8_t regCount = 0;
    
    if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
        // OV5640/OV3660 registers
        for (int reg = 0x3400; reg < 0x3406; reg += 2) {
            regs[regCount].addr = (uint16_t)reg;
            regs[regCount].value = (int32_t)s->get_reg(s, reg, 0xFFF);
            regCount++;
        }
        static const uint16_t singleRegs[] = {0x3406, 0x3500, 0x3503, 0x350a, 0x350c};
        static const uint32_t singleMasks[] = {0xFF, 0xFFFF0, 0xFF, 0x3FF, 0xFFFF};
        for (int i = 0; i < 5; i++) {
            regs[regCount].addr = singleRegs[i];
            regs[regCount].value = (int32_t)s->get_reg(s, singleRegs[i], singleMasks[i]);
            regCount++;
        }
        for (int reg = 0x5480; reg <= 0x5490; reg++) {
            regs[regCount].addr = (uint16_t)reg;
            regs[regCount].value = (int32_t)s->get_reg(s, reg, 0xFF);
            regCount++;
        }
        for (int reg = 0x5380; reg <= 0x538b; reg++) {
            regs[regCount].addr = (uint16_t)reg;
            regs[regCount].value = (int32_t)s->get_reg(s, reg, 0xFF);
            regCount++;
        }
        for (int reg = 0x5580; reg < 0x558a; reg++) {
            regs[regCount].addr = (uint16_t)reg;
            regs[regCount].value = (int32_t)s->get_reg(s, reg, 0xFF);
            regCount++;
        }
        regs[regCount].addr = 0x558a;
        regs[regCount].value = (int32_t)s->get_reg(s, 0x558a, 0x1FF);
        regCount++;
    } else if (s->id.PID == OV2640_PID) {
        static const uint16_t ov2640Regs[] = {0xd3, 0x111, 0x132};
        for (int i = 0; i < 3; i++) {
            regs[regCount].addr = ov2640Regs[i];
            regs[regCount].value = (int32_t)s->get_reg(s, ov2640Regs[i], 0xFF);
            regCount++;
        }
    }
    
    status->regCount = regCount;
    size_t totalLen = sizeof(BinCameraStatus) + regCount * sizeof(BinCameraRegEntry);
    sendBinaryResponse(server, 200, statusBuf, totalLen);
}

static void handleCmd(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    
    // Read binary control struct from body
    BinCameraControl ctrl;
    size_t bytesRead = readBinaryBody(server, &ctrl, sizeof(ctrl));
    if (bytesRead < sizeof(ctrl)) {
        sendBinaryError(server, 400, BIN_STATUS_ERROR, "Invalid control data");
        return;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    int val = ctrl.value;
    int res = 0;

    switch (ctrl.varId) {
        case CAM_VAR_FRAMESIZE:
            if (s->pixformat == PIXFORMAT_JPEG)
                res = s->set_framesize(s, (framesize_t)val);
            break;
        case CAM_VAR_QUALITY:        res = s->set_quality(s, val); break;
        case CAM_VAR_CONTRAST:       res = s->set_contrast(s, val); break;
        case CAM_VAR_BRIGHTNESS:     res = s->set_brightness(s, val); break;
        case CAM_VAR_SATURATION:     res = s->set_saturation(s, val); break;
        case CAM_VAR_GAINCEILING:    res = s->set_gainceiling(s, (gainceiling_t)val); break;
        case CAM_VAR_COLORBAR:       res = s->set_colorbar(s, val); break;
        case CAM_VAR_AWB:            res = s->set_whitebal(s, val); break;
        case CAM_VAR_AGC:            res = s->set_gain_ctrl(s, val); break;
        case CAM_VAR_AEC:            res = s->set_exposure_ctrl(s, val); break;
        case CAM_VAR_HMIRROR:        res = s->set_hmirror(s, val); break;
        case CAM_VAR_VFLIP:          res = s->set_vflip(s, val); break;
        case CAM_VAR_AWB_GAIN:       res = s->set_awb_gain(s, val); break;
        case CAM_VAR_AGC_GAIN:       res = s->set_agc_gain(s, val); break;
        case CAM_VAR_AEC_VALUE:      res = s->set_aec_value(s, val); break;
        case CAM_VAR_AEC2:           res = s->set_aec2(s, val); break;
        case CAM_VAR_DCW:            res = s->set_dcw(s, val); break;
        case CAM_VAR_BPC:            res = s->set_bpc(s, val); break;
        case CAM_VAR_WPC:            res = s->set_wpc(s, val); break;
        case CAM_VAR_RAW_GMA:        res = s->set_raw_gma(s, val); break;
        case CAM_VAR_LENC:           res = s->set_lenc(s, val); break;
        case CAM_VAR_SPECIAL_EFFECT: res = s->set_special_effect(s, val); break;
        case CAM_VAR_WB_MODE:        res = s->set_wb_mode(s, val); break;
        case CAM_VAR_AE_LEVEL:       res = s->set_ae_level(s, val); break;
#if defined(LED_GPIO_NUM)
        case CAM_VAR_LED_INTENSITY:
            led_duty = val;
            if (isStreaming) enable_led(true);
            break;
#endif
        default:
            log_i("Unknown camera varId: %u", ctrl.varId);
            res = -1;
            break;
    }

    if (res < 0) {
        sendBinaryError(server, 500, BIN_STATUS_ERROR, "Control failed");
        return;
    }
    BinSimpleResponse ok;
    ok.status = BIN_STATUS_OK;
    sendBinaryResponse(server, 200, &ok, sizeof(ok));
}

static void handleCapture(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    camera_fb_t *fb = NULL;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    int64_t fr_start = esp_timer_get_time();
#endif

#if defined(LED_GPIO_NUM)
    enable_led(true);
    vTaskDelay(150 / portTICK_PERIOD_MS);
    fb = esp_camera_fb_get();
    enable_led(false);
#else
    fb = esp_camera_fb_get();
#endif

    if (!fb) {
        log_e("Camera capture failed");
        server.send(500, "text/plain", "Camera capture failed");
        return;
    }

    server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (fb->format == PIXFORMAT_JPEG) {
        server.setContentLength(fb->len);
        server.send(200, "image/jpeg", "");
        server.sendContent((const char *)fb->buf, fb->len);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        int64_t fr_end = esp_timer_get_time();
        log_i("JPG: %uB %ums", (uint32_t)(fb->len), (uint32_t)((fr_end - fr_start) / 1000));
#endif
        esp_camera_fb_return(fb);
    } else {
        uint8_t *jpg_buf = NULL;
        size_t   jpg_len = 0;
        bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);
        if (!converted) {
            log_e("JPEG conversion failed");
            server.send(500, "text/plain", "JPEG conversion failed");
            return;
        }
        server.setContentLength(jpg_len);
        server.send(200, "image/jpeg", "");
        server.sendContent((const char *)jpg_buf, jpg_len);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        int64_t fr_end = esp_timer_get_time();
        log_i("JPG: %uB %ums", (uint32_t)(jpg_len), (uint32_t)((fr_end - fr_start) / 1000));
#endif
        free(jpg_buf);
    }
}

static void handleBmp(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint64_t fr_start = esp_timer_get_time();
#endif
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        server.send(500, "text/plain", "Camera capture failed");
        return;
    }

    uint8_t *buf     = NULL;
    size_t   buf_len = 0;
    bool converted = frame2bmp(fb, &buf, &buf_len);
    esp_camera_fb_return(fb);

    if (!converted) {
        log_e("BMP conversion failed");
        server.send(500, "text/plain", "BMP conversion failed");
        return;
    }

    server.sendHeader("Content-Disposition", "inline; filename=capture.bmp");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.setContentLength(buf_len);
    server.send(200, "image/x-windows-bmp", "");
    server.sendContent((const char *)buf, buf_len);
    free(buf);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint64_t fr_end = esp_timer_get_time();
    log_i("BMP: %llums, %uB", (uint64_t)((fr_end - fr_start) / 1000), buf_len);
#endif
}

static void handleXclk(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    if (!server.hasArg("xclk")) {
        server.send(404, "text/plain", "Variable not found");
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    int res = s->set_xclk(s, LEDC_TIMER_0, server.arg("xclk").toInt());
    if (res) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
}

static void handleReg(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    if (!server.hasArg("reg") || !server.hasArg("mask") || !server.hasArg("val")) {
        server.send(404, "text/plain", "Variable not found");
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    int res = s->set_reg(s, server.arg("reg").toInt(),
                         server.arg("mask").toInt(),
                         server.arg("val").toInt());
    if (res) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
}

static void handleGreg(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    if (!server.hasArg("reg") || !server.hasArg("mask")) {
        server.send(404, "text/plain", "Variable not found");
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    int res = s->get_reg(s, server.arg("reg").toInt(), server.arg("mask").toInt());
    if (res < 0) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", String(res));
}

static void handlePll(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    int bypass = getQueryInt(server, "bypass", 0);
    int mul    = getQueryInt(server, "mul",    0);
    int sys    = getQueryInt(server, "sys",    0);
    int root   = getQueryInt(server, "root",   0);
    int pre    = getQueryInt(server, "pre",    0);
    int seld5  = getQueryInt(server, "seld5",  0);
    int pclken = getQueryInt(server, "pclken", 0);
    int pclk   = getQueryInt(server, "pclk",   0);
    sensor_t *s = esp_camera_sensor_get();
    int res = s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk);
    if (res) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
}

static void handleWin(WebServerType& server) {
    if (!CameraManager::isEnabled()) {
        sendBinaryError(server, 503, BIN_STATUS_ERROR, "Camera is disabled");
        return;
    }
    int startX  = getQueryInt(server, "sx",   0);
    int startY  = getQueryInt(server, "sy",   0);
    int endX    = getQueryInt(server, "ex",   0);
    int endY    = getQueryInt(server, "ey",   0);
    int offsetX = getQueryInt(server, "offx", 0);
    int offsetY = getQueryInt(server, "offy", 0);
    int totalX  = getQueryInt(server, "tx",   0);
    int totalY  = getQueryInt(server, "ty",   0);
    int outputX = getQueryInt(server, "ox",   0);
    int outputY = getQueryInt(server, "oy",   0);
    bool scale   = getQueryInt(server, "scale",   0) == 1;
    bool binning = getQueryInt(server, "binning", 0) == 1;
    sensor_t *s = esp_camera_sensor_get();
    int res = s->set_res_raw(
        s, startX, startY, endX, endY, offsetX, offsetY,
        totalX, totalY, outputX, outputY, scale, binning);
    if (res) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
}

// -----------------------------------------------------------------------
// MJPEG stream handler (ESP-IDF httpd — separate server on port 81)
// -----------------------------------------------------------------------
static esp_err_t stream_handler(httpd_req_t *req) {
    if (!CameraManager::isEnabled()) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Camera is disabled\"}");
        return ESP_FAIL;
    }

    camera_fb_t     *fb = NULL;
    struct timeval   _timestamp;
    esp_err_t        res = ESP_OK;
    size_t           _jpg_buf_len = 0;
    uint8_t         *_jpg_buf = NULL;
    char             part_buf[128];
    static int64_t   last_frame = 0;

    if (!last_frame) last_frame = esp_timer_get_time();

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

#if defined(LED_GPIO_NUM)
    isStreaming = true;
    enable_led(true);
#endif

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        } else {
            _timestamp.tv_sec  = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if (!jpeg_converted) {
                    log_e("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf     = fb->buf;
            }
        }
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(
                part_buf, sizeof(part_buf), _STREAM_PART,
                _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK) {
            log_e("Send frame failed");
            break;
        }

        int64_t fr_end   = esp_timer_get_time();
        int64_t frame_time = (fr_end - last_frame) / 1000;
        last_frame = fr_end;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, (int)frame_time);
        log_i(
            "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)",
            (uint32_t)_jpg_buf_len, (uint32_t)frame_time, 1000.0 / frame_time,
            avg_frame_time, 1000.0 / avg_frame_time);
#endif
    }

#if defined(LED_GPIO_NUM)
    isStreaming = false;
    enable_led(false);
#endif
    return res;
}

// -----------------------------------------------------------------------
// CameraHandler public interface
// -----------------------------------------------------------------------

void CameraHandler::setupRoutes(WebServerType& server) {
    ra_filter_init(&ra_filter, 20);

    server.on("/status",     HTTP_GET, [&server]() { handleStatus(server); });
    server.on("/control",    HTTP_POST, [&server]() { handleCmd(server); }, rawBodyStub);
    server.on("/capture",    HTTP_GET, [&server]() { handleCapture(server); });
    server.on("/bmp",        HTTP_GET, [&server]() { handleBmp(server); });
    server.on("/xclk",       HTTP_GET, [&server]() { handleXclk(server); });
    server.on("/reg",        HTTP_GET, [&server]() { handleReg(server); });
    server.on("/greg",       HTTP_GET, [&server]() { handleGreg(server); });
    server.on("/pll",        HTTP_GET, [&server]() { handlePll(server); });
    server.on("/resolution", HTTP_GET, [&server]() { handleWin(server); });

    Utils::printSerial(F("Camera control routes registered on main HTTP server."));
}

void CameraHandler::startStreamServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port    = Config::CAMERA_STREAM_PORT;
    config.ctrl_port      = 32768 + Config::CAMERA_STREAM_PORT;

    httpd_uri_t stream_uri = {};
    stream_uri.uri      = "/stream";
    stream_uri.method   = HTTP_GET;
    stream_uri.handler  = stream_handler;
    stream_uri.user_ctx = NULL;

    Utils::printSerial(F("## Starting camera stream server on port "), "");
    char portbuf[6];
    snprintf(portbuf, sizeof(portbuf), "%u", Config::CAMERA_STREAM_PORT);
    Utils::printSerial(portbuf);

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

void CameraHandler::setupLedFlash() {
#if defined(LED_GPIO_NUM)
    ledcAttach(LED_GPIO_NUM, 5000, 8);
    Utils::printSerial(F("Camera LED flash configured."));
#else
    Utils::printSerial(F("Camera LED flash not configured (LED_GPIO_NUM undefined)."));
#endif
}

#endif  // ESP_CAM_HW_EXIST
