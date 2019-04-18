#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFSEditor.h>
#include <Arduino.h>
//flag to use from web update to reboot the ESP
bool shouldReboot = true;

void setup_Server(){

    Serial.println ("Inizializzo OTA .........\n");
    String hostNAME = wifiName;
    hostNAME += "-";
    hostNAME += String(ESP.getChipId(), HEX);
    Serial.print("set OTA+WiFi hostname: "); Serial.println(hostNAME);Serial.print("\n");
    WiFi.hostname(hostNAME);
    ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
    ArduinoOTA.onEnd([]() { events.send("UpdaWifiStart();te End", "ota"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        char p[32];
        sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
        events.send(p, "ota");
        });
    ArduinoOTA.onError([](ota_error_t error) {
        if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
        else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
        else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
        else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    
    });
  
    ArduinoOTA.setHostname((const char *)hostNAME.c_str());
  
    ArduinoOTA.begin();

    MDNS.addService("http","tcp",80);
  
  
    //////Setto web Server
    Serial.print ("Inizializzo Web Server .........\n");
  
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    events.onConnect([](AsyncEventSourceClient *client){
        client->send("hello!",NULL,millis(),1000);
        });
    
    server.addHandler(&events);

    server.addHandler(new SPIFFSEditor(httpusername,httppassword));
  
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
        });
  
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Restart ---");
        ESP.restart();
        });
    
    server.on("/logger", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/graph.html");
        Serial.println("Save Logger ---");
        int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
        String S_filename = "/datalog/"+ String(ihour) + ".csv";
        if(SPIFFS.exists(S_filename)==1)  {
            Serial.print("    Delete older file: ");Serial.println(S_filename);
            SPIFFS.remove(S_filename);
            }
        File datalogger = SPIFFS.open(S_filename, "w");
        if (!datalogger) {
            Serial.print(S_filename);Serial.println(" open failed");
            }
        for ( int i=0;i<=ulMeasCount-1;i++){
            datalogger.print(epoch_to_string_web(pulTime[i]));datalogger.print(",");datalogger.print(pfTemp[i],1);datalogger.print(",");datalogger.print(pfHum[i],1);datalogger.print(",");datalogger.println(pfPres[i],1);
            Serial.print("Scrivo file :");Serial.print(i);Serial.print(",");Serial.print(epoch_to_string_web(pulTime[i]));Serial.print(",");Serial.print(pfTemp[i],1);Serial.print(",");Serial.print(pfHum[i],1);Serial.print(",");Serial.println(pfPres[i],1);
            }
        Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
        datalogger.close();
        byte ibuffer[32];
        S_filename = "/datalog/graph.csv";
        if(SPIFFS.exists(S_filename)==1)  {
            Serial.print("    Delete older file: ");Serial.println(S_filename);
            SPIFFS.remove(S_filename);
        }
        File f2 = SPIFFS.open(S_filename, "w");    //open destination file to write
        if (!f2) {
          Serial.println("file open" + S_filename+ " failed");    //debug
          }
        for (int i = 0; i <=23; ++i)    //creo file graph.csv
          {
            String S_filename1 = "/datalog/"+ String(i) + ".csv";
            File f1 = SPIFFS.open(S_filename1, "r");    //open source file to read
            if (!f1) {
              Serial.println("file open" + S_filename1 + " failed");  //debug
              }
              while (f1.available() > 0){
                byte i = f1.readBytes((char *)ibuffer, 32); // i = number of bytes placed in buffer from file f1
                f2.write(ibuffer, i);               // write i bytes from buffer to file f2
                }
                Serial.println("Done copying" + S_filename1);     //debug
                f1.close(); // done, close the source file
            }
         f2.close(); // done, close the destination file
         Serial.println("Creazione file graph terminata.......");
         
    });
   
    server.on("/dati", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.printf("\nGET /dati");
        int iIndex= (ulMeasCount-1)%ulNoMeasValues;
        String json = "{";
        json += "\"S_heap\":"+String(ESP.getFreeHeap());
        json += ", \"S_hostname\":\""+wifiName+"\"";
        json += ", \"S_temperature\":"+String(pfTemp[iIndex]);
        json += ", \"S_setTemp\":"+String(setTemp);
        json += ", \"S_humidity\":"+String(pfHum[iIndex]);
        json += ", \"S_pressure\":"+String(pfPres[iIndex]);
        json += ", \"S_relestatus\":\"" +stato+"\"";
        json += ", \"S_control\":"+String(chekEnable);
        json += ", \"S_time\":\"" + String(epoch_to_string(pulTime[iIndex]))+"\"";
        json += ", \"S_sensor\":"+String(sensor_on);
        json += ", \"S_version\":\"" +versione+"\"";
        json += "}";
        Serial.print("Json: \n");Serial.print(json);Serial.print("\n");
        request->send(200, "application/json", json);
        });
    
    server.on("/thermoON", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        AsyncWebParameter* p = request->getParam(i);
        int sParam = p->name().toInt();
        thermostat_on = sParam;
        thermostat_prog = 0;
        chekEnable = 0;
        setTemp = 0;
        stato="";
        Serial.print("Thermostat ON  --- ");Serial.println(sParam);
        digitalWrite ( RELEPIN , !RELE_ON);
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
    
    server.on("/releON", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Rele ON  ---");
        thermostat_on = 2;
        chekEnable = 1;
        setTemp = 0;
        digitalWrite ( RELEPIN , RELE_ON);
        stato_sistema = 5;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
    server.on("/releOFF", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Rele OFF  ---");
        thermostat_on = 2;
        chekEnable = 0;
        setTemp = 0;
        digitalWrite ( RELEPIN , !RELE_ON);
        stato_sistema = 5;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });

    server.on("/zoneOFF", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Zona OFF  ---");
        thermostat_on = 2;
        setTemp = 0;
        stato_sistema = 1;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
  
    server.on("/zoneON", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        thermostat_on = 2;
        AsyncWebParameter* p = request->getParam(i);
        setTemp = p->name().toFloat();
        Serial.print("Zona ON  ---");Serial.println(setTemp);
        Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
        stato_sistema = 1;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
  
  
    server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Clear  ---");
        chekEnable = 0;
        setTemp = 0;
        thermostat_on = 0;
        digitalWrite ( RELEPIN , !RELE_ON);
        irsend.sendRaw(Off,iRlen,38);
        stato_sistema = 0;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
    
     server.on("/aggiorna", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Update  ---");
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
        
    server.on("/irDecoder", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        thermostat_on = 2;
        AsyncWebParameter* p = request->getParam(i);
        int sParam = p->name().toInt();
        Serial.print("IrDecoder ON  ---");Serial.println(sParam);
        switch (sParam){
            case 101:
                irRead = 0;
                stato = "All OFF";
                break;
            case 102:
                irRead = 1;
                stato = "Decoder ON";
                break;
            default: 
                irRead = 0;
                stato = "All OFF";
                break;  
            }
        stato_sistema = 8;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
        });
    
    server.on("/setNetwork", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        AsyncWebParameter* p = request->getParam(i);
        Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
        String salva_network = p->name().c_str();
        String S_filena_WBS = "/rete.json";
        request->redirect("/index.html");
        Serial.println("Data saved "+S_filena_WBS+" - "+salva_network);
        File fsUploadFile = SPIFFS.open(S_filena_WBS, "w");
        if (!fsUploadFile){ 
            Serial.println("file open failed");
            }
        else{
            fsUploadFile.println(salva_network); 
            Serial.printf("BodyEnd: %s\n",salva_network.c_str());
            }
        fsUploadFile.close();
        wifi_check = 1;
        });
    server.on("/setThermostat", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        AsyncWebParameter* p = request->getParam(i);
        Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
        String salva_network = p->name().c_str();
        String S_filena_WBS = "/thermostat.json";
        request->redirect("/index.html");
        Serial.println("Data saved "+S_filena_WBS+" - "+salva_network);
        File fsUploadFile = SPIFFS.open(S_filena_WBS, "w");
        if (!fsUploadFile){ 
            Serial.println("file open failed");
            }
        else{
            fsUploadFile.println(salva_network); 
            Serial.printf("BodyEnd: %s\n",salva_network.c_str());
            }
        fsUploadFile.close();
        wifi_check = 2;
        });
  
    server.on("/irSender", HTTP_GET, [](AsyncWebServerRequest *request){
        int params = request->params();
        int i = 0;
        thermostat_on = 2;
        AsyncWebParameter* p = request->getParam(i);
        int sParam = p->name().toInt();
        Serial.print("IrSEnder invia  ---");Serial.println(sParam);
        switch (sParam){
            case 20:
                irsend.sendRaw(On20,iRlen,38);
                chekEnable = 1;
                setTemp= 20.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 21:
                irsend.sendRaw(On21,iRlen,38);
                chekEnable = 1;
                setTemp= 21.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 22:
                irsend.sendRaw(On22,iRlen,38);
                chekEnable = 1;
                setTemp= 22.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 23:
                irsend.sendRaw(On23,iRlen,38);
                chekEnable = 1;
                setTemp= 23.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 24:Serial.println("Sto ricaricando il Sistema ..........");
                irsend.sendRaw(On24,iRlen,38);
                chekEnable = 1;
                setTemp= 24.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 25:
                irsend.sendRaw(On25,iRlen,38);
                chekEnable = 1;
                setTemp= 25.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 26:
                irsend.sendRaw(On26,iRlen,38);
                chekEnable = 1;
                setTemp= 26.0;
                stato="Cool ON Est: "+String(setTemp,1);
                break;
            case 120:
                irsend.sendRaw(On120,iRlen,38);
                chekEnable = 1;
                setTemp= 20.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 121:
                irsend.sendRaw(On121,iRlen,38);
                chekEnable = 1;
                setTemp= 21.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 122:
                irsend.sendRaw(On122,iRlen,38);
                chekEnable = 1;
                setTemp= 22.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 123:
                irsend.sendRaw(On123,iRlen,38);
                chekEnable = 1;
                setTemp= 23.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 124:
                irsend.sendRaw(On124,iRlen,38);
                chekEnable = 1;
                setTemp= 24.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 125:
                irsend.sendRaw(On125,iRlen,38);
                chekEnable = 1;
                setTemp= 25.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 126:
                irsend.sendRaw(On126,iRlen,38);
                chekEnable = 1;
                setTemp= 26.0;
                stato="Cool ON Inv: "+String(setTemp,1);
                break;
            case 99:
                irsend.sendRaw(Off,iRlen,38);
                chekEnable = 0;
                stato="Cool OFF";
                break;
            default:
                break;
            }
        stato_sistema = 2;
        ulNextMeas_ms = millis();
        request->redirect("/index.html");
    });
    // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      Serial.printf("Update Start: %s\n", filename.c_str());
      Update.runAsync(true);
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        Update.printError(Serial);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

    server
        .serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=300");

  
    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.printf("NOT_FOUND: ");
        if(request->method() == HTTP_GET)
            Serial.printf("GET");
        else if(request->method() == HTTP_POST)
            Serial.printf("POST");
        else if(request->method() == HTTP_DELETE)
            Serial.printf("DELETE");
        else if(request->method() == HTTP_PUT)
            Serial.printf("PUT");
        else if(request->method() == HTTP_PATCH)
            Serial.printf("PATCH");
        else if(request->method() == HTTP_HEAD)
            Serial.printf("HEAD");
        else if(request->method() == HTTP_OPTIONS)
            Serial.printf("OPTIONS");
        else
            Serial.printf("UNKNOWN");
        Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
        Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
        Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
        }

    int headers = request->headers();
    int i;
        for(i=0;i<headers;i++){
        AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    Serial.print("Parametri");
    for(i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isFile()){
            Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if(p->isPost()){
            Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        } else {
            Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }

    request->send(404);
    });
    server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
        Serial.print("On File Upload");
        if(!index)
            Serial.printf("UploadStart: %s\n", filename.c_str());
            Serial.printf("%s", (const char*)data);
        if(final)
            Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
        });
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        Serial.print("On request Body ");
        if(!index)
            Serial.printf("BodyStart: %u\n", total);
            Serial.printf("%s", (const char*)data);
        if(index + len == total)
            Serial.printf("BodyEnd: %u\n", total);
        });
  
 
    
  server.begin();

}
