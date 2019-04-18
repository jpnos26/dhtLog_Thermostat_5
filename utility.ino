#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <SPIFFSEditor.h>

void WiFiStart(){ 
        //WiFi.mode(WIFI_OFF);
        //WiFi.disconnect(true);
        //WiFi.softAPdisconnect(false);
        //yield();
        //delay(100);
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);
        Serial.print("Connessione con :"); Serial.print(wifissid);Serial.print(" - ");Serial.print(wifipassword);Serial.print("\n");
        WiFi.mode(WIFI_STA);
        if (dhcp == 0){
            WiFi.config(ip,gateway,netmask,gateway);
          }
        WiFi.begin(wifissid, wifipassword);
        WiFi.printDiag(Serial);
        for ( int count = 0; count < 20 ; count++ )  {
            Serial.print( count ); Serial.print( "." );Serial.print( WiFi.status() );Serial.print("-");
            if (WiFi.status()  == WL_CONNECTED ) {
              break;
            }
            delay(500);
        }
        /*if (WiFi.waitForConnectResult()  == WL_CONNECTED) {
            Serial.println("");
            Serial.print("WiFi connessa --> ");Serial.println(WiFi.localIP());
            ulNextWifiCheck = millis()+500000;
            }*/
        if (WiFi.status()  == WL_CONNECTED ) {
              ulNextWifiCheck = millis()+30000;
              Serial.println("Connection ready.........");
            }
        else{
           Serial.printf("\r\nWiFi connect aborted !\r\n");
           delay(50);
           WiFi.mode(WIFI_AP);
           WiFi.hostname(wifiName);
           WiFi.softAP(httpusername, httppassword);
           delay(150);
           Serial.print("Access Point : ");Serial.print(wifiName);Serial.print(" - address ");Serial.println(WiFi.softAPIP());
           check_wifi +=1;
           if (check_wifi >= 3){
                delay(100);
                Serial.println("Tentativi reconnect esuriti REBOOT");
                ESP.restart();
                }
           ulNextWifiCheck = millis()+300000;
        }
       ulNextMeas_ms = millis();
       if (wifi_change == 1){
        ws.enable(true);
        wifi_change = 0; 
       }
}


void ricarica_sistema(){
        Serial.println("Sto ricaricando il Sistema ..........");
        wifi_change = 1;
        carica_setting();
        delay(100);
        carica_thermostat;
        WiFi.mode(WIFI_OFF);
        ulNextWifiCheck = millis()+5000;
        wifi_check = 0;
        delay(1000);
        if ( ws.enabled() )
          ws.enable(false);
        
}

void carica_setting(){
 /// carichiamo se esiste il file di configurazione
    File  carica_setting = SPIFFS.open("/rete.json", "r");
    if (!carica_setting) {
      Serial.println(" ");
      Serial.println("Non riesco a leggere settings.json! ...");
      }
    else{
        String risultato = carica_setting.readStringUntil('\n');
        const size_t bufferSize = 3*JSON_ARRAY_SIZE(1) + 7*JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(3) + 2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + 350;
        char json[bufferSize];
        risultato.toCharArray(json, bufferSize);
        Serial.print("Array json convertito: ");Serial.println(json);Serial.print("Num char: ");Serial.println(bufferSize);
        DynamicJsonBuffer jsonBuffer(bufferSize);
        //StaticJsonBuffer<650> jsonBuffer_inlettura;
    JsonObject& root = jsonBuffer.parseObject(json);
        if (!root.success()) {
            Serial.println("parseObject() failed");
            }
    else{
            JsonObject& rete0 = root["rete"][0];
            wifissid = rete0["ssid"].as<const char*>();
            wifipassword = rete0["pwd"].as<const char*>();
            dhcp = rete0["dhcp"].as<int>();
            JsonArray& rete0_ip = rete0["ip"];
            String test= rete0_ip[0].as<String>()+"."+rete0_ip[1].as<String>()+"."+rete0_ip[2].as<String>()+"."+rete0_ip[3].as<String>();
            //Serial.print(test);Serial.print(" - ");Serial.println(static_ip);
            test.toCharArray(static_ip, 16);
            JsonArray& rete0_gateway = rete0["gateway"];
            test= rete0_gateway[0].as<String>()+"."+rete0_gateway[1].as<String>()+"."+rete0_gateway[2].as<String>()+"."+rete0_gateway[3].as<String>();
            //Serial.print(test);Serial.print(" - ");Serial.println(static_gateway);
            test.toCharArray(static_gateway, 16);
            JsonArray& rete0_netmask = rete0["netmask"];
            test= rete0_netmask[0].as<String>()+"."+rete0_netmask[1].as<String>()+"."+rete0_netmask[2].as<String>()+"."+rete0_netmask[3].as<String>();
            //Serial.print(test);Serial.print(" - ");Serial.println(static_netmask);
            test.toCharArray(static_netmask, 16);
            //IPAddress netmask(rete0_netmask[0],rete0_netmask[1],rete0_netmask[2],rete0_netmask[3]);
            JsonObject& ap0 = root["ap"][0];
            wifiName = ap0["hostname"].as<String>();
            httpusername = ap0["http_username"].as<const char*>();
            httppassword = ap0["http_password"].as<const char*>();
            }
        carica_setting.close();
        //set static ip
        ip.fromString(static_ip);
        gateway.fromString(static_gateway);
        netmask.fromString(static_netmask); 
        Serial.print("Dhcp abilitato: ");Serial.println(dhcp);
        Serial.print("Letto  Ip Fisso: ");Serial.print(ip);Serial.print(" - ");Serial.print(gateway);Serial.print(" - ");Serial.println(netmask);
        Serial.print("Dati Connessione: ");Serial.print( wifissid);Serial.print(" - ");Serial.println(wifipassword);
        delay(100);
        }
}

void carica_thermostat(){
 /// carichiamo se esiste il file di configurazione
    File  carica_setting = SPIFFS.open("/thermostat.json", "r");
    if (!carica_setting) {
      Serial.println(" ");
      Serial.println("Non riesco a leggere thermostat.json! ...");
      }
    else{
        String risultato = carica_setting.readStringUntil('\n');
        const size_t bufferSize = 3*JSON_ARRAY_SIZE(1) + 7*JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(3) + 2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + 350;
        char json[bufferSize];
        risultato.toCharArray(json, bufferSize);
        Serial.print("Array json convertito: ");Serial.println(json);Serial.print("Num char: ");Serial.println(bufferSize);
        DynamicJsonBuffer jsonBuffer(bufferSize);
        //StaticJsonBuffer<650> jsonBuffer_inlettura;
    JsonObject& root = jsonBuffer.parseObject(json);
        if (!root.success()) {
            Serial.println("parseObject() failed");
            }
    else{
            JsonObject& thermo0 = root["thermo"][0];
            JsonArray& thermo0_ip = thermo0["ip"];
            thermostat_ip = thermo0_ip[0].as<String>()+"."+thermo0_ip[1].as<String>()+"."+thermo0_ip[2].as<String>()+"."+thermo0_ip[3].as<String>();
            thermostat_on = thermo0["on"].as<int>();
            thermostat_type = thermo0["type"].as<int>();
            temp_correct = thermo0["correct"].as<float>();
            ////Serial.print(wifiName);Serial.print(httpusername);Serial.print(httppassword);
            ////Serial.print(ip);Serial.print(" - ");Serial.print(subnet);Serial.println("  - Dati accesso ap");
            }
        carica_setting.close();
        Serial.print("Configurazione Thermostat - Stato : ");Serial.print(thermostat_on);Serial.print(" type: ");Serial.print(thermostat_type);Serial.print(" ip: ");Serial.println(thermostat_ip);
        delay(100);
        }
}
