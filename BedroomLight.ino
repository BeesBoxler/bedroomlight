#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <elapsedMillis.h>
enum wifiMode {AP, NORMAL};

char ssid[50]; // = "BOXLERNETx"; 
char passwd[50]; // = "honeybear";

int currentWifiMode = NORMAL;

elapsedMillis timer;

ESP8266WebServer server(80);

int ledLength = 2;
int ledPin = 14;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(ledLength, ledPin);

void setup() {

  pixels.begin();
  pinMode(16,OUTPUT);
  
  EEPROM.begin(512);
  Serial.begin(115200);
  Serial.println((char*)ssid);
  readFromEEPROM();
  Serial.println((char*)ssid);
  connectWifi();
  setupServer();

  delay(500);
}

void readFromEEPROM() {

  Serial.println();
  Serial.println("Loading");
  
  int eepromCounter = 0;
  int ssidLength = EEPROM.read(eepromCounter);
  delay(500);
  eepromCounter++;
  String ssidFromMem;
  for(int i=0;i<ssidLength;i++){
    ssid[i] = (char)EEPROM.read(eepromCounter);
    eepromCounter++;
    delay(500);
  }
  int passwdLength = EEPROM.read(eepromCounter);
  delay(500);
  eepromCounter++;
  String passwdFromMem;
  for(int i=0;i<passwdLength;i++){
    passwd[i] = (char)EEPROM.read(eepromCounter);
    eepromCounter++;
    delay(500);
  }

  Serial.println((char*)ssid); 
  Serial.println((char*)passwd);
  delay(500);
}

void setupServer() {
  server.on("/", [](){
    String s = "<!DOCTYPE HTML>\r\n<html>\r\n";
    s += "<h1>Please enter wifi network details below</h1>";
    s += "<form action='http://192.168.4.1/submit' method='POST'>";
    s += "<label for='ssid' value='SSID:'>";
    s += "<input name='ssid'/><br/>";
    s += "<label for='passwd' value='Password:'>";
    s += "<input name='passwd'/><br/>";
    s += "<input type='submit'>";
    s += "</form>";
    s += "</html>";
    server.send(200, "text/html", s);
  });
  
  server.on("/submit", [](){
    if (server.args() > 0 ) {
      for ( uint8_t i = 0; i < server.args(); i++ ) {
        Serial.print(server.argName(i));
        Serial.print(": ");
        Serial.println(server.arg(i));
      }
    }

    for(int i=0;i<50;i++){ssid[i] = 0x00;};
    for(int i=0;i<50;i++){passwd[i] = 0x00;};
    
    const char* newSSID = server.arg("ssid").c_str();
    const char* newPasswd = server.arg("passwd").c_str();

    for(int i=0;i<strlen(newSSID);i++){ssid[i]=(char)newSSID[i];};
    for(int i=0;i<strlen(newPasswd);i++){passwd[i]=(char)newPasswd[i];};
    
    int ssidLength = strlen(server.arg("ssid").c_str());
    int passwdLength = strlen(server.arg("passwd").c_str());
    Serial.println(ssidLength);
    Serial.println(passwdLength);
    int eepromCounter = 0;
    EEPROM.write(eepromCounter,ssidLength);
    eepromCounter++;
    for (int i=0;i<ssidLength;i++){
      EEPROM.write(eepromCounter,server.arg("ssid").c_str()[i]);
      delay(100);
      eepromCounter++;
    }
    EEPROM.write(eepromCounter,passwdLength);
    delay(100);
    eepromCounter++;
    for (int i=0;i<passwdLength;i++){
      EEPROM.write(eepromCounter,server.arg("passwd").c_str()[i]);
      delay(100);
      eepromCounter++;
    }
    EEPROM.commit();

    delay(500);
    
    server.send(200, "text/html", "<h1>Thank you, restarting...</h1>");
    delay(1000);
//    resetDevice();
    ESP.restart();
  });
  server.begin();
}

void loop() {
  server.handleClient();
  if (currentWifiMode == AP) {
    pixels.setPixelColor(0,0,0,100);
    pixels.show();
    delay(200);
    pixels.setPixelColor(0,0,0,0);
    pixels.show();
    delay(200);
  }
}

bool connectWifi(){
  Serial.print("Connecting to ");
  Serial.println((char*)ssid);
  WiFi.begin(ssid,passwd);
  timer = 0;
  while (WiFi.status() != WL_CONNECTED && timer < 10000) {
      pixels.setPixelColor(0,255,0,0);
      pixels.show();
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      pixels.setPixelColor(0,0,255,0);
      pixels.show();
      return true;
    } else {
      WiFi.disconnect();
      setupAP();
      return false;
    }
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  Serial.println("Could not connectr, reverting to AP mode");
  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP("bedroomLight","password") ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  currentWifiMode = AP;
}

