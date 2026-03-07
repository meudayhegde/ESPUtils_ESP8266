#include "IRManager.h"
#include <ArduinoJson.h>  // only used in sendRawArray/sendIRState for JSON array parsing

IRrecv* IRManager::irRecv = nullptr;
IRsend* IRManager::irSend = nullptr;
decode_results IRManager::results;

void IRManager::begin() {
    Utils::printSerial(F("## Begin IR Receiver lib."));
    
    irRecv = new IRrecv(Config::IR_RECV_PIN, Config::CAPTURE_BUFFER_SIZE, Config::IR_TIMEOUT_MS, true);
    
    #if DECODE_HASH
        irRecv->setUnknownThreshold(Config::MIN_UNKNOWN_SIZE);
    #endif
    
    Utils::printSerial(F("## Begin IR Sender lib."));
    irSend = new IRsend(Config::IR_SEND_PIN);
    irSend->begin();
}

size_t IRManager::generateIRResult(const decode_results* results,
                                    uint8_t* buf, size_t bufSize) {
    if (!results || bufSize < sizeof(BinIrCaptureEventHeader)) {
        return 0;
    }
    
    decode_type_t protocol = results->decode_type;
    uint16_t size = results->bits;
    
    irRecv->disableIRIn();
    
    BinIrCaptureEventHeader* header = reinterpret_cast<BinIrCaptureEventHeader*>(buf);
    header->eventType = BIN_IR_EVENT_CAPTURE;
    memset(header->protocol, 0, sizeof(header->protocol));
    strncpy(header->protocol, typeToString(protocol).c_str(), sizeof(header->protocol) - 1);
    
    // Build irCode string into the buffer space after the header
    char* irCodeStart = reinterpret_cast<char*>(buf + sizeof(BinIrCaptureEventHeader));
    size_t remainingBuf = bufSize - sizeof(BinIrCaptureEventHeader);
    size_t irCodeLen = 0;
    
    // Handle UNKNOWN protocol (raw data)
    if (protocol == decode_type_t::UNKNOWN) {
        header->bitLength = getCorrectedRawLength(results);
        
        // Build raw array string: [val1,val2,...]
        size_t pos = 0;
        if (pos < remainingBuf) irCodeStart[pos++] = '[';
        
        for (uint16_t i = 1; i < results->rawlen && pos < remainingBuf - 10; i++) {
            uint32_t usecs;
            for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
                int w = snprintf(irCodeStart + pos, remainingBuf - pos, "%u,0,", (unsigned)UINT16_MAX);
                if (w > 0) pos += w;
            }
            int w = snprintf(irCodeStart + pos, remainingBuf - pos, "%u", (unsigned)usecs);
            if (w > 0) pos += w;
            if (i < results->rawlen - 1 && pos < remainingBuf - 1) {
                irCodeStart[pos++] = ',';
            }
        }
        if (pos < remainingBuf) irCodeStart[pos++] = ']';
        irCodeLen = pos;
    }
    // Handle AC protocols (state array)
    else if (hasACState(protocol)) {
        uint16_t nbytes = results->bits / 8;
        header->bitLength = nbytes;
        
        size_t pos = 0;
        if (pos < remainingBuf) irCodeStart[pos++] = '[';
        
        for (uint16_t i = 0; i < nbytes && pos < remainingBuf - 8; i++) {
            int w = snprintf(irCodeStart + pos, remainingBuf - pos, "'0x%02X'", results->state[i]);
            if (w > 0) pos += w;
            if (i < nbytes - 1 && pos < remainingBuf - 1) {
                irCodeStart[pos++] = ',';
            }
        }
        if (pos < remainingBuf) irCodeStart[pos++] = ']';
        irCodeLen = pos;
    }
    // Handle standard protocols (single value)
    else {
        header->bitLength = size;
        irCodeLen = snprintf(irCodeStart, remainingBuf, "%s",
                             uint64ToString(results->value, 16).c_str());
    }
    
    header->irCodeLen = (uint16_t)irCodeLen;
    return sizeof(BinIrCaptureEventHeader) + irCodeLen;
}

void IRManager::captureIR(int captureMode, WebServerType& server) {
    Utils::printSerial(F("\nBeginning IR capture procedure"));

    // Start SSE response — chunked transfer, never a fixed Content-Length
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Connection", "keep-alive");
    server.send(200, "text/event-stream", "");

    irRecv->enableIRIn();

    // Buffers for base64-encoded binary events
    // Max binary event: sizeof(BinIrCaptureEventHeader) + ~6000 bytes irCode
    // We'll use a reasonable buffer; large raw captures get truncated
    static uint8_t binBuf[4096];
    static char b64Buf[5500]; // ((4096+2)/3)*4 + 1
    char eventBuf[64];
    
    uint32_t startTime = millis();
    int currentTime = 0;
    int previousTime = -1;
    int blinkCounter = 0;
    bool multiCapture = (captureMode == 1);

    // Send initial countdown value (binary progress event, base64-encoded)
    {
        BinIrProgressEvent prog;
        prog.eventType = BIN_IR_EVENT_PROGRESS;
        prog.value = Config::RECV_TIMEOUT_SEC;
        size_t b64Len = Base64::encode(reinterpret_cast<uint8_t*>(&prog), sizeof(prog), b64Buf);
        snprintf(eventBuf, sizeof(eventBuf), "data: ");
        server.sendContent(eventBuf);
        server.sendContent(b64Buf, b64Len);
        server.sendContent("\n\n");
    }

    WiFiClient client = server.client();
    while ((currentTime < Config::RECV_TIMEOUT_SEC) && client.connected()) {
        currentTime = (int)((millis() - startTime) / 1000);

        // Send countdown once per second
        if (currentTime > previousTime) {
            Utils::setLED(HIGH);
            BinIrProgressEvent prog;
            prog.eventType = BIN_IR_EVENT_PROGRESS;
            prog.value = (uint8_t)(Config::RECV_TIMEOUT_SEC - currentTime);
            size_t b64Len = Base64::encode(reinterpret_cast<uint8_t*>(&prog), sizeof(prog), b64Buf);
            snprintf(eventBuf, sizeof(eventBuf), "data: ");
            server.sendContent(eventBuf);
            server.sendContent(b64Buf, b64Len);
            server.sendContent("\n\n");
            previousTime = currentTime;
        }

        // Check for IR signal
        if (irRecv->decode(&results)) {
            irRecv->disableIRIn();
            size_t totalLen = generateIRResult(&results, binBuf, sizeof(binBuf));

            if (totalLen > 0) {
                // Emit the captured signal as a base64-encoded SSE event
                size_t b64Len = Base64::encode(binBuf, totalLen, b64Buf);
                server.sendContent("data: ");
                server.sendContent(b64Buf, b64Len);
                server.sendContent("\n\n");
            }

            if (multiCapture) {
                // Re-arm receiver for next capture; reset timer
                irRecv->enableIRIn();
                irRecv->resume();
                startTime = millis();
                currentTime = 0;
                previousTime = -1;
            } else {
                Utils::setLED(HIGH);
                // Close the SSE stream
                server.sendContent("");
                return;
            }
        }

        // LED blinking for visual feedback
        if (blinkCounter % 100 == 0)       Utils::setLED(LOW);
        else if ((blinkCounter - 8) % 100 == 0) Utils::setLED(HIGH);

        delay(1);
        yield();
        blinkCounter++;
    }

    Utils::setLED(HIGH);

    // Timeout — send final event then close stream
    {
        BinIrTimeoutEvent timeout;
        timeout.eventType = BIN_IR_EVENT_TIMEOUT;
        size_t b64Len = Base64::encode(reinterpret_cast<uint8_t*>(&timeout), sizeof(timeout), b64Buf);
        server.sendContent("data: ");
        server.sendContent(b64Buf, b64Len);
        server.sendContent("\n\n");
    }
    server.sendContent("");
}

void IRManager::sendIR(const char* protocolStr, uint16_t bitLength,
                       const char* irCode, uint16_t irCodeLen,
                       BinIrSendResponse* resp) {
    memset(resp, 0, sizeof(BinIrSendResponse));
    decode_type_t protocol = strToDecodeType(protocolStr);

    if (protocol == decode_type_t::UNKNOWN) {
        sendRawArray(bitLength, irCode);
        resp->status = BIN_STATUS_OK;
        strncpy(resp->response, "success", sizeof(resp->response) - 1);
    } else {
        bool success = hasACState(protocol)
            ? sendIRState(bitLength, protocol, irCode)
            : sendIRValue(bitLength, protocol, irCode);
        resp->status = success ? BIN_STATUS_OK : BIN_STATUS_ERROR;
        snprintf(resp->response, sizeof(resp->response), "%s %s",
                 typeToString(protocol).c_str(),
                 success ? "success" : "failure");
    }
}

void IRManager::sendRawArray(uint16_t size, const char* irData) {
    JsonDocument json;
    if (deserializeJson(json, irData) != DeserializationError::Ok) {
        Utils::printSerial(F("sendRawArray: JSON parse error"));
        return;
    }

    // Clamp to compile-time maximum to prevent stack overflow
    const uint16_t safeSize = (size > Config::IR_RAW_SEND_MAX)
                              ? Config::IR_RAW_SEND_MAX : size;

    uint16_t* rawData = new(std::nothrow) uint16_t[safeSize];
    if (!rawData) {
        Utils::printSerial(F("sendRawArray: alloc failed"));
        return;
    }

    for (uint16_t i = 0; i < safeSize; i++) {
        rawData[i] = json[i].as<uint16_t>();
    }

    irSend->sendRaw(rawData, safeSize, Config::IR_FREQUENCY);
    delete[] rawData;
}

bool IRManager::sendIRValue(uint16_t size, decode_type_t protocol, const char* irData) {
    uint64_t value = Utils::getUInt64FromHex(irData);
    return irSend->send(protocol, value, size);
}

bool IRManager::sendIRState(uint16_t size, decode_type_t protocol, const char* data) {
    String stateListString = String(data);
    stateListString.replace("'", "\"");
    
    JsonDocument doc;
    deserializeJson(doc, stateListString.c_str());
    
    uint8_t stateList[size];
    for (uint16_t i = 0; i < size; i++) {
        const char* hexStr = doc[i];
        stateList[i] = strtol(hexStr, NULL, 16);
    }
    
    return irSend->send(protocol, stateList, size);
}
