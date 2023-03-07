#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRtext.h>
#include <IRutils.h>
#include <LittleFS.h>

#define LEGACY_TIMING_INFO false
#define LED 02
#define SERIAL_MONITOR_ENABLED true

const char* WiFiConfigFile = "/WiFiConfig.json";
const char* LoginCredential = "/LoginCredential.json";
const char* GPIOConfigFile = "/GPIOConfig.json";

const char* defaultMode = "AP"; // AP / WIFI
const char* deviceName = "ESPUtils";
const char* devicePassword = "ESP.device@8266";

struct UserConfig{
    String user;
    String pass;
};

struct WirelessConfig{
    String wireless_mode;
    String station_ssid;
    String station_psk;
    String ap_ssid;
    String ap_psk;
};

WirelessConfig wirelessConfig;
UserConfig userConfig;

bool wireless_updated = false;

const uint16_t socketPort = 48321;
const uint16_t otaPort = 48325;
const uint16_t udpPortESP = 48327;
const uint16_t udpPortApp = 48326;

const uint8_t recv_timeout = 8;    // seconds

const uint8_t wirelessTimeout = 20;   // seconds

const uint16_t maxRequestLength = 5120;

const uint8_t kRecvPin = 14;
const uint8_t kIrLed = 4;

const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;

const uint8_t kTimeout = 50;  // Milli-Seconds

const uint16_t kFrequency = 38000;

const uint8_t kMinUnknownSize = 12;

const uint16_t udpPacketSize = 255;

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
IRsend irsend(kIrLed);

decode_results results;

WiFiServer socketServer(socketPort);
WiFiUDP UDP;

// Function prototypes
String applyGPIO(const int GPIOPinNumber, const char* GPIOPinMode, const int GPIOPinValue);
bool authenticate(const char* username, const char* password);
void checkResetState(const int pinNumber);
bool factoryReset();
String generateIrResult(const decode_results * const results);
String getGPIO(const int pinNumber);
uint64_t getUInt64fromHex(char const *hex);
String getWireless();
void handleDatagram();
void handleSocket();
void initCredentials();
void initWireless();
String irCapture(bool multiCapture, WiFiClient client);
String irSend(uint16_t size, const char* protocol_str, const char* irData);
void ledPulse(int _on, int _off, int _count);
void loop();
size_t printSerial(const char* serial, const char* end = "\n");
size_t printSerial(const String serial, const char* end = "\n");
String requestHandler(String request, WiFiClient client);
bool sendIrState(uint16_t size, decode_type_t protocol, const char* data);
bool sendIrValue(uint16_t size, decode_type_t protocol, const char* irData);
void sendRawArray(uint16_t size, const char* irData);
void setup();
String setCredentials(const char* user_name,const char* passwd);
String setWireless(const char* wireless_mode, const char* wireless_name, const char* wireless_passwd);

void setup() {
   /*
    * Startup instructions.
    */
    #if defined(ESP8266)
        Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
    #else
        Serial.begin(kBaudRate, SERIAL_8N1);
    #endif
    
    printSerial("## Set Board Indicator.");
    pinMode(LED, OUTPUT);

    printSerial("## Begin flash storage.");
    LittleFS.begin();

    printSerial("## Apply GPIO settings.");
    applyGPIO(-1, "", 0);

    printSerial("## Begin wireless network.");
    initWireless();

    printSerial("## Begin TCP socket server.");
    socketServer.begin();

    #if DECODE_HASH
        irrecv.setUnknownThreshold(kMinUnknownSize);
    #endif
    printSerial("## Begin IR Sender lib.");
    irsend.begin();

    printSerial("## Load user credentials.");
    initCredentials();

    printSerial("## Begin ArduinoOTA service.");
    ArduinoOTA.onStart([]() {
        printSerial("## Begin OTA Update process.");
    });
    ArduinoOTA.onEnd([]() {
        printSerial("\n## OTA Update process End.");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if(SERIAL_MONITOR_ENABLED) Serial.printf("==> Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        if(SERIAL_MONITOR_ENABLED) Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) printSerial("OTA Auth Failed");
        else if (error == OTA_BEGIN_ERROR) printSerial("OTA Begin Failed");
        else if (error == OTA_CONNECT_ERROR) printSerial("OTA Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) printSerial("OTA Receive Failed");
        else if (error == OTA_END_ERROR) printSerial("OTA End Failed");
    });
    ArduinoOTA.setPort(otaPort);
    ArduinoOTA.begin();

    printSerial(String("## Begin UDP on port: ") + udpPortESP);
    UDP.begin(udpPortESP);
}

String applyGPIO(const int GPIOPinNumber, const char* GPIOPinMode, const int GPIOPinValue){
   /*
    * Apply GPIOPinMode and GPIOPinValue for gpio GPIOPinNumber.
    * add/update GPIO settings for GPIOPinNumber and store in GPIOConfigFile.
    * if GPIOPinNumber == -1, apply the stored settings for all the GPIO pins.
    * if GPIOPinValue == -1, toggle pinValue based on stored value.
    */
  
    printSerial(String("Reading File: ") + GPIOConfigFile, "...  ");
    // open config file 
    File file = LittleFS.open(GPIOConfigFile, "r");    
    printSerial((!file)? " Failed!, falling back to default configs." : " Done.");

    // read config file.
    // structure: [{"pinNumber": 2, "pinMode": "OUTPUT", "pinValue": 0},...]
    String gpioConfig = file.readStringUntil(']') + "]";
    const size_t capacity = gpioConfig.length() + 512;
    DynamicJsonDocument doc(capacity);

    printSerial("Parsing File...  ", "");    
    // parse config file to doc as JSONObject.
    DeserializationError error = deserializeJson(doc, gpioConfig);
    printSerial((error)? "Failed!, falling back to default configs." : "Done.");

    JsonArray array = doc.as<JsonArray>();
    bool pinConfExist = false;

    int retPinVal = -1;

    // traverse through each JSONObject, each JSONObject is config of a certain GPIO Pin
    for(JsonVariant gpio : array) {
        const int pin = gpio["pinNumber"];
        const int pinVal = gpio["pinValue"];

        // update settings for GPIOPinNumber.
        if(GPIOPinNumber != -1 && pin == GPIOPinNumber){
            gpio["pinNumber"] = GPIOPinNumber;
            gpio["pinMode"] = GPIOPinMode;
            gpio["pinValue"] = (GPIOPinValue == -1)? ((pinVal == LOW)? HIGH: LOW): GPIOPinValue;
        }
        
        // if GPIOPinNumber == -1, apply the stored settings for all the GPIO pins.
        // else apply settings for GPIOPinNumber only.
        if(GPIOPinNumber == -1 || pin == GPIOPinNumber){
            pinConfExist = true;
            const char* PinMode = gpio["pinMode"];
            const int PinValue = gpio["pinValue"];
            if(strcmp(PinMode, "OUTPUT") == 0){
                pinMode(pin, OUTPUT);
                digitalWrite(pin, PinValue);
            }
            retPinVal = PinValue;
        }
    }

    //If settings for GPIOPinNumber does not exist in storage, create new JSONObject for GPIOPinNumber.
    if(!pinConfExist){
        JsonObject docNewPin = doc.createNestedObject();
        docNewPin["pinNumber"] = GPIOPinNumber;
        docNewPin["pinMode"] = GPIOPinMode;
        docNewPin["pinValue"] = GPIOPinValue;
        if(strcmp(GPIOPinMode, "OUTPUT") == 0){
            pinMode(GPIOPinNumber, OUTPUT);
            digitalWrite(GPIOPinNumber, GPIOPinValue);
        }
    }
    
    if(file) file.close();
    
    printSerial(String("Writing File: ") + GPIOConfigFile, "...  ");
    file = LittleFS.open(GPIOConfigFile, "w");  
    printSerial((!file)? " Failed!, falling back to default configs." : " Done.");

    // Save the updated content as file.
    serializeJson(doc, file);
    if(file) file.close();
    String ret = String("{\"response\": \"success\", \"pinValue\":");
    ret += retPinVal;
    ret += "}";
    return ret;
}

String getGPIO(const int pinNumber){
   /*
    * return JSONObject corresponding to pinNumber.
    * if pinNumber == -1, return JSONArray of all JSONObjects
    */
    printSerial(String("Reading File: ") + GPIOConfigFile, "...  ");
    File file = LittleFS.open(GPIOConfigFile, "r"); 
    printSerial((!file)? " Failed!, falling back to default configs." : " Done.");
    
    // read config file
    String gpioConfig = file.readStringUntil(']') + "]";
    // ifpinNumber == -1, return JSONArray of all JSONObjects
    if(pinNumber == -1){
        return gpioConfig;
    }
    const size_t capacity = gpioConfig.length() + 512;
    DynamicJsonDocument doc(capacity);
    
    printSerial("Parsing File...  ", " ");
    // parse config file to doc as JSONObject
    DeserializationError error = deserializeJson(doc, gpioConfig);
    printSerial((error)? "Failed!, falling back to default configs." : "Done.");
    
    // traverse through each JSONObject, each JSONObject is config of a certain GPIO Pin
    JsonArray array = doc.as<JsonArray>();
    for(JsonVariant gpio : array) {
        const int pin = gpio["pinNumber"];
        //if gpio is corresponding to pinNumber, return gpio as string
        if(pin == pinNumber){
            String output;
            serializeJson(gpio, output);
            if(file) file.close();
            return output;
        }
    }
    if(file) file.close();
    return String("{}");
}

void initWireless(){
   /* 
    * initialize wifi/softAP based on the settings saved in WiFiConfigFile.
    * WiFiConfigFIle structure: {"mode":"WIFI","wifi_name":"myWiFiName","wifi_pass":"myWiFiPassword","ap_name":"myApName","ap_pass":"myAPPassword"}
    * mode : WIFI / AP
    */
    printSerial(String("Reading File: ") + WiFiConfigFile, "...  ");
    File file = LittleFS.open(WiFiConfigFile, "r");
    printSerial((!file)? " Failed!, falling back to default configs." : " Done.");

    const size_t capacity = JSON_OBJECT_SIZE(5) + 240;
    DynamicJsonDocument doc(capacity);

    printSerial("Parsing File...  ", " ");
    // parse WiFiConfigFile to doc as JSONObject
    DeserializationError error = deserializeJson(doc, file);
    printSerial((error)? "Failed!, falling back to default configs." : "Done.");

    const char* wifiMode = doc["mode"] | defaultMode;
    const char* wifi_name = doc["wifi_name"] | deviceName;
    const char* wifi_password = doc["wifi_pass"] | devicePassword;
    const char* ap_name = doc["ap_name"] | deviceName;
    const char* ap_pass = doc["ap_pass"] | devicePassword;

    wirelessConfig.wireless_mode = wifiMode;
    wirelessConfig.station_ssid = wifi_name;
    wirelessConfig.station_psk = wifi_password;
    wirelessConfig.ap_ssid = ap_name;
    wirelessConfig.ap_psk = ap_pass;
    
    if(file) file.close();

    if(strcmp(wifiMode,"WIFI") == 0){
        WiFi.mode(WIFI_STA);
        // begin wifi connection.
        WiFi.begin(wifi_name, wifi_password);

        printSerial(String("Connecting to wifi network \"") + wifi_name + "\" Using password: \"" + wifi_password + "\"");

        for(int i = 0; i < wirelessTimeout * 2 ; i++){
            digitalWrite(LED, LOW);

            // check wifi connection status
            if(WiFi.status() == WL_CONNECTED){
                printSerial("WiFi Connection established...");
                printSerial("IP Address: ");
                if(SERIAL_MONITOR_ENABLED) Serial.println(WiFi.localIP());
                delay(2000);
                digitalWrite(LED, HIGH);
                break;
            }
            
            // Delay 500 milli-seconds            
            printSerial(".", ".");delay(50);
            digitalWrite(LED, HIGH);
            delay(450);
        }
        printSerial(".");        
    }
    
    // begin AP if config stored as softAP or wifi connection timed out.
    if(strcmp(wifiMode, "AP") == 0 || WiFi.status() != WL_CONNECTED){
        WiFi.mode(WIFI_AP);
        printSerial(String("Beginning SoftAP \"") + ap_name, "\"");
        WiFi.softAP(ap_name, ap_pass, 1, 0, 5);
        printSerial("IP Address:", " ");
        if(SERIAL_MONITOR_ENABLED) Serial.println(WiFi.softAPIP());
        ledPulse(1000,2000,3);
    }
}

void ledPulse(int _on, int _off, int _count){
   /*
    * _on : Board LED LOW state time in milli-seconds
    * _off: Board LED HIGH state time in mill-seconds
    * _count: count of Board LED Blink
    */
    int count = 0;
    while(count<_count){
        printSerial(".", ".");
        digitalWrite(LED, LOW);  delay(_on);
        digitalWrite(LED, HIGH);  delay(_off);
        count++;
    }
    printSerial(".");
}

void initCredentials(){
   /*
    * Load username and user password from storage.
    */
    printSerial(String("Reading File: ") + LoginCredential, "...  ");
    File file = LittleFS.open(LoginCredential, "r");
    printSerial((!file)? " Failed!, falling back to default configs." : " Done.");

    const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
    DynamicJsonDocument doc(capacity);

    printSerial("Parsing File...", "  ");
    // parse config file.
    DeserializationError error = deserializeJson(doc, file);
    printSerial((error)? "Failed!, falling back to default configs." : "Done.");

    const char* user = doc["username"] | deviceName;
    const char* pass = doc["password"] | devicePassword;
    
    userConfig.user = user;
    userConfig.pass = pass;

    if(file) file.close();    
}

void loop() {
   /*
    * default loop instructions
    */

    // checkResetState(4);

    ArduinoOTA.handle();
    handleDatagram();
    handleSocket();
    if(wireless_updated){
        initWireless();
        wireless_updated = false;
    }
}

void handleDatagram(){
    char packet[udpPacketSize];
    int packetSize = UDP.parsePacket();
    if(packetSize){
        int len = UDP.read(packet, udpPacketSize);
        if (len > 0){
            packet[len] = '\0';
        }
        const size_t capacity = JSON_OBJECT_SIZE(1) + 128;
        DynamicJsonDocument doc(capacity);
        printSerial("Parsing request from udp client...");
        DeserializationError error = deserializeJson(doc, packet);
        if (error){
            printSerial("Failed, abort operation.");
            return;
        }
        
        const char* req = doc["request"] | "undefined";
        IPAddress remoteIP = UDP.remoteIP();
        printSerial(String("request from ") + remoteIP.toString() + ": " + req);

        if(strcmp(req, "ping") == 0){
            String response = String("{\"MAC\": \"") + WiFi.macAddress() + "\"}";
            UDP.beginPacket(remoteIP, udpPortApp);          
            UDP.print(response);
            UDP.endPacket();
            printSerial("response: ", response.c_str());
            printSerial("");
        }
        else printSerial("request not identified, abort.");
    }
}

void handleSocket(){
    // Check for Socket Client
    WiFiClient client = socketServer.available();
    // client will be null if socket client is not connected.
    if (client) {
        if(client.connected()){
            printSerial("Client Connected, client IP:", " ");
            if(SERIAL_MONITOR_ENABLED) Serial.println(client.remoteIP());
        }
        while(client.connected()){      
            while(client.available()){
                String result = requestHandler(client.readStringUntil('\n'), client);
                result.replace("\n","");
                client.println(result); 
                client.flush();
                client.stop();
            }
        }
        printSerial("Client disconnected.");    
    }
}

void checkResetState(const int pinNumber){
   /*
    * if pin state of pinNumber is HIGH for more than 10 seconds,
    * reset all the configurations to default.    
    */
    pinMode(pinNumber, INPUT);
    int pinState = digitalRead(pinNumber);
    uint32_t start_time = millis();
    int cur_time = 0;
    if(pinState == 1){
        printSerial("Reset button clicked, will reset config if reset button is kept on hold for 10 seconds");
    }
    int count = 0;
    while(cur_time < 10){
        cur_time = (millis() - start_time) / 1000; 
        if(pinState == 0) return;
        pinState = digitalRead(pinNumber);
    }
    printSerial("Confirmed, begin Reset...");
    factoryReset();
}

bool factoryReset(){
   /*
    * reset all configurations to default settings.
    */
    printSerial("Factory reset begin...");
    if(LittleFS.format()){
        printSerial("Factory reset successful.");
    }else{
        printSerial("Factory reset failed.");
        return false;
    }
    
    printSerial(String("Creating File: ") + GPIOConfigFile, "...  ");
    File file = LittleFS.open(GPIOConfigFile, "w");  

    if (!file){
        printSerial("Failed!");
    }else{
        printSerial("Done.");      
        file.print("[]");
        file.close();
    }
    setCredentials(deviceName, devicePassword);
    setWireless(defaultMode, deviceName, devicePassword);
    printSerial("Reset completed.");

    return true;
}

String requestHandler(String request, WiFiClient client){
   /*
    * parse request (JSON) and return appropriate response (JSON)
    */
    const size_t capacity = JSON_OBJECT_SIZE(6) + (request.length() * 1.5);

    if(capacity > maxRequestLength )
        return String("{\"response\":\"request length exceeded the limit!!\"}");
    
    DynamicJsonDocument doc(capacity);
    printSerial("Parsing request from socket client...", "  ");
    DeserializationError error = deserializeJson(doc, request.c_str());
    if (error){
        printSerial("Failed.");
        return String("{\"response\":\"JSON Error, failed to parse the request\"}");
    }
    printSerial("Done.");
    
    const char* req = doc["request"] | "undefined";
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";

    printSerial("Incomming request:", " ");
    printSerial(req);

    if(strcmp(req, "undefined") == 0){
        return String("{\"response\":\"Purpose not defined\"}");
    }
    

    // response with mac address for ping request.
    else if(strcmp(req, "ping") == 0){
        return String("{\"MAC\":\"") + WiFi.macAddress() + "\"}";
    }
    
    // check username and password from client
    else if(strcmp(req, "authenticate") == 0){
        if(authenticate(username, password)){
            return String("{\"response\":\"authenticated\"}");
        }else return String("{\"response\":\"deny\"}");
    }
    
    // capture and send IR data to socket client
    else if(strcmp(req, "ir_capture") == 0){
        if(authenticate(username, password)){
            const int cap_mode = doc["capture_mode"] | 0;
            return irCapture(cap_mode, client);
        }else return String("{\"response\":\"deny\"}");
    }
    
    // send IR data from client using IR transmitter
    else if(strcmp(req, "ir_send") == 0){
        if(authenticate(username, password)){
            const char* irData = doc["irCode"] | "";
            const char* len = doc["length"] | "0";
            const char* protocol = doc["protocol"] | "UNKNOWN";
            uint16_t size = strtol(len, NULL, 16);
                      
            digitalWrite(LED, LOW);
            String result =  irSend(size, protocol, irData);
            digitalWrite(LED,HIGH);
            return result;
        }else return String("{\"response\":\"deny\"}");
    }
    
    // set wireless mode, wireless name and password
    else if(strcmp(req, "set_wireless") == 0){
        if(authenticate(username,password)){
            const char* wireless_mode = doc["wireless_mode"] | "AP";
            if(strcmp(wireless_mode, "WIFI") == 0){
                const char* wireless_name = doc["new_ssid"] | wirelessConfig.station_ssid.c_str();
                const char* wireless_pass = doc["new_pass"] | wirelessConfig.station_psk.c_str();
                return setWireless("WIFI", wireless_name, wireless_pass);
            }else{
                const char* ap_name = doc["new_ssid"] | wirelessConfig.ap_ssid.c_str();
                const char* ap_pass = doc["new_pass"] | wirelessConfig.ap_psk.c_str();
                return setWireless("AP", ap_name, ap_pass);
            }
        }else return String("{\"response\":\"deny\"}");
    }
    
    // set new username and password    
    else if(strcmp(req, "set_user") == 0){
        if(authenticate(username, password)){
            const char* user = doc["new_username"] | username;
            const char* pass = doc["new_password"] | password;
            return setCredentials(user, pass);
        }else return String("{\"response\":\"deny\"}");
    }
    
    // return wireless settings
    else if(strcmp(req, "get_wireless") == 0){
        if(authenticate(username, password)){
          return getWireless();
        }else return String("{\"response\":\"deny\"}");
    }
    
    // add/update GPIO settings.
    else if(strcmp(req, "gpio_set") == 0){
        if(authenticate(username, password)){
            const int GPIOPinNumber = doc["pinNumber"];
            const char* GPIOPinMode = doc["pinMode"];
            const int GPIOPinValue = doc["pinValue"];
            return applyGPIO(GPIOPinNumber, GPIOPinMode, GPIOPinValue);
        }else return String("{\"response\":\"deny\"}");
    }
    
    // return GPIO Settings.
    else if(strcmp(req, "gpio_get") == 0){
        if(authenticate(username, password)){
            const int GPIOPinNumber = doc["pinNumber"];
            return getGPIO(GPIOPinNumber);
        }else return String("{\"response\":\"deny\"}");
    }
    
    // restart device
    else if(strcmp(req, "restart") == 0){
        if(authenticate(username, password)){
            ESP.restart();
        }else return String("{\"response\":\"deny\"}");
    }

    else if(strcmp(req, "reset") == 0){
        if(authenticate(username, password)){
            return String("{\"response\": \"") + (factoryReset()? "success": "failure") + "\"}";
        }else return String("{\"response\":\"deny\"}");
    }
    
    else{
        return String("{\"response\":\"Invalid Purpose\"}");
    }
    return String("{\"response\":\"operation complete\"}");    
}

String getWireless(){
   /*
    * return wireless settings (JSON) string
    */  
    String result = "{\"wireless_mode\":\"";
    result += wirelessConfig.wireless_mode + "\",\"station_ssid\":\"" + wirelessConfig.station_ssid + "\",\"station_psk\":\"" + wirelessConfig.station_psk + "\",\"ap_ssid\":\"" + wirelessConfig.ap_ssid + "\",\"ap_psk\":\"" + wirelessConfig.ap_psk + "\"}";
    return result;
}

String setWireless(const char* wireless_mode, const char* wireless_name, const char* wireless_passwd){
   /*
    * apply and save new wireless settings
    * wireless_mode: WIFI / AP
    */  
    LittleFS.remove(WiFiConfigFile);
    printSerial(String("Writing File : ") + WiFiConfigFile, " ");
    File file = LittleFS.open(WiFiConfigFile, "w");
    if (!file){
        printSerial(" Failed!, falling back to default configs.");
        return String("{\"response\":\"Config File Creation Failed\"}");
    }

    const size_t capacity = JSON_OBJECT_SIZE(5) + 240;
    DynamicJsonDocument doc(capacity);
    
    if(strcmp(wireless_mode,"WIFI") == 0){
        doc["mode"] = "WIFI";
        // if wireless_mode is WIFI, add mew settings to wifi_name and wifi_pass
        doc["wifi_name"] = wireless_name;
        doc["wifi_pass"] = wireless_passwd;
        doc["ap_name"] = wirelessConfig.ap_ssid.c_str();
        doc["ap_pass"] = wirelessConfig.ap_psk.c_str();
    }else{
        doc["mode"] = "AP";
        doc["wifi_name"] = wirelessConfig.station_ssid.c_str();
        doc["wifi_pass"] = wirelessConfig.station_psk.c_str();
        // if wireless_mode is AP, add mew settings to ap_name and ap_pass
        doc["ap_name"] = wireless_name;
        doc["ap_pass"] = wireless_passwd;
    }

    // save new configuration to WiFiConfigFile
    if (serializeJson(doc, file) == 0) {
        printSerial(" Failed, falling back to default configs.");
        if(file) file.close();
        return String("{\"response\":\"Config File Write Failed\"}");
    }
    printSerial(" Done.");
    if(file) file.close();
    wireless_updated = true;
    return String("{\"response\":\"Wireless config successfully applied\"}");
}

String setCredentials(const char* user_name,const char* passwd){
   /*
    * set new username and user password
    */   
    
    LittleFS.remove(LoginCredential);
    printSerial(String("Writing File: ") + LoginCredential, " ");
    File file = LittleFS.open(LoginCredential, "w");
    if (!file)
        printSerial("Failed!, falling back to default configs");
        
    const size_t capacity = JSON_OBJECT_SIZE(2) + 100;
    DynamicJsonDocument doc(capacity);
    doc["username"] = user_name;
    doc["password"] = passwd;
    if (serializeJson(doc, file) == 0) {
        printSerial("Failed!, falling back to default configs");
        if(file) file.close();
        return String("{\"response\":\"Config File Write Failed\"}");
    }
    printSerial("  Done.");
    if(file) file.close();
    initCredentials();
    return String("{\"response\":\"User config successfully applied\"}");
}

String generateIrResult(const decode_results * const results){
   /*
    * generate IR data in string format using decode_results
    */
    decode_type_t protocol = results->decode_type;
    uint16_t size = results->bits;
    
    irrecv.disableIRIn();
    printSerial(typeToString(protocol));
    String output = String("{\"response\":\"success\",\"protocol\":\"") + typeToString(protocol) + "\",\"length\":\"";
    if (protocol == decode_type_t::UNKNOWN) {
        output += uint64ToString(getCorrectedRawLength(results), 16);
        output += F("\",\"irCode\":\"[ ");

        for (uint16_t i = 1; i < results->rawlen; i++) {
            uint32_t usecs;
            for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
                output += uint64ToString(UINT16_MAX);
                output += (i % 2)? F(", 0,  "): F(",  0, ");
            }
            output += uint64ToString(usecs, 10);
            if (i < results->rawlen - 1) output += kCommaSpaceStr;
            if (i % 2 == 0) output += ' ';
        }
        output += F(" ]\"}");
    } else if (hasACState(protocol)) {
          uint16_t nbytes = results->bits / 8;
          output += uint64ToString(nbytes, 16);
          output += F("\",\"irCode\":\"[ ");
          for (uint16_t i = 0; i < nbytes; i++) {
              output += F("'0x");
              if (results->state[i] < 0x10) output += '0';
              output += uint64ToString(results->state[i], 16);
              output += F("'");
              if (i < nbytes - 1) output += kCommaSpaceStr;
          }
          output += F("]\"}");
    } else {
        output += uint64ToString(size, 16); 
        output += "\",\"irCode\":\"" + uint64ToString(results->value, 16) + "\"}";
    }
    output.replace(" ", "");
    return output;
}

String irCapture(bool multiCapture, WiFiClient client){
   /*
    * Captire signal from IR Remote control and return IR data.
    * capture more than one signal if multicapture is true.
    */
    printSerial("Beginning ir capture procedure" );
    irrecv.enableIRIn();
    String result = "{\"response\":\"timeout\"}";
    uint32_t start_time = millis();
    int cur_val = 0;
    int prev_val = cur_val;
    String res = String("{\"response\":\"progress\",\"value\":");
    String temp = res + recv_timeout;
    temp += "}";
    client.println(temp);
    client.flush();
    printSerial(String("start time: ") + start_time);
    int count = 0;
    while((cur_val < recv_timeout) && client.connected()){
        cur_val = (millis() - start_time) / 1000;

        if(cur_val > prev_val){
            digitalWrite(LED, HIGH);
            temp = res + (recv_timeout - cur_val);
            temp += "}";
            client.println(temp);
            client.flush();
            prev_val = cur_val;
        }  
        
        if (irrecv.decode(&results)) {
          irrecv.disableIRIn();
            if(multiCapture){
                digitalWrite(LED,HIGH);
                client.println(generateIrResult(&results));
                client.flush();
                irrecv.enableIRIn();
                result = "{\"response\":\"success\"}";
                start_time = millis();
                cur_val = 0;
                prev_val = -1;
                irrecv.resume();
            }else{
                result = generateIrResult(&results);
                break;
            }
        }
        if(count % 100 == 0){
            digitalWrite(LED, LOW);
        }else if((count - 8) % 100 == 0){
            digitalWrite(LED, HIGH);
        }      
        delay(1);
        yield();
        count++;
    }
    digitalWrite(LED, HIGH);
    return result;
}

bool authenticate(const char* username, const char* password){
   /**
    * check authenticity of username and password
    */
    return (userConfig.user == username && userConfig.pass == password);
}

String irSend(uint16_t size, const char* protocol_str, const char* irData){
   /*
    * send IR signal (irData) through IR Transmitter using
    */
    decode_type_t protocol = strToDecodeType(protocol_str);
    if (protocol == decode_type_t::UNKNOWN) {
        sendRawArray(size, irData);
    } else{ 
        bool success = (hasACState(protocol))? sendIrState(size, protocol, irData): sendIrValue(size, protocol, irData);
        return String("{\"response\":\"") + typeToString(protocol) + " " + (success? "success": "failure") + "\"}";
    }
    return String("{\"response\":\"success\"}");
}

void sendRawArray(uint16_t size, const char* irData){
   /*
    * when irData is int array.
    */
    const size_t capacity = JSON_ARRAY_SIZE(size);
    DynamicJsonDocument json(capacity);
    deserializeJson(json, irData);
    uint16_t rawData[size];
    for(int i = 0; i < size; i++){
        rawData[i] = json[i];
    }
    irsend.sendRaw(rawData, size, kFrequency);
}

bool sendIrValue(uint16_t size, decode_type_t protocol, const char* irData){
   /*
    * when irData is hex value.
    */
    uint64_t value = getUInt64fromHex(irData);
    return irsend.send(protocol, value, size);
}

bool sendIrState(uint16_t size, decode_type_t protocol, const char* data){
   /*
    * when irData is array of hex values.
    */
    String stateListString = String(data);
    stateListString.replace("'", "\"");
    const size_t capacity = JSON_ARRAY_SIZE(size) + (6 * size);
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, stateListString.c_str());
    printSerial(stateListString);
    uint8_t stateList[size];
    for(int i = 0; i < size; i++){
        const char* hexStr = doc[i];
        stateList[i] = strtol(hexStr, NULL, 16);
    }
    return irsend.send(protocol, stateList, size);
}

uint64_t getUInt64fromHex(char const *hex) {
   /*
    * convert hex string to uint64_t
    */
    uint64_t result = 0;
    uint16_t offset = (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))? 2: 0;
    for (; isxdigit((unsigned char)hex[offset]); offset++) {
        char c = hex[offset];
        result *= 16;
        result += c - (isdigit(c)? '0': ((isupper(c)? 'A' : 'a') + 10));
    }
    return result;
}

size_t printSerial(const String serial, const char* end){
    return SERIAL_MONITOR_ENABLED ? Serial.print(serial) + Serial.print(end) : 0;        
}

size_t printSerial(const char* serial, const char* end){
    return SERIAL_MONITOR_ENABLED ? Serial.print(serial) + Serial.print(end) : 0; 
}
