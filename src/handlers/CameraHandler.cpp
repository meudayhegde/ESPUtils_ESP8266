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

// CameraHandler.h includes board_config.h which sets ESP_CAM_ENABLED when a
// camera-equipped board is selected.  No camera → entire file is compiled out.
#if defined(ESP_CAM_ENABLED)

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "../hardware/camera/camera_index.h"
#include "../hardware/camera/board_config.h"

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
static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask) {
    return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

// -----------------------------------------------------------------------
// Camera control handlers (Arduino WebServer, port 80)
// -----------------------------------------------------------------------

static void handleIndex(WebServerType& server) {
    server.sendHeader("Content-Encoding", "gzip");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        const uint8_t *data;
        size_t len;
        if (s->id.PID == OV3660_PID) {
            data = index_ov3660_html_gz;
            len  = index_ov3660_html_gz_len;
        } else if (s->id.PID == OV5640_PID) {
            data = index_ov5640_html_gz;
            len  = index_ov5640_html_gz_len;
        } else {
            data = index_ov2640_html_gz;
            len  = index_ov2640_html_gz_len;
        }
        server.setContentLength(len);
        server.send(200, "text/html", "");
        server.sendContent((const char *)data, len);
    } else {
        log_e("Camera sensor not found");
        server.send(500, "text/plain", "Camera sensor not found");
    }
}

static void handleStatus(WebServerType& server) {
    static char json_response[1024];
    sensor_t   *s = esp_camera_sensor_get();
    char       *p = json_response;
    *p++ = '{';

    if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
        for (int reg = 0x3400; reg < 0x3406; reg += 2) p += print_reg(p, s, reg, 0xFFF);
        p += print_reg(p, s, 0x3406, 0xFF);
        p += print_reg(p, s, 0x3500, 0xFFFF0);
        p += print_reg(p, s, 0x3503, 0xFF);
        p += print_reg(p, s, 0x350a, 0x3FF);
        p += print_reg(p, s, 0x350c, 0xFFFF);
        for (int reg = 0x5480; reg <= 0x5490; reg++) p += print_reg(p, s, reg, 0xFF);
        for (int reg = 0x5380; reg <= 0x538b; reg++) p += print_reg(p, s, reg, 0xFF);
        for (int reg = 0x5580; reg < 0x558a;  reg++) p += print_reg(p, s, reg, 0xFF);
        p += print_reg(p, s, 0x558a, 0x1FF);
    } else if (s->id.PID == OV2640_PID) {
        p += print_reg(p, s, 0xd3,  0xFF);
        p += print_reg(p, s, 0x111, 0xFF);
        p += print_reg(p, s, 0x132, 0xFF);
    }

    p += sprintf(p, "\"xclk\":%u,",           s->xclk_freq_hz / 1000000);
    p += sprintf(p, "\"pixformat\":%u,",       s->pixformat);
    p += sprintf(p, "\"framesize\":%u,",       s->status.framesize);
    p += sprintf(p, "\"quality\":%u,",         s->status.quality);
    p += sprintf(p, "\"brightness\":%d,",      s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,",        s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,",      s->status.saturation);
    p += sprintf(p, "\"sharpness\":%d,",       s->status.sharpness);
    p += sprintf(p, "\"special_effect\":%u,",  s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,",         s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,",             s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,",        s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,",             s->status.aec);
    p += sprintf(p, "\"aec2\":%u,",            s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,",        s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,",       s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,",             s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,",        s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,",     s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,",             s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,",             s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,",         s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,",            s->status.lenc);
    p += sprintf(p, "\"hmirror\":%u,",         s->status.hmirror);
    p += sprintf(p, "\"vflip\":%u,",           s->status.vflip);
    p += sprintf(p, "\"dcw\":%u,",             s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u",         s->status.colorbar);
#if defined(LED_GPIO_NUM)
    p += sprintf(p, ",\"led_intensity\":%u", led_duty);
#else
    p += sprintf(p, ",\"led_intensity\":%d", -1);
#endif
    *p++ = '}';
    *p++ = 0;

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json_response);
}

static void handleCmd(WebServerType& server) {
    if (!server.hasArg("var") || !server.hasArg("val")) {
        server.send(404, "text/plain", "Variable not found");
        return;
    }
    String variable = server.arg("var");
    int    val      = server.arg("val").toInt();
    sensor_t *s     = esp_camera_sensor_get();
    int       res   = 0;

    if      (variable == "framesize")      { if (s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val); }
    else if (variable == "quality")         res = s->set_quality(s, val);
    else if (variable == "contrast")        res = s->set_contrast(s, val);
    else if (variable == "brightness")      res = s->set_brightness(s, val);
    else if (variable == "saturation")      res = s->set_saturation(s, val);
    else if (variable == "gainceiling")     res = s->set_gainceiling(s, (gainceiling_t)val);
    else if (variable == "colorbar")        res = s->set_colorbar(s, val);
    else if (variable == "awb")             res = s->set_whitebal(s, val);
    else if (variable == "agc")             res = s->set_gain_ctrl(s, val);
    else if (variable == "aec")             res = s->set_exposure_ctrl(s, val);
    else if (variable == "hmirror")         res = s->set_hmirror(s, val);
    else if (variable == "vflip")           res = s->set_vflip(s, val);
    else if (variable == "awb_gain")        res = s->set_awb_gain(s, val);
    else if (variable == "agc_gain")        res = s->set_agc_gain(s, val);
    else if (variable == "aec_value")       res = s->set_aec_value(s, val);
    else if (variable == "aec2")            res = s->set_aec2(s, val);
    else if (variable == "dcw")             res = s->set_dcw(s, val);
    else if (variable == "bpc")             res = s->set_bpc(s, val);
    else if (variable == "wpc")             res = s->set_wpc(s, val);
    else if (variable == "raw_gma")         res = s->set_raw_gma(s, val);
    else if (variable == "lenc")            res = s->set_lenc(s, val);
    else if (variable == "special_effect")  res = s->set_special_effect(s, val);
    else if (variable == "wb_mode")         res = s->set_wb_mode(s, val);
    else if (variable == "ae_level")        res = s->set_ae_level(s, val);
#if defined(LED_GPIO_NUM)
    else if (variable == "led_intensity") {
        led_duty = val;
        if (isStreaming) enable_led(true);
    }
#endif
    else { log_i("Unknown command: %s", variable.c_str()); res = -1; }

    if (res < 0) { server.send(500, "text/plain", ""); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "");
}

static void handleCapture(WebServerType& server) {
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
    camera_fb_t     *fb = NULL;
    struct timeval   _timestamp;
    esp_err_t        res = ESP_OK;
    size_t           _jpg_buf_len = 0;
    uint8_t         *_jpg_buf = NULL;
    char            *part_buf[128];
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
                (char *)part_buf, 128, _STREAM_PART,
                _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
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

    server.on("/",           HTTP_GET, [&server]() { handleIndex(server); });
    server.on("/status",     HTTP_GET, [&server]() { handleStatus(server); });
    server.on("/control",    HTTP_GET, [&server]() { handleCmd(server); });
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

#endif  // ESP_CAM_ENABLED
