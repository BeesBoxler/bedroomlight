#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <elapsedMillis.h>
#include <PubSubClient.h>

enum wifiMode {AP, NORMAL};
enum mode { MOOD, STROBE, SOLID, POLICE, WHORE, BLUE, DIM, OFF };

#define MQTT_SERVER "192.168.0.100"
char ssid[50];
char passwd[50];

int currentWifiMode = NORMAL;

bool mainLight = false;
bool switchStatus;
bool previousSwitchStatus = false;

elapsedMillis timer;

ESP8266WebServer server(80);

int relayPin = 16;
int switchPin = 5;

int ledLength = 10;
int ledPin = 14;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(ledLength, ledPin);

char* moodTopic = "bedroom/#";
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient wifiClient;
PubSubClient client(MQTT_SERVER, 1883, callback, wifiClient);

int colorMode = MOOD;

int currentColor[3] = { 0,0,0 };
int targetColor[3] = { 0,0,0 };

void setColor(int r, int g, int b) {
  targetColor[0] = r;
  targetColor[1] = g;
  targetColor[2] = b;
}

void forceColor(int r, int g, int b) {
  setColor(r, g, b);
  currentColor[0] = r;
  currentColor[1] = g;
  currentColor[2] = b;
  for (int i = 0; i <= ledLength; i++) {
    pixels.setPixelColor(i, currentColor[0], currentColor[1], currentColor[2]);
  };
  pixels.show();

}

void updateColor() {
  if (targetColor[0] > currentColor[0]) { currentColor[0]++; }; // become redder
  if (targetColor[0] < currentColor[0]) { currentColor[0]--; }; // less red
  if (targetColor[1] > currentColor[1]) { currentColor[1]++; }; // more green
  if (targetColor[1] < currentColor[1]) { currentColor[1]--; }; // less green
  if (targetColor[2] > currentColor[2]) { currentColor[2]++; }; // more blue
  if (targetColor[2] < currentColor[2]) { currentColor[2]--; }; // less blue

  for (int i = 0; i <= ledLength; i++) {
    pixels.setPixelColor(i, currentColor[0], currentColor[1], currentColor[2]);
  };
  pixels.show();
}

void setup() {

  pixels.begin();
  pinMode(relayPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);
  
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
  if (ssidLength > 50) {
    return;
  }
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
  Serial.println("end read eeprom");
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
    ESP.restart();
  });
  server.begin();
}

void lightsOut() {
	for (int i = 0; i <= ledLength; i++) {
		pixels.setPixelColor(i, 0,0,0);
		pixels.show();
	};
}

void callback(char* topic, byte* payload, unsigned int length) {

	//convert topic to string to make it easier to work with
	String topicStr = topic;

	//Print out some debugging info
	Serial.println("Callback update.");
	Serial.print("Topic: ");
	Serial.println(topicStr);

	Serial.print("Payload: ");
	Serial.println((char*)payload);

	if (topicStr == "bedroom/strobelight") {
		if (char(payload[0]) == '1') {
			colorMode = STROBE;
    }
		else{
			colorMode = OFF;
		}
	}

  if (topicStr == "bedroom/mainLight") {
    if (char(payload[0]) == '1') {
      mainLight = 1;
    }
    else {
      mainLight = 0;
    }
  }

  if (topicStr == "bedroom/whoreLight") {
    if (char(payload[0]) == '1') {
      colorMode = WHORE;
    }
    else {
      colorMode = OFF;
    }
  }

  if (topicStr == "bedroom/blueLight") {
    if (char(payload[0]) == '1') {
      colorMode = BLUE;
    }
    else {
      colorMode = OFF;
    }
  }

  if (topicStr == "bedroom/dimLight") {
    if (char(payload[0]) == '1') {
      colorMode = DIM;
    }
    else {
      colorMode = OFF;
    }
  }

	if (topicStr == "bedroom/policelight") {
		if (char(payload[0]) == '1') {
      colorMode = POLICE;
		}
		else{
      colorMode = OFF;
		}
	}

	if (topicStr == "bedroom/moodlight") {
		if (char(payload[0]) == '1') {
			colorMode = MOOD;
		}
		else{
			colorMode = OFF;
		}
	}

}

void whoreLight() {
  setColor(255, 0, 0);
}

void blueLight() {
  setColor(0, 0, 255);
}

void dimLight() {
  setColor(100, 100, 100);
}

void moodLight() {
  if (targetColor[0] != 255 && targetColor[1] != 255 && targetColor[2] != 255) {
    targetColor[0] = 255;
  };
  if (currentColor[0] == 255) { setColor(0, 255, 0); };
  if (currentColor[1] == 255) { setColor(0, 0, 255); };
  if (currentColor[2] == 255) { setColor(255, 0, 0); };
  delay(20);
}

void strobeLight() {
  forceColor(255, 255, 255);
  delay(40);
  forceColor(0, 0, 0);
  delay(20);
}

void loop() {
  connectWifi();
  client.loop();
  server.handleClient();
  if (currentWifiMode == AP) {
    pixels.setPixelColor(0,0,0,100);
    pixels.show();
    delay(200);
    pixels.setPixelColor(0,0,0,0);
    pixels.show();
    delay(200);
  }

  if (digitalRead(switchPin) == HIGH) {
    switchStatus = 1;
  }
  else {
    switchStatus = 0;
  }

  if (switchStatus != previousSwitchStatus) {
    mainLight = !mainLight;
  }

  if (mainLight) {
    digitalWrite(relayPin, HIGH);
  }
  else {
    digitalWrite(relayPin, LOW);
  }


  switch (colorMode) {
  case MOOD:
	  moodLight();
	  break;
  case STROBE:
	  strobeLight();
	  break;
  case POLICE:
	  //policeLight();
	  break;
  case SOLID:
	  break;
  case WHORE:
    whoreLight();
    break;
  case BLUE:
    blueLight();
    break;
  case DIM:
    dimLight();
    break;
  default:
    setColor(0, 0, 0);
  };

  updateColor();
  delay(20);
  previousSwitchStatus = switchStatus;
}

void connectWifi(){
  
  if (WiFi.status() != WL_CONNECTED && currentWifiMode == NORMAL) {
    Serial.print("Connecting to ");
    Serial.println((char*)ssid);
    WiFi.begin(ssid, passwd);
    pixels.setPixelColor(0, 255, 0, 0);
    pixels.show();
  }

  timer = 0;
  while (WiFi.status() != WL_CONNECTED && timer < 10000 && currentWifiMode == NORMAL) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED && currentWifiMode == NORMAL) {
    WiFi.disconnect();
    setupAP();
	return;
  }

	if (WiFi.status() == WL_CONNECTED) {
		// Loop until we're reconnected to the MQTT server
		while (!client.connected()) {
			Serial.print("Attempting MQTT connection...");

			// Generate client name based on MAC address and last 8 bits of microsecond counter
			String clientName;
			clientName += "esp8266-";
			uint8_t mac[6];
			WiFi.macAddress(mac);
			clientName += macToStr(mac);

			//if connected, subscribe to the topic(s) we want to be notified about
			if (client.connect((char*)clientName.c_str(), "esp8266", "elephant")) {
				Serial.println("\tMTQQ Connected");
				if (client.subscribe(moodTopic)) {

					Serial.println("All Channels connected");

					// WS2812b green status light 
					lightsOut();
					pixels.setPixelColor(0, 0, 255, 0);
					pixels.show();
				}
			}

			//otherwise print failed for debugging
			else { Serial.println("\tFailed."); abort(); }
		}
	}
}

String macToStr(const uint8_t* mac) {

	String result;

	for (int i = 0; i < 6; ++i) {
		result += String(mac[i], 16);

		if (i < 5) {
			result += ':';
		}
	}

	return result;
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

