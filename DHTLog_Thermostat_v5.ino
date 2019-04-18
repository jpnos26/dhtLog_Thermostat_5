/*---------------------------------------------------
HTTP 1.1 Temperature & Humidity Webserver for ESP8266 
and ext value for Raspberry Thermostat

by Jpnos 2017  - free for everyone

Tabella Connessioni

NodeMCU D0  GPIO16  NON UTILIZZATO
NodeMCU D1  GPIO05  SCL   SCL Display1306 - SCL BME280
NodeMCU D2  GPIO04  SDA   SDA Display1306 - SCL BME280
NodeMCU D3  GPIO00  NON UTILIZZATO
NodeMCU D4  GPIO02  DHT22 Data
NodeMCU D5  GPIO14  Rele
NodeMCU D6  GPIO12  Ir Sender
NodeMCU D7  GPIO13  Ir Decoder
NodeMCU D8  GPIO15  NON UTILIZZATO

+3.3v       Vin Display1306
+5V         Vin DHT22 - BME280
GND         GND DHT22 - BME280

Changelog
versione 5.1.3 13/11/2018 10.30
- aggiornate librerie Adafruit ssd1306
- tutte lebrerie aggiornate
-ATTENZIONE LIBRERIA JSON deve rimanere a versione 5.13.2 NON AGGIORNARE A ver 6
- aggiunta pagina update nascosta per upload firmware..... si raggiunge con http://IP/update
  file da caricare da dir /temp dove compila arduino estensione file .bin

versione 5.1.2 15/09/18 12.48
-Aggiunto set scostamento temperatura modificabile
-Rimossa media 
-pulita selezione sensore di temperatura
-pulizia codice e affinato web server
-semplificato uso rele con bool
versione 5.1.1 15/09/18 12.48
-Quando si entra in config dal web cambia tutti i parametri al volo
-Se non trova il wifi, attiva la modalita Acess Point
-Posibile utilizzo sensore BME280 o DHT21/AMS2301
-Aggiunto utilizzo libreria "ESP8266HTTPClient" 

versione 5.0.2 10/06/18 12.48
-Introdotto gestione OffSet Temperatura

versione 5.0.1 19/05/18 09.52
-Modificato principio di comunicazione tra DL e Thermostat 5
-Introdotta funzione di access point
-Introdotta pagina di configurazione network e parametri vs Thermostat 5

versione 4.0.1 29/04/18 13.35
-Modificato index.css con drop menu colori blu/verde e introduzione nuovi gauge

---------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include "time_ntp.h"
#include <DHT.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include "RecIR.h"
#include "SendIR.h"
#include "ssd1306.h"
#include "bme280.h"
#include <ArduinoJson.h>
#include <SPIFFSEditor.h>
#include <ESP8266HTTPClient.h>
#include <Arduino.h>




////////////// START CONFIGURAZIONE ///////////


// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

///Wifi Configuration
const char* wifissid = "Wifi SSId";
const char* wifipassword = "Wifi pwd";
String wifiName = "DhtLogv5";
const char* httpusername = "admin";
const char* httppassword = "admin";
unsigned long ulNextWifiCheck;      // siamo in ap check wifi
int dhcp = 0; ////0 dhcp disable, 1 dhcp enabled
char static_ip[16] = "192.168.1.160";
char static_gateway[16] = "192.168.1.1";
char static_netmask[16] = "255.255.255.0";
IPAddress ip;
IPAddress gateway;
IPAddress netmask;
IPAddress dns(8,8,8,8);
//IPAddress ip(192,168,1,123);
//IPAddress gateway(192,168,1,1);
//IPAddress subnet(255,255,255,0);
//IPAddress dns(8,8,8,8);

int check_wifi; /// Controllo del sistema se in ap per 3 volte resetta il sistema
String versione = "5.0.1 12/11/2018";

// needed to avoid link error on ram check
extern "C" 
{
#include "user_interface.h"
}

//enable screen !!!Attenzione se non si ha lo schermo mettere a zero Qui tutte le abilitazioni
bool screen_on = 1;     //abilito la visualizzazione
bool irread_on = 1;     //abilito ricezione ir
bool irsend_on = 1;     //abilito trasmissione ir
int  sensor_on = 2;     // abilito lettura  sensore temperatura 0 = no sensore, 1 = dht22, 2 = bme280
bool time_on   = 1;     //abilito accesso all'orario se usato solo come periferica di Thermostat e non si ha accesso a internet mettere a zero.
int wifi_check = 0;     // variabile check connessione wifi
byte ntp_check = 0;     // byte check validita orario

bool RELE_ON = 0;       // selezionare il tipo di on se optoisolato = 0 se diretto = 1;
//int RELE_OFF= 1;       // selezionare il contrario di RELE_ON

byte dht22_pin = 0  ;      // Selezionare il pin dov e collegato il dht 22
float temp_correct = 0;   // correzione da applicare alla temperatura
byte wifi_change = 0;     // check per ricaricare variabili

/////////////ds18b20/////////////////////////

///// Data for Thermostat 5 //////
String thermostat_ip = "192.168.1.101"; /// ip del Thermostat da raggiungere for slave mode
byte thermostat_on = 1;                 /// uso o meno di master thermostat 0 = si passivo; 1 = si  attivo ; 2 = no
byte thermostat_prog = 0;                /// variabile uso di thermostat
byte thermostat_type = 2;                ///Tipo di DHT 0 = temp ext ; 1 = Rele ; 2 = Zona ; 3 = Ir  ;  7 = thermovalvola
char ip_slave[14];

// storage for Measurements; keep some mem free; allocate remainder

unsigned long ulMeasCount=0;          // values already measured
unsigned long ulNoMeasValues=0;       // size of array
unsigned long ulMeasDelta_ms;         // distance to next meas time
unsigned long ulNextMeas_ms;          // next meas time
unsigned long ulNextIr_ms= 900000;    // tempo inattivita IR tra due comandi in ms 900000 = 15 minuti
unsigned long ulNextIr;               // prossimo check IR
unsigned long *pulTime;               // array for time points of measurements
float *pfTemp,*pfHum,*pfPres;         // array for temperature and humidity measurements
bool irRead;                           // enable read ir data
bool chekEnable = 0;                   // comando ricevuto
int RELEPIN = 14;                     // pin per RELE
float tempHist=0.2;                   // temp histeresis
float dhtTemp;                        // dht read temperature
float dhtUm;	                        // dht read umidita
float dhtPres;                        // dht pressione
float setTemp;                        // set Temp to check
unsigned long ulNextntp;              // next ntp check
unsigned long timezoneRead();         // read time form internet db
String stato= "All OFF";              //stato sistema
int ihour_prev;                       // ora precedente
int stato_sistema = 0;                ///Nel caso di no thermostat slave setta le funzioni 0 = temp ext ; 1 = Zona ; 2 = IR ; 3 = Stanza Fredda solo Dati ; 4 = Zona + Stanza Fredda ; 5 = rele; 6 = rele +tempext;

decode_results results; // Somewhere to store the results


//////////////////////////////
// DHT21 / AMS2301 pin
//////////////////////////////
#define DHTPIN dht22_pin

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// init DHT; 3rd parameter = 16 works for ESP8266@80MHz
DHT dht(DHTPIN, DHTTYPE,16);


  

/////////////////////
// the setup routine
/////////////////////
void setup(){
    ////Init Serial
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.print("\n***************************************\n");
    Serial.println("Esp8266 WI-FI DhtLog Jpnos-2018 v5.1.1");
    
    //SETUP Screen
    if( screen_on ==1){
        initDisplay();
        }
        
    // inizializzo sensore temperatura
    if (sensor_on==2){
        init_bme280();
    }
    if (sensor_on ==1){
      dht.begin();
    }
    
    // setup globals
    irRead=0;
    
    //setup IR
    Serial.println("Setup IR");
    irrecv.setUnknownThreshold(MIN_UNKNOWN_SIZE);
    irrecv.enableIRIn();  // Start the receiver  
    irsend.begin();  //start the sender
  
    //Setup Pin OUT
    Serial.println("Inizializzo PinOut");
    pinMode(RELEPIN,OUTPUT);
    pinMode(16,OUTPUT);
    digitalWrite(RELEPIN,!RELE_ON);
    digitalWrite(16,1);

    
   
    //////// Inizializzo tabella per dati 
    Serial.println("Inizializzo Memoria");
    ulNoMeasValues = 240;//free / (sizeof(float)*2+sizeof(unsigned long));  // humidity & temp --> 2 + time
    pulTime = new unsigned long[ulNoMeasValues];
    pfTemp = new float[ulNoMeasValues];
    pfHum = new float[ulNoMeasValues];
    pfPres = new float[ulNoMeasValues];
    if (pulTime==NULL || pfTemp==NULL || pfHum==NULL){
        ulNoMeasValues=0;
        Serial.println("Errore in allocazione di memoria!");
        }
    else{
        Serial.print("Memoria Preparata per ");
        Serial.print(ulNoMeasValues);
        Serial.println(" data points.");
        float fMeasDelta_sec = 3600/ulNoMeasValues;///MEAS_SPAN_H*3600./ulNoMeasValues;
        ulMeasDelta_ms = ( (unsigned long)(fMeasDelta_sec) ) * 1000;  // round to full sec
        Serial.print("La misura avviene ogni");
        Serial.print(ulMeasDelta_ms);
        Serial.println(" ms.");
        ulNextMeas_ms = millis()+ulMeasDelta_ms;
        }
  
    
    delay(100);
     
    ///inizializzo spiffs
    Serial.print ("Inizializzo SPIFFS .........\n");
    SPIFFS.begin();
    delay(100);
    
    

    carica_setting();
    carica_thermostat();
    delay(100);
    
    
    ////Inizializzo Wifi
    Serial.print ("Inizializzo Wifi .........\n");
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFiStart(); ////Start Wifi
    ///Inizializzo ntp
    if (time_on == 1){
        ntpacquire();
        }
    delay(100);
    
    //Send OTA events to the browser
    setup_Server();
  
    //////Set ihour_prev
    ihour_prev = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
}

///////////////////////////////////////////////////////
///routine di uso sistema  ////////////////////////////
///////////////////////////////////////////////////////




void ntpacquire() {
 
    ///connect to NTP and get time

    if ( screen_on == 1){
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0,20);
        display.print("Load Time");
        display.display();
        }
    /*delay(50);
    ulSecs2000_timer=getNTPTimestamp();
    Serial.print("Ora Corrente da server NTP : " );
    Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
    ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
    ulNextntp=millis()+3600000;
    delay(1000);*/
    /////////////////////
    //timezoneRead()
    /////////////////////
    unsigned long timeInternet=timezoneRead();
    if (!timeInternet == 0){
        ulSecs2000_timer=timeInternet;
        Serial.print("Ora Corrente da server Internet: " );
        Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
        ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
        ulNextntp=millis()+3600000;
        ntp_check = 0;
        }
    else{
        ntp_check += 1;
        ulNextntp=millis()+10000;
    }
    if (ihour_prev ==0){
        ihour_prev = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
        }
    if ( screen_on==1){
        display.clearDisplay();
        }
}

void DecodingIr(){
  decode_results  results;        // Somewhere to store the resultsconst char*
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf("Timestamp : %06u.%03u\n", now / 1000, now % 1000);
    if (results.overflow){
      Serial.printf("WARNING: IR code is toint gethour();o big for buffer (>= %d). "
                    "This result shouldn't be trusted until this is resolved. "
                    "Edit & increase CAPTURE_BUFFER_SIZE.\n",
                    CAPTURE_BUFFER_SIZE);
    }
    // Display the library version the message was captured with.
    Serial.print("Library   : v");
    Serial.println(_IRREMOTEESP8266_VERSION_);
    Serial.println();

    // Output RAW timing info of the result.
    Serial.println(resultToTimingInfo(&results));
    yield();  // Feed the WDT (again)

    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println("");  // Blank line between entries
    yield();  // Feed the WDT (again)
  }
}
void save_logger() {
    int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
    if (ihour == 0){
        ihour = 24;
        }
    String S_filename = "/datalog/"+ String(ihour-1) + ".csv";
    Serial.print("ihour. ");Serial.println(ihour);
    Serial.print("ihour_prev. ");Serial.println(ihour_prev);
  
    if(SPIFFS.exists(S_filename)==1)  {
      Serial.print("    Delete older file: ");Serial.println(S_filename);
      SPIFFS.remove(S_filename);
        }
    File datalogger = SPIFFS.open(S_filename, "a");
    if (!datalogger) {
        Serial.print(S_filename);Serial.println(" open failed");
        }
    for ( int i=0;i<=ulMeasCount-1;i++){
        datalogger.print(epoch_to_string_web(pulTime[i]));datalogger.print(",");datalogger.print(pfTemp[i],1);datalogger.print(",");datalogger.print(pfHum[i],1);datalogger.print(",");datalogger.println(pfPres[i],1);
        Serial.print("Scrivo file :");Serial.print(i);Serial.print(",");Serial.print(epoch_to_string_web(pulTime[i]));Serial.print(",");Serial.print(pfTemp[i],1);Serial.print(",");Serial.print(pfHum[i],1);Serial.print(",");Serial.println(pfPres[i],1);
        }
    Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
    datalogger.close();
}

void ReadDht(){
    if (sensor_on == 1){
      dhtTemp = dht.readTemperature();
      dhtUm = dht.readHumidity();
      dhtPres = 0;
    }
    if (sensor_on == 2){
      dhtTemp = bme.readTemperature();
      dhtPres= bme.readPressure() / 100.0F;
      dhtUm = bme.readHumidity();
      
      Serial.print(dhtTemp);Serial.print(" - ");Serial.print(dhtUm);Serial.print(" - ");Serial.println(dhtPres);
    }
    if(sensor_on > 0){
      if (isnan(dhtTemp) || isnan(dhtUm)) 
      {
          Serial.println("Failed to read from sensor!"); 
      }   
      else
      {
        
          pfHum[ulMeasCount%ulNoMeasValues] = dhtUm;
          pfTemp[ulMeasCount%ulNoMeasValues] = dhtTemp + temp_correct;
          pfPres[ulMeasCount%ulNoMeasValues] = dhtPres;
          pulTime[ulMeasCount%ulNoMeasValues] = millis()/1000+ulSecs2000_timer;
          Serial.print("Logging lettura: ");Serial.print(ulMeasCount%ulNoMeasValues);Serial.print(" - Temperature: ");
          Serial.print(pfTemp[ulMeasCount%ulNoMeasValues]);
          Serial.print(" deg Celsius - Humidity: "); 
          Serial.print(pfHum[ulMeasCount%ulNoMeasValues]);
          Serial.print("% - Pressure: ");
          Serial.print(pfPres[ulMeasCount%ulNoMeasValues]);
          Serial.print("% - Time: ");
          Serial.println(pulTime[ulMeasCount%ulNoMeasValues]);
          ulMeasCount++;
          int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
          if(ihour != ihour_prev) {
            save_logger();
            ihour_prev = ihour;
            ulMeasCount=0;
        }
      }
    }
}


void DisplayText() {
    if ( screen_on == 1){
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0,5);
        display.print(epoch_to_date(millis()/1000+ulSecs2000_timer));
        display.setTextSize(2);
        display.setCursor(65,0);
        display.print(epoch_to_time(millis()/1000+ulSecs2000_timer));
        display.setCursor(0,27);
        display.print(pfTemp[(ulMeasCount-1)%ulNoMeasValues],1);
        display.print((char)247);
        display.setCursor(66,27);
        display.print(pfHum[(ulMeasCount-1)%ulNoMeasValues],1);
        display.print("%");
        display.setTextSize(1);
        display.setCursor(0,53);
        display.print(stato);
        display.display();
        }
}
void  writeWebData(){
    Serial.print("\nscrivo dati su Thermostat: " );Serial.println(thermostat_ip); 
    HTTPClient http;
    // We now create a URI for the request
    String url = "http://"+thermostat_ip+":9090/?"+String(ESP.getChipId(), HEX)+"&"+wifiName+"&"+WiFi.localIP().toString()+"&"+String(pfTemp[(ulMeasCount-1)%ulNoMeasValues],1)+"&"+String(dhtUm,0)+"&"+dhtPres+"&"+String(setTemp,1)+"&"+thermostat_type+"&"+chekEnable+"&"+String(thermostat_on)+"&DHT";
    Serial.print("Requesting URL: ");Serial.println(url);

    
    // Read all the lines of the reply from server and print them to Serial
   
    http.begin(url);
      http.addHeader("Content-Type", "application/json");
      String json;
      int httpCode = http.GET();
      if (httpCode == 200) {
          json = http.getString();
          Serial.println(json);
        }
        else{
          Serial.println(httpCode);
        }
    http.end();
    const size_t bufferSize = JSON_OBJECT_SIZE(13) + 100;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    Serial.print("Got data:");Serial.println(json);
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()){
        Serial.println("parseObject() failed");
        return;
        }
    if (root["programma"] == 1){
        tempHist = 0.2;
        if (root["stato"] == 1){  
            setTemp = root["setta"];
            }
        else if(root["stato"]== 2){
          setTemp = root["manTemp"];
            }
        else if(root["stato"] == 3){
          setTemp = root["noIce"];
            }
        else {
          setTemp = 0;
            }
        }
    else if (root["programma"] == 2){
        tempHist = 1;
        if (root["stato"] == 1){  
            setTemp = int(root["setta"]);
            }
        else if(root["stato"]== 2){
          setTemp = int(root["manTemp"]);
            }
        else {
          setTemp = 99;
            }
      }
      
      thermostat_prog = root["programma"];
      chekEnable = root["caldaia"];

      Serial.print ("setTemp : ");Serial.print (setTemp);Serial.print(" Programma: ");Serial.println(thermostat_prog);
      Serial.print ("Heap Memory: ");Serial.println(String(ESP.getFreeHeap()));
        Serial.println("closing connection");  
    
}

/////////////
// main loop
/////////////
void loop() {

    // DO NOT REMOVE. Attend OTA update from Arduino IDE
    ArduinoOTA.handle();
   
    ///////////Aggiorno display
    if ( screen_on == 1){
        DisplayText();
        delay(5);// The repeating section of the code
        }
    //
    //////////////////////////////
    // check if WLAN is connected
    //////////////////////////////
    if (millis() >= ulNextWifiCheck){
        if (WiFi.status() != WL_CONNECTED){
            WiFiStart();
            }
        }
        
    if (wifi_check == 1){
      
        ricarica_sistema();
        }
    if (wifi_check == 2){
      
        carica_thermostat();
        wifi_check = 0;
        ulNextMeas_ms = millis();
        }
    ///////////////////
    // do data logging
    ///////////////////

    if (millis()>=ulNextMeas_ms) {
        ReadDht();
        ulNextMeas_ms = millis()+ulMeasDelta_ms;
        //Serial.print(String(setTemp)+" - "+String(tempHist)+" - "+stato);
        if (thermostat_on == 0 || thermostat_on ==1){
            writeWebData();
            }
        }

    if(thermostat_on == 0 || thermostat_on == 1){
        switch (thermostat_prog){
                case 0:
                    stato ="Thermo ALL OFF"; 
                    break;
                case 1:
                    if (thermostat_type == 2){
                       if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist){
                          //Serial.println("Zona - Rele ON\n");
                          digitalWrite ( RELEPIN,RELE_ON);
                          chekEnable = 1;
                          if (thermostat_on == 0){
                              stato= "Zona Att. ON:"+String(setTemp); 
                            }
                            else{
                              stato= "Zona Pass. ON:"+String(setTemp); 
                            }
                        }
                        else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues]){
                          //Serial.println("Zona - Rele OFF \n");
                          digitalWrite ( RELEPIN , !RELE_ON) ;
                          chekEnable = 0;
                          if (thermostat_on == 0){
                              stato= "Zona Att. OFF:"+String(setTemp); 
                            }
                            else{
                              stato= "Zona Pass. OFF:"+String(setTemp); 
                            }
                 
                          }
                    }
                         /*
                    else if (thermostat_type == 1 || thermostat_type == 4){  
                        if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist){
                            //Serial.println("Zona - Rele ON\n");
                            digitalWrite ( RELEPIN,RELE_ON);
                            stato="Thermo Zona ON: "+String(setTemp,1);
                            chekEnable = 1;
                        }
                        else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues]){
                            //Serial.println("Zona - Rele OFF \n");
                            digitalWrite ( RELEPIN , RELE_OFF) ;
                            stato="Thermo Zona OFF: "+String(setTemp,1);
                            chekEnable = 0;
                            }
                    }*/
                    else if (thermostat_type == 0 ){
                        stato="Thermo Ext ";
                        }
                    if (thermostat_type == 1){
                        if(chekEnable == 1){
                            //Serial.println("Rele ON\n");
                            digitalWrite ( RELEPIN,RELE_ON);
                            stato= "Remote Rele ON: "; 
                        }
                        else{
                            //Serial.println("Rele OFF\n");
                            digitalWrite ( RELEPIN,!RELE_ON);
                            stato= "Remote Rele OFF: ";  
                        }
                    }
                    else if (thermostat_type == 3 ){
                        if (millis() >= ulNextIr){
                            if (setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist){
                                switch (int(setTemp)){
                                    case 20:
                                        irsend.sendRaw(On120,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 20.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 21:
                                        irsend.sendRaw(On121,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 21.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 22:
                                        irsend.sendRaw(On122,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 22.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 23:
                                        irsend.sendRaw(On123,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 23.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 24:
                                        irsend.sendRaw(On124,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 24.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 25:
                                        irsend.sendRaw(On125,iRlen,38);
                                        chekEnable = 1;
                                        setTemp= 25.0;
                                        stato="Cool ON Inv: "+String(setTemp,1);
                                        break;
                                    case 26:
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
                                }
                            else if ( setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]){
                                //Serial.println("Zona - Rele OFF \n");
                                irsend.sendRaw(Off,iRlen,38);
                                chekEnable = 0;
                                stato="Cool OFF";
                            }
                        ulNextIr = millis()+ulNextIr_ms;
                        }
                    }
                    break;
                case 2:
                    if (thermostat_type == 2 ){
                        if (millis() >= ulNextIr){
                            if (setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist){
                                switch (int(setTemp)){
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
                                    case 24:
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
                                    case 99:
                                        irsend.sendRaw(Off,iRlen,38);
                                        chekEnable = 0;
                                        stato="Cool OFF";
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            else if ( setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]){
                                //Serial.println("Zona - Rele OFF \n");
                                irsend.sendRaw(Off,iRlen,38);
                                chekEnable = 0;
                                stato="Cool OFF";
                            }
                        ulNextIr = millis()+ulNextIr_ms;
                        }
                    }
                    break;
            }
    }
    else {
        switch (stato_sistema){
            case 0:
                stato = "Temp esterna";
                break;
            case 1:
                if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist){
                    //Serial.println("Zona - Rele ON\n");
                    digitalWrite ( RELEPIN,RELE_ON);
                    stato="Zona ON: "+String(setTemp,1);
                    chekEnable = 0;
                    }
                else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues]){
                    //Serial.println("Zona - Rele OFF \n");
                    digitalWrite ( RELEPIN , !RELE_ON) ;
                    stato="Zona OFF: "+String(setTemp,1);
                    chekEnable = 1;
                    }
                break;
            case 2:
                break;
            case 3:
                stato = "Stanza Fredda";
                break;
            case 4:
                if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist)
                {
                    //Serial.println("Zona Fredda - Rele ON\n");
                    digitalWrite ( RELEPIN,RELE_ON);
                    stato="Zona Fredda ON: "+String(setTemp,1);
                    chekEnable = 0;
                    }
                else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues])
                {
                    //Serial.println("Zona Fredda - Rele OFF \n");
                    digitalWrite ( RELEPIN , !RELE_ON) ;
                    stato="Zona Fredda OFF: "+String(setTemp,1);
                    chekEnable = 1;
                    }
                break;
            case 5:
                if (chekEnable == 0){
                    stato = "Rele Caldaia OFF";
                    }
                else{
                    stato = "Rele Caldaia ON";
                    }
                break;
            case 6:
                if (chekEnable == 0){
                    stato = "Rele Caldaia OFF";
                    }
                else{
                    stato = "Rele Caldaia ON";
                    }
                break;
            case 7:
                break;  
            }
        }
    if (irRead == 1){
        if (irread_on ==1 ){
        DecodingIr();
        }
    }
    
    ///////////////////////////
    ///check NTP
    //////////////////////////
    if(time_on==1){
      if (ntp_check >= 5){
        wifi_check == 1;
      }
      else{
        if (millis()>=ulNextntp){
          ntpacquire();
          }
      }  
    }
}
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}
