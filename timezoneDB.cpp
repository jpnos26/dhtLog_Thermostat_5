#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
 
unsigned long timezoneRead()
 {
      Serial.print("Ora Corrente da server Internet: " );
      String keyDB = "NSQ0TEUMGUED";        ///chiave database internet
      HTTPClient http;
      // We now create a URI for the request
      String url = "http://api.timezonedb.com/v2/get-time-zone?key="+keyDB+"&format=json&by=zone&zone=Europe/Rome";
      Serial.print("Requesting URL: ");Serial.println(url);
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      String json;
      int httpCode = http.GET();
      if (httpCode) {
        if (httpCode == 200) {
          json = http.getString();
          Serial.println(json);
        }
      }
      http.end();
      const size_t bufferSize = JSON_OBJECT_SIZE(13) + 250;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      
      Serial.print("Got data:");Serial.println(json);
      JsonObject& root = jsonBuffer.parseObject(json);
      if (!root.success()){
        Serial.println("parseObject() failed");
        return (0);
        }
      String data = root["status"];
      int data1= root["gmtOffset"]; 
      unsigned long data2 = root["timestamp"]; 
      if (data =="OK"){
          data2 -= 946684800UL;
          return(data2);
      }
      else {
        return (0);
      }
    
 }

  
