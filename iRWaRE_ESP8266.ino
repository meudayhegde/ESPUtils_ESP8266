#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

const char* WiFiConfig = "/WiFiConfig.json";
int port = 48321;
WiFiServer server(port);

void setup() {
    Serial.begin(115200);
    SPIFFS.begin();
    initWireless();
    server.begin();
}

void loop() {
    WiFiClient client = server.available();
    if (client) {
        if(client.connected()){
            Serial.print("Client Connected, client IP: ");Serial.println(client.remoteIP());
        }
        while(client.connected()){      
            while(client.available()>0){
                client.println(requestHandler(client.readStringUntil('\n'))); 
                client.flush();
            }
        }
        client.stop();
        Serial.println("Client disconnected");    
    }
}

String requestHandler(String request){
    Serial.print("Incomming request: ");Serial.println(request);

    const size_t capacity = JSON_OBJECT_SIZE(4) + 520;
    DynamicJsonDocument doc(capacity);
   
    deserializeJson(doc, request);
    
    const char* req = doc["request"], username = doc["username"], password = doc["password"],data = doc["data"];

    if(String(req)=="ping"){
        return String("{\"MAC\":\"")+WiFi.macAddress()+"\"}";
    }else if(String(req)=="authenticate"){
        return String("verified");
    }else if(String(req)=="ir_capture"){
        return String("");
    }else if(String(req)=="ir_send"){
        return String("");
    }else if(String(req)=="set_config"){
        return String("");
    }else{
        return String("--empty response--");
    }
}

void initWireless(){
  /* 
   * initialize wifi/softAP based on the settings saved in wifi config file.
   */
    
    String content="";
    File file = SPIFFS.open(WiFiConfig, "r");
    if (!file) {
        Serial.println("file open failed!, falling back to default configs");
        content = "{\"mode\":\"AP\",\"wifi_name\":\"myJio-Hotspot\",\"wifi_pass\":\"finalDestination\",\"ap_name\":\"iRWaRE\",\"ap_pass\":\"infraredEverywhere\"}";
    }else{
        Serial.print("Reading Data from File :");Serial.println(WiFiConfig);
        content = file.readStringUntil('\r');
        file.close();
        Serial.println("File Closed");
    }
    
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
