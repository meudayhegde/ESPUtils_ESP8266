#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

#define LEGACY_TIMING_INFO false
#define LED 02

const char* WiFiConfigFile = "/WiFiConfig.json";
const char* LoginCredential = "/LoginCredential.json";
const char* GPIOConfigFile = "/GPIOConfig.json";

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

const int port = 48321;
const int recv_timeout = 8;

const uint16_t kRecvPin = 14;
const uint16_t kIrLed = 4;

const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;

const uint8_t kTimeout = 50;  // Milli-Seconds

const uint16_t kFrequency = 38000;

const uint16_t kMinUnknownSize = 12;

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
IRsend irsend(kIrLed);

decode_results results;

WiFiServer server(port);

void setup() {
    #if defined(ESP8266)
        Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
    #else
        Serial.begin(kBaudRate, SERIAL_8N1);
    #endif
    pinMode(LED,OUTPUT);
    LittleFS.begin();
    applyGPIO(-1, "", 0);
    initWireless();
    server.begin();
    #if DECODE_HASH
        irrecv.setUnknownThreshold(kMinUnknownSize);
    #endif
    irsend.begin();
    initUser();
}

void applyGPIO(const int GPIOPinNumber, const char* GPIOPinMode, const int GPIOPinValue){
  /*
   * Apply GPIO settings and store in config file
   */
    Serial.print(String("Reading File: ") + GPIOConfigFile + "...  ");
    File file = LittleFS.open(GPIOConfigFile, "r");    
    Serial.println((!file)? " Failed!, falling back to default configs." : " Done.");

    String gpioConfig = file.readStringUntil(']') + "]";
    const size_t capacity = gpioConfig.length() + 512;
    DynamicJsonDocument doc(capacity);

    Serial.print("Parsing File...  ");
    DeserializationError error = deserializeJson(doc, gpioConfig);
    Serial.println((error)? "Failed!, falling back to default configs." : "Done.");

    JsonArray array = doc.as<JsonArray>();
    bool pinConfExist = false;
    for(JsonVariant gpio : array) {
        const int pin = gpio["pinNumber"];
        if(GPIOPinNumber != -1 && pin == GPIOPinNumber){
            gpio["pinNumber"] = GPIOPinNumber;
            gpio["pinMode"] = GPIOPinMode;
            gpio["pinValue"] = GPIOPinValue;
        }
        if(GPIOPinNumber == -1 || pin == GPIOPinNumber){
            pinConfExist = true;
            const char* PinMode = gpio["pinMode"];
            const int PinValue = gpio["pinValue"];
            if(strcmp(PinMode, "OUTPUT") == 0){
                pinMode(pin, OUTPUT);
                digitalWrite(pin, PinValue);
            }
        }
    }
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
    
    file.close();

    Serial.print(String("Writing File: ") + GPIOConfigFile + "...  ");
    file = LittleFS.open(GPIOConfigFile, "w");  
    Serial.println((!file)? " Failed!, falling back to default configs." : " Done.");

    serializeJson(doc, file);
}

String getGPIO(const int pinNumber){
    Serial.print(String("Reading File: ") + GPIOConfigFile + "...  ");
    File file = LittleFS.open(GPIOConfigFile, "r"); 
    Serial.println((!file)? " Failed!, falling back to default configs." : " Done.");
    
    String gpioConfig = file.readStringUntil(']') + "]";
    if(pinNumber == -1){
        return gpioConfig;
    }
    const size_t capacity = gpioConfig.length() + 512;
    DynamicJsonDocument doc(capacity);
    
    Serial.print("Parsing File...  ");
    DeserializationError error = deserializeJson(doc, gpioConfig);
    Serial.println((error)? "Failed!, falling back to default configs." : "Done.");
        
    JsonArray array = doc.as<JsonArray>();
    for(JsonVariant gpio : array) {
        const int pin = gpio["pinNumber"];
        if(pin == pinNumber){
            String output;
            serializeJson(gpio, output);
            file.close();
            return output;
        }
    }
    file.close();
}

void initWireless(){
  /* 
   * initialize wifi/softAP based on the settings saved in wifi config file.
   */
    Serial.print(String("Reading File: ") + WiFiConfigFile + "...  ");
    File file = LittleFS.open(WiFiConfigFile, "r");
    Serial.println((!file)? " Failed!, falling back to default configs." : " Done.");

    const size_t capacity = JSON_OBJECT_SIZE(5) + 240;
    DynamicJsonDocument doc(capacity);

    Serial.print("Parsing File...  ");
    DeserializationError error = deserializeJson(doc, file);
    Serial.println((error)? "Failed!, falling back to default configs." : "Done.");

    const char* wifiMode = doc["mode"] | "AP";
    const char* wifi_name = doc["wifi_name"] | "iRWaRE";
    const char* wifi_password = doc["wifi_pass"] | "infrared.redefined";
    const char* ap_name = doc["ap_name"] | "iRWaRE";
    const char* ap_pass = doc["ap_pass"] | "infrared.redefined";

    wirelessConfig.wireless_mode = wifiMode;
    wirelessConfig.station_ssid = wifi_name;
    wirelessConfig.station_psk = wifi_password;
    wirelessConfig.ap_ssid = ap_name;
    wirelessConfig.ap_psk = ap_pass;
    
    file.close();

    if(strcmp(wifiMode,"WIFI") == 0){
      //config prefered as wifi
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifi_name, wifi_password);

        Serial.println(String("Connecting to wifi network \"") + wifi_name + "\" Using password: \"" + wifi_password + "\"");

        for(int i = 0; i < 40 ; i++){
            digitalWrite(LED,LOW);
            if(WiFi.status() == WL_CONNECTED){

                Serial.println("WiFi Connection established...");
                Serial.print("IP Address: ");Serial.println(WiFi.localIP());

                delay(2000);
                digitalWrite(LED,HIGH);
                break;
            }
            Serial.print(".");
            delay(50);
            digitalWrite(LED,HIGH);
            delay(450);
        }
        Serial.println();
    }if(strcmp(wifiMode, "AP") == 0 || WiFi.status() != WL_CONNECTED){
      //config prefered as softAP or wifi connection timed out 
        WiFi.mode(WIFI_AP);
        Serial.println(String("Beginning SoftAP \"") + ap_name + "\"");
        WiFi.softAP(ap_name, ap_pass, 1, 0,5);
        Serial.print("IP Address: ");Serial.println(WiFi.softAPIP());
        ledPulse(1000,2000,3);
    }
}

void ledPulse(int _on, int _off, int _count){
    int count = 0;
    while(count<_count){
        Serial.print(".");
        digitalWrite(LED,LOW);
        delay(_on);
        digitalWrite(LED,HIGH);
        delay(_off);
        count++;
    }
    Serial.println();
}

void initUser(){
    Serial.print(String("Reading File: ") + LoginCredential + "...  ");
    File file = LittleFS.open(LoginCredential, "r");
    Serial.println((!file)? " Failed!, falling back to default configs." : " Done.");

    const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
    DynamicJsonDocument doc(capacity);

    Serial.print("Parsing File...  ");
    DeserializationError error = deserializeJson(doc, file);
    Serial.println((error)? "Failed!, falling back to default configs." : "Done.");

    const char* user = doc["username"] | "iRWaRE";
    const char* pass = doc["password"] | "infrared.redefined";
    
    userConfig.user = user;
    userConfig.pass = pass;

    file.close();    
}

void loop() {
    checkResetState(4);
    WiFiClient client = server.available();
    if (client) {
        if(client.connected()){
            Serial.print("Client Connected, client IP: ");Serial.println(client.remoteIP());
        }
        while(client.connected()){      
            while(client.available()){
                String result = requestHandler(client.readStringUntil('\n'),client);
                result.replace("\n","");
                client.println(result); 
                client.flush();
                client.stop();
            }
        }
        Serial.println("Client disconnected");    
    }
    if(wireless_updated){
        initWireless();
        wireless_updated = false;
    }
}

void checkResetState(const int pinNumber){
    pinMode(pinNumber, INPUT);
    int pinState = digitalRead(pinNumber);
    uint32_t start_time = millis();
    int cur_time = 0;
    if(pinState == 1){
        Serial.println("Reset button clicked, will reset config if reset button is kept on hold for 10 seconds");
    }
    int count = 0;
    while(cur_time < 10){
        cur_time = (millis()-start_time)/1000; 
        if(pinState == 0) return;
        pinState = digitalRead(pinNumber);
    }
    Serial.println("Confirmed, begin Reset...");
    Serial.print(String("Clearing File: ") + GPIOConfigFile + "...  ");
    File file = LittleFS.open(GPIOConfigFile, "w");  

    if (!file){
        Serial.println("Failed!");
    }else{
        Serial.println("Done.");      
        file.print("[]");
        file.close();
    }
    setUser("iRWaRE", "infrared.redefined");
    setWireless("AP", "iRWaRE", "infrared.redefined");

    Serial.println("Reset completed.");
}

String requestHandler(String request, WiFiClient client){
    const size_t capacity = JSON_OBJECT_SIZE(6) + (request.length()*1.5);

    if(capacity > 5120 )
        return String("{\"response\":\"request size limit exceeded!!\"}");
    
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, request.c_str());
    if (error)
        return String("Invalid Request!");
    
    const char* req = doc["request"] | "undefined";
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    const char* irData = doc["irCode"] | "";
    const char* len = doc["length"] | "0";
    const char* protocol = doc["protocol"] | "UNKNOWN";
    const int cap_mode = doc["capture_mode"] | 0;

    uint16_t size = strtol(len,NULL,16);

    Serial.print("Incomming request: ");Serial.println(req);

    if(strcmp(req,"undefined") == 0){
        return String("{\"response\":\"Purpose Undefined\"}");
    }else if(strcmp(req,"ping") == 0){
        return String("{\"MAC\":\"")+WiFi.macAddress()+"\"}";
    }else if(strcmp(req,"authenticate") == 0){
        if(authenticate(username,password)){
            return String("{\"response\":\"authenticated\"}");
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req,"ir_capture") == 0){
        if(authenticate(username,password)){
            return irCapture(cap_mode,client);
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req,"ir_send") == 0){
        if(authenticate(username,password)){
            digitalWrite(LED,LOW);
            String result =  irSend(size,protocol,irData);
            digitalWrite(LED,HIGH);
            return result;
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req,"set_config") == 0){
        return String("");
    }else if(strcmp(req,"set_wireless") == 0){
        if(authenticate(username,password)){
            const char* wireless_mode = doc["wireless_mode"] | "AP";
            if(strcmp(wireless_mode,"WIFI") == 0){
                const char* wireless_name = doc["new_ssid"] | wirelessConfig.station_ssid.c_str();
                const char* wireless_pass = doc["new_pass"] | wirelessConfig.station_psk.c_str();
                return setWireless("WIFI",wireless_name,wireless_pass);
            }else{
                const char* wireless_name = doc["new_ssid"] | wirelessConfig.ap_ssid.c_str();
                const char* wireless_pass = doc["new_pass"] | wirelessConfig.ap_psk.c_str();
                return setWireless("AP",wireless_name,wireless_pass);
            }
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req,"set_user") == 0){
        if(authenticate(username,password)){
            const char* user = doc["new_username"] | username;
            const char* pass = doc["new_password"] | password;
            return setUser(user,pass);
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req,"get_wireless") == 0){
        if(authenticate(username,password)){
          return getWireless();
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req, "gpio_set") == 0){
        if(authenticate(username,password)){
            const int GPIOPinNumber = doc["pinNumber"];
            const char* GPIOPinMode = doc["pinMode"];
            const int GPIOPinValue = doc["pinValue"];
            applyGPIO(GPIOPinNumber, GPIOPinMode, GPIOPinValue);
            return String("{\"response\": \"success\"}");
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req, "gpio_get") == 0){
        if(authenticate(username,password)){
            const int GPIOPinNumber = doc["pinNumber"];
            return getGPIO(GPIOPinNumber);
        }else return String("{\"response\":\"deny\"}");
    }else if(strcmp(req, "restart") == 0){
        if(authenticate(username,password)){
            ESP.restart();
        }else return String("{\"response\":\"deny\"}");
    }else{
        return String("{\"response\":\"Invalid Purpose\"}");
    }
}

String getWireless(){
    String result = "{\"wireless_mode\":\"";
    result += wirelessConfig.wireless_mode + "\",\"station_ssid\":\"" + wirelessConfig.station_ssid + "\",\"station_psk\":\"" + wirelessConfig.station_psk + "\",\"ap_ssid\":\"" + wirelessConfig.ap_ssid + "\",\"ap_psk\":\"" + wirelessConfig.ap_psk + "\"}";
    return result;
}

String setWireless(const char* wireless_mode,const char* wireless_name,const char* wireless_passwd){
    LittleFS.remove(WiFiConfigFile);
    Serial.print(String("Writing File : ") + WiFiConfigFile);
    File file = LittleFS.open(WiFiConfigFile, "w");
    if (!file){
        Serial.println(" Failed!, falling back to default configs.");
        return String("{\"response\":\"Config File Creation Failed\"}");
    }
        
    const size_t capacity = JSON_OBJECT_SIZE(5) + 240;
    DynamicJsonDocument doc(capacity);
    
    if(strcmp(wireless_mode,"WIFI") == 0){
        doc["mode"] = "WIFI";
        doc["wifi_name"] = wireless_name;
        doc["wifi_pass"] = wireless_passwd;
        doc["ap_name"] = wirelessConfig.ap_ssid.c_str();
        doc["ap_pass"] = wirelessConfig.ap_psk.c_str();
    }else{
        doc["mode"] = "AP";
        doc["wifi_name"] = wirelessConfig.station_ssid.c_str();
        doc["wifi_pass"] = wirelessConfig.station_psk.c_str();
        doc["ap_name"] = wireless_name;
        doc["ap_pass"] = wireless_passwd;
    }

    if (serializeJson(doc, file) == 0) {
        Serial.println(" Failed, falling back to default configs.");
        file.close();
        return String("{\"response\":\"Config File Write Failed\"}");
    }
    Serial.println(" Done.")
    file.close();
    wireless_updated = true;
    return String("{\"response\":\"Wireless config successfully applied\"}");
}

String setUser(const char* user_name,const char* passwd){
    LittleFS.remove(LoginCredential);
    File file = LittleFS.open(LoginCredential, "w");
    if (!file)
        Serial.println("file open failed!, falling back to default configs");
        
    const size_t capacity = JSON_OBJECT_SIZE(2) + 100;
    DynamicJsonDocument doc(capacity);
    doc["username"] = user_name;
    doc["password"] = passwd;
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
        file.close();
        return String("{\"response\":\"Config File Write Failed\"}");
    }
    file.close();
    initUser();
    return String("{\"response\":\"User config successfully applied\"}");
}

String generateIrResult(const decode_results * const results){
    decode_type_t protocol = results->decode_type;
    uint16_t size = results->bits;
    
    irrecv.disableIRIn();
    Serial.println(typeToString(protocol));
    String output = String("{\"response\":\"success\",\"protocol\":\"") + typeToString(protocol) + "\",\"length\":\"";
    if (protocol == decode_type_t::UNKNOWN) {
        output += uint64ToString(getCorrectedRawLength(results), 16);
        output += F("\",\"irCode\":\"[ ");

        for (uint16_t i = 1; i < results->rawlen; i++) {
            uint32_t usecs;
            for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX;usecs -= UINT16_MAX) {
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
          output += uint64ToString(nbytes,16);
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
        output += uint64ToString(size,16); 
        output += "\",\"irCode\":\"" + uint64ToString(results->value, 16) + "\"}";
    }
    output.replace(" ","");
    return output;
}

String irCapture(bool multiCapture, WiFiClient client){
    Serial.println("beginning ir capture procedure" );
    irrecv.enableIRIn();
    String result="{\"response\":\"timeout\"}";
    uint32_t start_time = millis();
    int cur_val = 0;
    int prev_val = cur_val;
    String res = String("{\"response\":\"progress\",\"value\":");
    String temp = res + recv_timeout;
    temp += "}";
    client.println(temp);
    client.flush();
    Serial.println(String("start time: ")+start_time);
    int count = 0;
    while((cur_val < recv_timeout) && client.connected()){
        cur_val = (millis() - start_time)/1000;

        if(cur_val > prev_val){
            digitalWrite(LED,HIGH);
            temp = res+(recv_timeout-cur_val);
            temp+="}";
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
                start_time = millis();
                cur_val = 0;
                prev_val = -1;
                irrecv.resume();
            }else{
                result = generateIrResult(&results);
                break;
            }
        }
        if(count%100 == 0){
            digitalWrite(LED,LOW);
        }else if((count-8)%100 == 0){
            digitalWrite(LED,HIGH);
        }      
        delay(1);
        yield();
        count++;
    }
    digitalWrite(LED,HIGH);
    return result;
}

bool authenticate(const char* username, const char* password){
    Serial.println(userConfig.user);
    Serial.println(userConfig.pass);
    return (userConfig.user == username && userConfig.pass == password );
}

String readFromFile(const char* file_path,String default_str){
    File file = LittleFS.open(file_path, "r");
    if (!file) {
        Serial.println("file open failed!, falling back to default configs");
        return default_str;
    }else{
        Serial.print("Reading Data from File :");Serial.println(file_path);
        String content = file.readStringUntil('\r');
        file.close();
        Serial.println("File Closed");
        return content;
    }
}


String irSend(uint16_t size, const char* protocol_str, const char* irData){
    decode_type_t protocol = strToDecodeType(protocol_str);
    if (protocol == decode_type_t::UNKNOWN) {
        sendRawArray(size, irData);
    } else{ 
        bool success = (hasACState(protocol))? sendIrState(size, protocol, irData): sendIrValue(size, protocol, irData);
        return String("{\"response\":\"") + typeToString(protocol) + " " + (success? "success": "failure") + "\"}";
    }
    return String("{\"response\":\"success\"}");
}

void sendRawArray(uint16_t size,const char* irData){
    const size_t capacity = JSON_ARRAY_SIZE(size);
    DynamicJsonDocument json(capacity);
    deserializeJson(json, irData);
    uint16_t rawData[size];
    for(int i=0;i<size;i++){
        rawData[i] = json[i];
    }
    irsend.sendRaw(rawData, size, kFrequency);
}

bool sendIrValue(uint16_t size,decode_type_t protocol,const char* irData){
    uint64_t value = getUInt64fromHex(irData);
    return irsend.send(protocol, value, size);
}

bool sendIrState(uint16_t size,decode_type_t protocol, const char* data){
    String stateListString = String(data);
    stateListString.replace("'", "\"");
    const size_t capacity = JSON_ARRAY_SIZE(size) + (6*size);
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, stateListString.c_str());
    Serial.println(stateListString);
    uint8_t stateList[size];
    for(int i=0; i<size; i++){
        const char* hexStr = doc[i];
        stateList[i] = strtol(hexStr, NULL, 16);
    }
    return irsend.send(protocol, stateList, size);
}

uint64_t getUInt64fromHex(char const *hex) {
    uint64_t result = 0;
    uint16_t offset = (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))? 2: 0;
    for (; isxdigit((unsigned char)hex[offset]); offset++) {
        char c = hex[offset];
        result *= 16;
        result += c - (isdigit(c)? '0': ((isupper(c)? 'A' : 'a') + 10));
    }
    return result;
}
