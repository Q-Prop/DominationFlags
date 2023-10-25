// Define the MAC addresses of the flag wavers
#define F1MAC "0B"
#define F2MAC "81"
#define F3MAC "39"
#define F4MAC "13"
#define F5MAC "93"
#define F6MAC "B5"
#define F7MAC "A4:E5:7C:B8:CD:1B"
#define F8MAC "E8:DB:84:E1:62:55"

#define redButtonPin D3
#define blueButtoinPin D4
#define redServoPin D7
#define blueServoPin D0
#define battLevelPin A0

#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Pinger.h>
#include <Servo.h> 
#include <Time.h> 
#include <WiFiClient.h>
#include <WiFiUdp.h>

boolean goUpA = false;
boolean goDownA = true;
int nowA = 0;
boolean goUpB = false;
boolean goDownB = true;
int nowB = 0;
int doReset = false;
int doContested = false;
String holding = "I";
boolean gameActive = false;
String myName;
long int batteryCheckTime = 1;
int batteryCheckInterval = 60;
float VOLT_CALC = 195;
float batteryLevel = 9;
int holdStart = 0;
int reTouchTime = 10000;
int holdTime = 2000;
int lastTouch = 0;

Servo ServoA; 
Servo ServoB; 
AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("-");
  Serial.println("Setup Started");
  ServoA.attach(blueServoPin, 700, 2400); 
  ServoB.attach(redServoPin, 700, 2400); 
  pinMode(redButtonPin, INPUT_PULLUP);  //Red team button
  pinMode(blueButtoinPin, INPUT_PULLUP);  //team B Button
  String myMAC = WiFi.macAddress();
       if (myMAC == F1MAC) {myName = "F1";}
  else if (myMAC == F2MAC) {myName = "F2";}
  else if (myMAC == F3MAC) {myName = "F3";}
  else if (myMAC == F4MAC) {myName = "F4";}
  else if (myMAC == F5MAC) {myName = "F5";}
  else if (myMAC == F6MAC) {myName = "F6";}
  else if (myMAC == F7MAC) {myName = "F7";}
  else if (myMAC == F8MAC) {myName = "F8";}
  else {myName = myMAC;}
  WiFi.begin("Flags", "nopassword1");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)  {
    delay(500);
    Serial.print("*");
    i ++;
    if (i > 30) {
      i = 0;
      Serial.println("-");
    }
  }
  Serial.println("*");
  Serial.print("Wifi Started, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Station name ");
  Serial.println(myName);
  ArduinoOTA.setHostname(myName.c_str());
  ArduinoOTA.begin();
  if (!MDNS.begin(myName)) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  server.on("/allon", HTTP_GET, [] (AsyncWebServerRequest *request) {
    doContested = true;
    request->redirect("http://flags.local/");
  });
  server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest *request) {
    doReset = true;
    request->redirect("http://flags.local/");
  });    
  server.on("/aon", HTTP_GET, [] (AsyncWebServerRequest *request) {
    holding = "A";
    gameActive = true;
    request->redirect("http://flags.local/");
  });
  server.on("/bon", HTTP_GET, [] (AsyncWebServerRequest *request) {
    holding = "B";
    gameActive = true;
    request->redirect("http://flags.local/");
  });
  server.on("/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (holding == "A") {request->send(200, "text/plain", "Red On");}
    if (holding == "B") {request->send(200, "text/plain", "Blue On");}
    if (holding == "I") {request->send(200, "text/plain", "Down");}
    if (holding == "C") {request->send(200, "text/plain", "Contested");} 
  });
  server.on("/name", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", myName);
  });
  server.on("/battery", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(batteryLevel));
  });
  server.onNotFound(notFound);
  server.begin();
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(300);
  http.begin(client, "http://10.10.10.1/reTouchTime"); 
  http.GET(); 
  reTouchTime = (http.getString()).toInt();
  Serial.println("Maximum station holding time " + String(reTouchTime) + " seconds");
  http.begin(client, "http://10.10.10.1/holdTime"); 
  http.GET(); 
  holdTime = (http.getString()).toInt();
  Serial.println("Minumum button holding time " + String(holdTime) + " milliseconds");
  Serial.println("Setup Complete");
}


void loop() {
  if (gameActive) { //Read buttons & request actions if the game is on 
    if ((digitalRead(redButtonPin) == LOW) & (digitalRead(blueButtoinPin) == LOW)) {  //Read button state
      Serial.println("Pressing both, doing nothing");
      holdStart = 0;
      lastTouch = millis();
    } else if ((digitalRead(redButtonPin) == LOW) & (holding != "A")){
      if (holdStart == 0) {
        holdStart = millis();
        Serial.println("Pressing Red");
      } else if ((holdStart + holdTime) < millis()) {
        holding = "A";
        Serial.println("Red On");
        holdStart = 0;
      }
    } else if ((digitalRead(blueButtoinPin) == LOW) & (holding != "B")){
      if (holdStart == 0) {
        holdStart = millis();
        Serial.println("Pressing Blue");
      } else if ((holdStart + holdTime) < millis()) {
        holding = "B";
        Serial.println("Blue On");
        holdStart = 0;
      }
    } else {
      holdStart = 0;
    }
    if ((digitalRead(redButtonPin) == LOW) | (digitalRead(blueButtoinPin) == LOW)) {
      lastTouch = millis();
    }
    if (millis() > (lastTouch + (reTouchTime * 1000))) {
      goUpA = true;
      goDownA = false;
      goUpB = true;
      goDownB = false;  
      holding = "I";  
    }
  }


  if (holding == "N") {
    goUpA = false;
    goDownA = true;
    goUpB = false;
    goDownB = true;
  }
  if (holding == "A") {
      goUpA = true;
      goDownA = false;
      goUpB = false;
      goDownB = true;
  }
  if (holding == "B") {
      goUpA = false;
      goDownA = true;
      goUpB = true;
      goDownB = false;
  }

  // Take the defined actions
  if (goUpA) { 
    nowA +=2;
    ServoA.write(nowA);
    if (nowA > 180) {    
      goUpA = false;
      nowA = 180;
    }
  }

  if (goDownA ) { 
    nowA -=2;
    ServoA.write(nowA);
    if (nowA < 0) {    
      goDownA = false;
      nowA = 0;
    }
  }

  if (goUpB) { 
    nowB +=2;
    ServoB.write(nowB);
    if (nowB > 180) {;    
      goUpB = false;
      nowB = 180;
    }
  }

  if (goDownB ) { 
    nowB -=2;
    ServoB.write(nowB);
    if (nowB < 1) {    
      goDownB = false;
      nowB = 0;
    }
  }

  if (millis() > batteryCheckTime) {
    batteryLevel = analogRead(battLevelPin) / VOLT_CALC;
    Serial.print(batteryLevel);
    Serial.println("V");
    batteryCheckTime = (millis() +(batteryCheckInterval * 1000));
  }
  if (doContested) {
    goUpA = true;
    goDownA = false;
    goUpB = true;
    goDownB = false;
    gameActive = true;
    doContested = false;
  }
  if (doReset) {
    goUpA = false;
    goDownA = true;
    goUpB = false;
    goDownB = true;
    holding = "I";
    gameActive = false;
    doReset = false;
  }
  ArduinoOTA.handle();
  delay(1);
}
