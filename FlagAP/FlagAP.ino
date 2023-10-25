// Three settings to manage timing
//
// How many milliseconds must a button be held to count as domination
#define holdTime 500
// How many milliseconds should the siren sound at the start of the game
#define sirenAllOn 3000
// How many milliseconds should the siren sound at the end of the game
#define sirenReset 3000

// That's it, beyond here is the code that does things

#include <ESPping.h>
#include <ping32.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESPping.h>
#include <Time.h> 
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "MyHTML.h"

#define battLevelPin A0
#define sirenPin D5
#define LEDPin D4

String myName;
int batteryCheckInterval = 60;
float VOLT_CALC = 195;
float batteryLevel = 9;
//Scoring
int blueScore = 999;
int BlueHold = 0;
int redScore = 999;
int RedHold = 0;
String sBlueScore = String(blueScore);
String sRedScore = String(redScore);
//Station records
int stationCount = 0;
String stationList;
String Stations[10];
String StationNames[10];
long int stationTime =0;
String ThisIPAddress;
//Game Operations
int doReset = false;
int doContested = false;
boolean gameActive = false;
//Game Timing
int reTouchTime = 30;
int gameMinutes = 5;
long int nextScoringTime;
int remainMinutes = 99;
int remainSeconds = 99;
long int startTime;
String times = "99-99";
long int nowMillis;
long int batteryCheckTime = 1;
//Screamy Siren
long int sirenTime;
bool sirenOn = false;
//SoftAP variables
const byte DNS_PORT = 53;  
IPAddress ip(10,10,10,1);
IPAddress gateway(0,0,0,0);
IPAddress subnet(255,255,255,0);


// *** Objects ***
AsyncWebServer server(80);
DNSServer      dnsServer; 


//*****************************************************
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void CheckNewStations() {
  if ((WiFi.softAPgetStationNum() != stationCount) & (stationTime == 0)) { // New station connected
    stationTime = millis() + 5000;
  }
  if ((stationTime > 0) & (millis() > stationTime)) {
    Serial.println("New Stations");
    stationTime = 0;
    String newstationList = "";
    int i = 100;
    stationCount = 0;
    while ((stationCount < WiFi.softAPgetStationNum() ) & (i < 120)) {
      ThisIPAddress = "10.10.10." + String(i);    
      stationCount ++;    
      HTTPClient http;
      http.setTimeout(300);
      WiFiClient client;
      http.begin(client, "http://" + ThisIPAddress + "/name"); 
      int httpCode = http.GET();
      if (httpCode == 200) {
        String mDNSname = http.getString();
        if (mDNSname != "") {
          Serial.println("Found station " + mDNSname + " at " + ThisIPAddress);
          Stations[stationCount] = (ThisIPAddress);
          StationNames[stationCount] = (mDNSname);
          if (gameActive) {
            http.begin(client, "http://" + ThisIPAddress + "/status"); 
            int httpCode = http.GET();
            if (http.getString().indexOf("Down") > -1) {
              http.begin(client, "http://" + ThisIPAddress + "/allon"); 
              int httpCode = http.GET();
            }
          } else {
            http.begin(client, "http://" + ThisIPAddress + "/reset"); 
            int httpCode = http.GET();
          }
        }
      } else {
        Serial.println("Did not add " + ThisIPAddress + " due to return code " + httpCode);
      }
      http.end();
    i++;
    }
    for (int i=(stationCount + 1); i<10; i++) {
      StationNames[i] = "";
    }
    sort(StationNames, 10);
    for( int i=0; i<10; i++) {
          if (newstationList == "") {
            newstationList = StationNames[i];
          } else {
            newstationList = (newstationList + " " + StationNames[i]);
          }
    }
    stationList = newstationList;
    Serial.println(stationList);
    Serial.println("Done adding");
  }
}

void resetGame() {
  Serial.println("Ending Game");
  gameActive = false;
  WiFiClient client;
  for (auto station : Stations) {
    HTTPClient http;
    http.begin(client, "http://" + station + "/reset"); 
    http.GET();
  };
  digitalWrite(sirenPin, HIGH);
  sirenTime = millis() + sirenReset;
  sirenOn = true;
  doReset = false;
  if (redScore > blueScore) {
    sRedScore = (String(RedHold) + ":Win " + String(redScore));
    sBlueScore = (String(BlueHold) + ":Lose " + String(blueScore));
  } else if (redScore < blueScore){
    sRedScore = (String(RedHold) + ":Lose " + String(redScore));
    sBlueScore = (String(BlueHold) + ":Win " + String(blueScore));
  } else {
    sRedScore = (String(RedHold) + ":Tied " + String(redScore));
    sBlueScore = (String(BlueHold) + ":Tied " + String(blueScore));      
  }
}
void startGame() {
  Serial.println("Starting Game");
  redScore = 0;
  blueScore = 0;
  nextScoringTime = millis() + 1000;
  digitalWrite(sirenPin, HIGH);
  sirenTime = millis() + sirenAllOn;
  sirenOn = true;
  gameActive = true;
  WiFiClient client;
  for (auto station : Stations) {
    HTTPClient http;
    http.begin(client, "http://" + station + "/allon"); 
    http.GET();        
  };
  startTime = millis();
  doContested = false; 
}
void runGame() {
  nowMillis = millis();
  if (nowMillis > (startTime + (gameMinutes * 60000))) {
    gameActive = false;
    doReset = true;
  } else {        
    remainMinutes = (gameMinutes - ((nowMillis - startTime)/60000)) -1;
    remainSeconds = 60-(((nowMillis - startTime)/1000) % 60) - 1;
    String stringSeconds = String(remainSeconds);
    if(stringSeconds.length() < 2) {
      stringSeconds = "0" + stringSeconds;
    };
    times = String(gameMinutes) + "/" + String(remainMinutes) + ":" + stringSeconds;
  }
  if (nowMillis > nextScoringTime) {
    nextScoringTime += 1000;
    int newRedHold = 0;
    int newBlueHold = 0;
    int scoreStationCount = 0;
    WiFiClient client;
    for (auto station : Stations) {
      if (station != "") {
        Serial.println("Scoring station " + station);
        HTTPClient http;
        http.setTimeout(300);
        http.begin(client, "http://" + station + "/name"); 
        http.GET();  
        String mDNSname = http.getString();
        http.begin(client, "http://" + station + "/status"); 
        http.GET(); 
        if (http.getString().indexOf("Red On") > -1) {
          redScore ++;
          newRedHold ++;
          mDNSname += "R";
          } 
        if (http.getString().indexOf("Blue On") > -1) {
          blueScore ++;
          newBlueHold ++;
          mDNSname += "B";
          }
        if (http.getString().indexOf("Down") > -1) {
          Serial.println("Setting station " + station + " contested");
          http.begin(client, "http://" + station + "/allon"); 
          http.GET(); 
          mDNSname += "C";
        }
        if (http.getString().indexOf("Contested") > -1) {
          mDNSname += "C";
        }
        StationNames[scoreStationCount] = mDNSname;
        scoreStationCount++;
      } 
    }
    for (int i=(scoreStationCount + 1); i<10; i++) {
      StationNames[i] = "";
    }
    sort(StationNames, 10);
    String newstationList = "";
    for( int i=0; i<10; i++) {
          if (newstationList == "") {
            newstationList = StationNames[i];
          } else {
            newstationList = (newstationList + " " + StationNames[i]);
          }
    }
    stationList = newstationList;
    Serial.println(stationList);
    RedHold = newRedHold;
    BlueHold = newBlueHold;
    sRedScore = (String(RedHold) + "-" + String(redScore));
    sBlueScore = (String(BlueHold) + "-" +String(blueScore));   
    Serial.println(stationList);
  } 
} 

void sort( String *p, int n)
{
  // Very simple sort.
  // Find the lowest, and put it the first location,
  // Then find the lowest of the remaining and put it in the second location.
  // And so on.

  if( n > 1)     // When there is one String object, then there is no sorting to do.
  {
    for( int i=0; i<n; i++)
    {
      for( int j=i+1; j<n; j++)
      {
        if( p[j] < p[i])   // compare the text of one String object to text of another.
        {
          // The new (higher index) object has lower text.
          // Put the new one in the place of the first one.
          String q = p[i];
          p[i] = p[j];
          p[j] = q;
        }
      }
    }
  }
}
//*****************************************************
void setup() {
  Serial.begin(115200);
  pinMode(LEDPin, OUTPUT);
  delay(1000);
  Serial.println("-");
  Serial.println("Setup Started");
  pinMode(sirenPin,OUTPUT);
  //myName = WiFi.macAddress().substring(15);
  myName = "flags";
  Serial.print("I am the AP:");
  Serial.println(myName);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP("Flags", "nopassword1", 1, false, 8);
  digitalWrite(sirenPin,HIGH);
  delay(1000);
  digitalWrite(sirenPin,LOW);
  Serial.println("Wifi AP Started");
  ArduinoOTA.setHostname(myName.c_str());
  ArduinoOTA.begin();
  Serial.println("Wifi Started, IP address: 10.10.10.1");
  if (MDNS.begin(myName)) {             
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
  dnsServer.start(DNS_PORT, "flags.local", ip);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
    });
  server.on("/allon", HTTP_GET, [] (AsyncWebServerRequest *request) {
    doContested = true;
    request->redirect("/");
  });
  server.on("/battery", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(batteryLevel));
  });
  server.on("/blueScore", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", sBlueScore);
  });
  server.on("/endGame", HTTP_GET, [](AsyncWebServerRequest *request){
    if (gameActive) {
      request->send_P(200, "text/html", end_html);
    } else {
      request->redirect("/");
    }
  });
    server.on("/favicon.ico", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send_P(200, "image/x-icon", tblFavicon, tblFavicon_len);
  });
  server.on("/gameStatus", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (gameActive) {
      request->send(200, "text/plain", "Game On");
    } else {
      request->send(200, "text/plain", "Stopped");
    }  
  });
  server.on("/holdTime", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(holdTime));
  });
  server.on("/leader", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (redScore > blueScore) {
      request->send(200, "text/plain", "Red");
    } else if (blueScore > redScore) {
      request->send(200, "text/plain", "Blue");
    } else if (gameActive) {
      request->send(200, "text/plain", "Tied");
    } else {
      request->send(200, "text/plain", " ");
    }
  });
  server.on("/minus5", HTTP_GET, [] (AsyncWebServerRequest *request) {
    gameMinutes -= 5;
    if (gameMinutes < 5) {
      gameMinutes = 5;
    }
    request->redirect("/");
  });
  server.on("/minus5s", HTTP_GET, [] (AsyncWebServerRequest *request) {
    reTouchTime -= 5;
    if (reTouchTime < 5) {
      reTouchTime = 5;
    }
    request->redirect("/");
  });
  server.on("/overrides", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", override_html);
  });
  server.on("/plus5", HTTP_GET, [] (AsyncWebServerRequest *request) {
    gameMinutes += 5;
    request->redirect("/");
  });
  server.on("/plus5s", HTTP_GET, [] (AsyncWebServerRequest *request) {
    reTouchTime += 5;
    request->redirect("/");
  });
  server.on("/redScore", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", sRedScore);
  });
  server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest *request) {
    doReset = true;
    request->redirect("/");
  });    
  server.on("/reTouchTime", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(reTouchTime));
  });
  server.on("/startGame", HTTP_GET, [](AsyncWebServerRequest *request){
    if (gameActive) {
      request->redirect("/");
    } else {
      request->send_P(200, "text/html", start_html);
    }
  });
  server.on("/stations", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", (stationList));
  });
  server.on("/times", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", times);
  }); 
  server.onNotFound(notFound);
  server.begin();
  Serial.println("Setup complete");
}

//*****************************************************
void loop() {
  CheckNewStations();
  if (doReset) {resetGame();}
  if (doContested) {startGame();}
  if (gameActive) {runGame();} else {times = String(gameMinutes) + "-" + String(gameMinutes); }
  if (sirenOn) {
    digitalWrite(sirenPin, HIGH);
    if (millis() > sirenTime) {
      sirenOn = false;
    }
  } else {
    digitalWrite(sirenPin, LOW);
  }
    if (millis() > batteryCheckTime) {
    batteryLevel = analogRead(battLevelPin) / VOLT_CALC;
    Serial.print(batteryLevel);
    Serial.println("V");
    batteryCheckTime = (millis() +(batteryCheckInterval * 1000));
  }
  digitalWrite(LEDPin, HIGH);
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  delay(1);
}
