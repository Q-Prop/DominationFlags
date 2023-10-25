#define LILYGO_T5_V213
#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>  
#include GxEPD_BitmapExamples
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Time.h> 
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#define EPD_CS 5
#define EPD_DC 17
#define EPD_RSET 16u
#define EPD_BUSY 4
#define EPD_SCLK 18
#define EPD_MISO -1
#define EPD_MOSI 23

#define action 2
#define plus5 14
#define minus5 13
#define startStop 15
#define battLevelPin 35
#define LEDPin 33
#define debug false

String myName;
String serverName = "http://10.10.10.1/";
bool gameOn = false;
int lastTime;
int lastDisplayUpdate;
int displayUpdateSeconds = 10;
int scoreRate = 2000;
String OldRed = "1";
String OldBlue = "1";
String OldStations = "1";
String OldTimes = "1";
String GameStatus = "Off";
long int batteryCheckTime = 1;
int batteryCheckInterval = 60;
float VOLT_CALC =528;
float batteryLevel = 9;


GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);
WiFiMulti wifiMulti;  

int loops = 0;

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.setHostname(myName.c_str());
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(action,INPUT_PULLUP);
  pinMode(plus5,INPUT_PULLUP);
  pinMode(minus5,INPUT_PULLUP);
  pinMode(startStop,INPUT_PULLUP);
  pinMode(LEDPin,OUTPUT);
  digitalWrite(LEDPin,HIGH);
  lastDisplayUpdate = millis();
  delay(1000);
  Serial.println(">");
  Serial.println("Starting WiFi");
  myName = "C-" + WiFi.macAddress().substring(15);
  wifiMulti.addAP("Flags", "nopassword1");
  wifiMulti.run();
  delay(1000);
  if (wifiMulti.run() == WL_CONNECTED) {
    setupOTA();
    Serial.print("Wifi Started, IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Starting alternative WiFi");
    wifiMulti.addAP("DNZ", "TraceyCooke");
    wifiMulti.addAP("Demitasse", "TraceyCooke");
    wifiMulti.run();
    delay(500);
    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.print("Wifi Started, IP address: ");
      Serial.println(WiFi.localIP());
      setupOTA();
    } else {
        Serial.println("WiFi Failed");
    }
  }
  Serial.println("Starting ePaper");
  SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
  display.init();
  lastTime = millis();
  Serial.println("Init complete");
  digitalWrite(LEDPin,LOW);
}

String GetHttpInfo (String Path) {
     HTTPClient http;
    String serverPath = serverName + Path;
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode>0) {
      String payload = http.getString();
      if (debug) {
        Serial.print(serverName + Path + " : " );
        Serial.println(payload);
      }
      return payload;
    }
    else {
      Serial.print(serverName + Path + " : " );
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      return "Error";
    }
    // Free resources
    http.end();
}

void DoGameStatus() {
  if ((lastDisplayUpdate + (displayUpdateSeconds * 1000)) > millis()) {
    Serial.println("Too soon to update display");
  } else {
    String RedScore = GetHttpInfo("redScore");
    String BlueScore = GetHttpInfo("blueScore");
    String Times = GetHttpInfo("times");
    String Stations = GetHttpInfo("stations");
    GameStatus = GetHttpInfo("gameStatus");
    String Leader = GetHttpInfo("leader");
    String APBattery = GetHttpInfo("battery");
    if ((RedScore == OldRed) & (BlueScore == OldBlue) & (Times == OldTimes) & (Stations == OldStations)){
      Serial.println("No change, so no display update");
    } else {
      Serial.println("Data changed, updating display");
      OldRed = RedScore;
      OldBlue = BlueScore;
      OldTimes = Times;
      OldStations = Stations;
      lastDisplayUpdate = millis();
      String RSSI = ("RSSI:" + String(WiFi.RSSI()));
      display.setRotation(3);
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&FreeMonoBold12pt7b);
      display.setCursor(5, 15);
      display.println("Domination");
      display.setCursor(150, 35);
      display.println(GameStatus); 
      display.setCursor(180, 65);
      display.println(Leader);
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(5, 35);
      display.println("Time:" + Times);    
      display.setCursor(5, 55);
      display.println("Red:" + RedScore);
      display.setCursor(5, 75);
      display.println("Blue:" + BlueScore);
      display.setCursor(5, 100);
      display.println(Stations);
      display.setFont(&FreeMono9pt7b);
      display.setCursor(5, 120);
      display.println("Me:" + String(batteryLevel) + "V");
      display.setCursor(140, 120);
      display.println("AP:" + String(APBattery) + "V");
      display.setCursor(160, 10);
      display.println(RSSI);

      display.update();
      Serial.println("Display updated");
    }
  }
}

void ProcessButtons() {
  if ((digitalRead(plus5) == LOW) & (GameStatus != "Game On")) {
    Serial.println("Plus 5");
    GetHttpInfo("plus5");
  } else if ((digitalRead(minus5) == LOW)& (GameStatus != "Game On")) {
    Serial.println("Minus 5");
    GetHttpInfo("minus5");
  } else if ((digitalRead(action) == LOW) & (digitalRead(startStop) == LOW)){
    Serial.println("Game starts/stop");
    if (GameStatus == "Game On") {
      GetHttpInfo("reset");
      delay(200);
    } else {
      GetHttpInfo("allon");
      delay(200);
    }
  } else {
    return;
  }
  Serial.println("Wait for button release");
  while ((digitalRead(plus5) == LOW) || (digitalRead(minus5) == LOW) || (digitalRead(action) == LOW) || (digitalRead(startStop) == LOW)) {
    delay(50);
  };
  delay (100);
  DoGameStatus();
}

void loop() {
  digitalWrite(LEDPin,HIGH);
  ProcessButtons();
  digitalWrite(LEDPin,LOW);
  if (lastTime + scoreRate < millis()) {
    Serial.print("Checking... ");
    lastTime = millis();
    digitalWrite(LEDPin,HIGH);
    DoGameStatus();
    digitalWrite(LEDPin,LOW);
  }
  if (millis() > batteryCheckTime) {
    batteryLevel = analogRead(battLevelPin) / VOLT_CALC;
    Serial.print(batteryLevel);
    Serial.println("V");
    batteryCheckTime = (millis() + batteryCheckInterval * 1000);
  }
  ArduinoOTA.handle();
  delay(10);
}
