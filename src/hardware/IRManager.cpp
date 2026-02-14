#include "IRManager.h"

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

String IRManager::generateIRResult(const decode_results* results) {
    if (!results) {
        return "{\"response\":\"error\",\"message\":\"Invalid results\"}";
    }
    
    decode_type_t protocol = results->decode_type;
    uint16_t size = results->bits;
    
    irRecv->disableIRIn();
    
    String output = String("{\"response\":\"success\",\"protocol\":\"");
    output += typeToString(protocol);
    output += "\",\"length\":\"";
    
    // Handle UNKNOWN protocol (raw data)
    if (protocol == decode_type_t::UNKNOWN) {
        output += uint64ToString(getCorrectedRawLength(results), 16);
        output += "\",\"irCode\":\"[";
        
        for (uint16_t i = 1; i < results->rawlen; i++) {
            uint32_t usecs;
            for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
                output += uint64ToString(UINT16_MAX);
                output += (i % 2) ? ",0," : ",0,";
            }
            output += uint64ToString(usecs, 10);
            if (i < results->rawlen - 1) {
                output += kCommaSpaceStr;
            }
            if (i % 2 == 0) {
                output += ' ';
            }
        }
        output += "]\"}";
    }
    // Handle AC protocols (state array)
    else if (hasACState(protocol)) {
        uint16_t nbytes = results->bits / 8;
        output += uint64ToString(nbytes, 16);
        output += "\",\"irCode\":\"[";
        
        for (uint16_t i = 0; i < nbytes; i++) {
            output += "'0x";
            if (results->state[i] < 0x10) {
                output += '0';
            }
            output += uint64ToString(results->state[i], 16);
            output += "'";
            if (i < nbytes - 1) {
                output += kCommaSpaceStr;
            }
        }
        output += "]\"}";
    }
    // Handle standard protocols (single value)
    else {
        output += uint64ToString(size, 16);
        output += "\",\"irCode\":\"";
        output += uint64ToString(results->value, 16);
        output += "\"}";
    }
    
    output.replace(" ", "");
    return output;
}

String IRManager::captureIR(bool multiCapture, WiFiClient& client) {
    Utils::printSerial(F("Beginning IR capture procedure"));
    
    irRecv->enableIRIn();
    
    char timeoutResponse[32];
    snprintf(timeoutResponse, sizeof(timeoutResponse), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::TIMEOUT));
    String result = String(timeoutResponse);
    
    uint32_t startTime = millis();
    int currentTime = 0;
    int previousTime = -1;
    
    // Send initial progress
    char progressMsg[64];
    snprintf(progressMsg, sizeof(progressMsg), "{\"response\":\"%s\",\"value\":%d}", 
             FPSTR_TO_CSTR(ResponseMsg::PROGRESS), Config::RECV_TIMEOUT_SEC);
    client.println(progressMsg);
    client.flush();
    
    int blinkCounter = 0;
    
    while ((currentTime < Config::RECV_TIMEOUT_SEC) && client.connected()) {
        currentTime = (millis() - startTime) / 1000;
        
        // Send progress update
        if (currentTime > previousTime) {
            Utils::setLED(HIGH);
            snprintf(progressMsg, sizeof(progressMsg), "{\"response\":\"%s\",\"value\":%d}", 
                     FPSTR_TO_CSTR(ResponseMsg::PROGRESS), (Config::RECV_TIMEOUT_SEC - currentTime));
            client.println(progressMsg);
            client.flush();
            previousTime = currentTime;
        }
        
        // Check for IR signal
        if (irRecv->decode(&results)) {
            irRecv->disableIRIn();
            
            if (multiCapture) {
                Utils::setLED(HIGH);
                client.println(generateIRResult(&results));
                client.flush();
                
                irRecv->enableIRIn();
                snprintf(progressMsg, sizeof(progressMsg), "{\"response\":\"%s\"}", FPSTR_TO_CSTR(ResponseMsg::SUCCESS));
                result = String(progressMsg);
                
                // Reset timer for next capture
                startTime = millis();
                currentTime = 0;
                previousTime = -1;
                
                irRecv->resume();
            } else {
                result = generateIRResult(&results);
                break;
            }
        }
        
        // LED blinking for visual feedback
        if (blinkCounter % 100 == 0) {
            Utils::setLED(LOW);
        } else if ((blinkCounter - 8) % 100 == 0) {
            Utils::setLED(HIGH);
        }
        
        delay(1);
        yield();
        blinkCounter++;
    }
    
    Utils::setLED(HIGH);
    return result;
}

String IRManager::sendIR(uint16_t size, const char* protocolStr, const char* irData) {
    decode_type_t protocol = strToDecodeType(protocolStr);
    
    if (protocol == decode_type_t::UNKNOWN) {
        sendRawArray(size, irData);
        return F("{\"response\":\"success\"}");
    } else {
        bool success = hasACState(protocol) 
            ? sendIRState(size, protocol, irData) 
            : sendIRValue(size, protocol, irData);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"response\":\"%s %s\"}", 
                 typeToString(protocol).c_str(),
                 success ? "success" : "failure");
        return String(response);
    }
}

void IRManager::sendRawArray(uint16_t size, const char* irData) {
    JsonDocument json;
    deserializeJson(json, irData);
    
    uint16_t rawData[size];
    for (uint16_t i = 0; i < size; i++) {
        rawData[i] = json[i];
    }
    
    irSend->sendRaw(rawData, size, Config::IR_FREQUENCY);
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
