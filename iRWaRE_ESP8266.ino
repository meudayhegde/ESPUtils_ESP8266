#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

#define LEGACY_TIMING_INFO false

const char* WiFiConfig = "/WiFiConfig.json";
const char* LoginCredential = "/LoginCredential.json";
const int port = 48321;
const int recv_timeout = 8;

const uint16_t kRecvPin = 14;
const uint16_t kIrLed = 4;

const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;

String USERNAME;
String PASSWORD;

#if DECODE_AC
    const uint8_t kTimeout = 50;
#else
    const uint8_t kTimeout = 15;
#endif 

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
    
    while (!Serial) delay(50);
    
    SPIFFS.begin();
    initWireless();
    server.begin();
    
    #if DECODE_HASH
        irrecv.setUnknownThreshold(kMinUnknownSize);
    #endif

    irsend.begin();

    initUser();
}

void loop() {
    WiFiClient client = server.available();
    if (client) {
        if(client.connected()){
            Serial.print("Client Connected, client IP: ");Serial.println(client.remoteIP());
        }
        while(client.connected()){      
            while(client.available()>0){
                String result = requestHandler(client.readStringUntil('\n'));
                Serial.println(String("response: "+result));
                result.replace("\n","");
                client.println(result); 
                client.flush();
            }
        }
        client.stop();
        Serial.println("Client disconnected");    
    }
}

void initUser(){
    String content = readFromFile(LoginCredential,String("{\"username\":\"_-!#__\",\"password\":\"!!+@-_<!7a>\"}"));
    const size_t capacity = JSON_OBJECT_SIZE(2) + 100;
    DynamicJsonDocument jsonDocument(capacity);
    deserializeJson(jsonDocument, content.c_str());
    const char* username = jsonDocument["username"];
    const char* password = jsonDocument["password"];

    USERNAME = username;
    PASSWORD = password;
}

String requestHandler(String request){
    Serial.print("Incomming request: ");Serial.println(request);

    const size_t capacity = JSON_OBJECT_SIZE(5) + 1024;
    DynamicJsonDocument doc(capacity);
   
    deserializeJson(doc, request);
    
    const char* req = doc["request"];
    const char* username = doc["username"];
    const char* password = doc["password"];
    const char* data = doc["data"];
    const int len = doc["length"];

    if(String(req)=="ping"){
        return String("{\"MAC\":\"")+WiFi.macAddress()+"\"}";
    }else if(String(req)=="authenticate"){
        if(authenticate(username,password)){
            return String("{\"response\":\"authenticated\"}");
        }else return String("{\"response\":\"deny\"}");
    }else if(String(req)=="ir_capture"){
        if(authenticate(username,password)){
            return irCapture();
        }else return String("{\"response\":\"deny\"}");
    }else if(String(req)=="ir_send"){
        if(authenticate(username,password)){
            return irSend(len,data);
        }else return String("{\"response\":\"deny\"}");
    }else if(String(req)=="set_config"){
        return String("");
    }else{
        return String("--empty response--");
    }
}

String irSend(int len,const char* rawdataString){
    const size_t capacity = JSON_ARRAY_SIZE(len);
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, rawdataString);

    uint16_t rawData[len];
    
    for(int i=0;i<len;i++){
        rawData[i] = doc[i];
    }

    irsend.sendRaw(rawData, len, 38);

    return String("{\"response\":\"success\"}");
}

String irCapture(){
    Serial.println("beginning ir capture procedure" );
    irrecv.enableIRIn();
    String result="{\"response\":\"timeout\"}";
    uint32_t start_time = millis();
    Serial.println(String("start time: ")+start_time);
    while((millis()-start_time) < (1000*recv_timeout)){
        if (irrecv.decode(&results)) {
            yield();
            result = String("{\"response\":\"rawData\",")+generateRawResponse(&results)+"}";
            irrecv.disableIRIn();
            return result;
        }
        delay(10);
    }
    irrecv.disableIRIn();
    return result;
}

bool authenticate(const char* username, const char* password){
  Serial.println(USERNAME);
  Serial.println(PASSWORD);
    if(USERNAME==username && PASSWORD==password) return true; else return false;
}

void initWireless(){
  /* 
   * initialize wifi/softAP based on the settings saved in wifi config file.
   */
    
    String content = readFromFile(WiFiConfig,String("{\"mode\":\"AP\",\"wifi_name\":\"myJio-Hotspot\",\"wifi_pass\":\"finalDestination\",\"ap_name\":\"iRWaRE\",\"ap_pass\":\"infraredEverywhere\"}"));
    const size_t capacity = JSON_OBJECT_SIZE(5) + 120;
    DynamicJsonDocument jsonDocument(capacity);
    deserializeJson(jsonDocument, content.c_str());
    const char* mode = jsonDocument["mode"];
    const char* wifi_name = jsonDocument["wifi_name"];
    const char* wifi_password = jsonDocument["wifi_password"];
    const char* ap_name = jsonDocument["ap_name"];
    const char* ap_pass = jsonDocument["ap_pass"];
    
    if(String(mode)=="WIFI"){
      //config prefered as wifi
        WiFi.begin(wifi_name, wifi_password);
        Serial.print("connecting to wifi network: ");
        Serial.println(wifi_name);
        for(int i=0;i<20;i++){
            if(WiFi.status() == WL_CONNECTED){
              Serial.println("WiFi Connection established...");
              Serial.print("IP Address: ");Serial.println(WiFi.localIP());
              break;
            }
            Serial.print(".");
            delay(500);
        }
        Serial.println();
    }if(String(mode)=="AP" || WiFi.status() != WL_CONNECTED){
      //config prefered as softAP or wifi connection timed out 
        Serial.print("Beginning SoftAP: ");
        Serial.println(ap_name);
        WiFi.softAP(ap_name,ap_pass,1,0,5);
        Serial.print("IP Address: ");Serial.println(WiFi.softAPIP());
    }
}

String readFromFile(const char* file_path,String default_str){
    File file = SPIFFS.open(file_path, "r");
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

String generateRawResponse(const decode_results * const results) {
    String output = "\"length\":";
    output += uint64ToString(getCorrectedRawLength(results), 10);
    output += F(",\"irCode\":[ ");

    for (uint16_t i = 1; i < results->rawlen; i++) {
        uint32_t usecs;
        for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX;usecs -= UINT16_MAX) {
            output += uint64ToString(UINT16_MAX);
            if (i % 2)
                output += F(", 0,  ");
            else
                output += F(",  0, ");
        }
        output += uint64ToString(usecs, 10);
        if (i < results->rawlen - 1)
            output += kCommaSpaceStr;
        if (i % 2 == 0) output += ' ';
    }

    output += F(" ] ");
    return output;
}
