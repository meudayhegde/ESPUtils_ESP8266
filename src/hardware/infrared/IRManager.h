#ifndef IR_MANAGER_H
#define IR_MANAGER_H

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
#include "../../config/Config.h"
#include "../../utils/Utils.h"
#include "../../platform/Platform.h"
#include "../../protocol/BinaryProtocol.h"

class IRManager {
private:
    static IRrecv* irRecv;
    static IRsend* irSend;
    static decode_results results;
    
public:
    /**
     * @brief Initialize IR receiver and sender
     */
    static void begin();
    
    /**
     * @brief Capture IR signal and stream results via SSE.
     *        captureMode: 0 = single capture, 1 = multi-capture (stays open until timeout)
     *        Sends HTTP 200 text/event-stream directly; caller must NOT send any additional
     *        response after calling this function.
     *        SSE event data fields are base64-encoded binary structs.
     * @param captureMode 0=single, 1=multi
     * @param server WebServer instance (response sent inside)
     */
    static void captureIR(int captureMode, WebServerType& server);
    
    /**
     * @brief Send IR signal (binary interface)
     * @param protocol Protocol name string
     * @param bitLength Bit length / raw size
     * @param irCode   IR code data (hex string or array string)
     * @param irCodeLen Length of irCode string
     * @param resp     Pointer to BinIrSendResponse to fill
     */
    static void sendIR(const char* protocol, uint16_t bitLength,
                       const char* irCode, uint16_t irCodeLen,
                       BinIrSendResponse* resp);
    
    /**
     * @brief Generate binary IR capture event from decode_results.
     *        Writes header + irCode data into caller-provided buffer.
     * @param results Pointer to decode_results
     * @param buf     Output buffer (must be large enough)
     * @param bufSize Size of output buffer
     * @return Total bytes written (header + irCode data), or 0 on error
     */
    static size_t generateIRResult(const decode_results* results,
                                   uint8_t* buf, size_t bufSize);
    
private:
    /**
     * @brief Send raw IR array
     * @param size Array size
     * @param irData JSON array string
     */
    static void sendRawArray(uint16_t size, const char* irData);
    
    /**
     * @brief Send IR value (non-AC protocols)
     * @param size Data size
     * @param protocol Protocol type
     * @param irData Hex string
     * @return true if successful, false otherwise
     */
    static bool sendIRValue(uint16_t size, decode_type_t protocol, const char* irData);
    
    /**
     * @brief Send IR state (AC protocols)
     * @param size State array size
     * @param protocol Protocol type
     * @param data State array as JSON string
     * @return true if successful, false otherwise
     */
    static bool sendIRState(uint16_t size, decode_type_t protocol, const char* data);
};

#endif // IR_MANAGER_H
