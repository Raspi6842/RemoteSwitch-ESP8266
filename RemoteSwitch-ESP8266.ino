/*

  https://github.com/Raspi6842/RemoteSwitch-ESP8266
  GNU General Public License v3.0

  First steps are located in README.md file

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "FS.h"

#define AP_SSID "RemoteSwitch_1_0"
#define AP_PSK  "switch config"

#define RELAY 12
#define LED   13
#define BTN   0

ESP8266WebServer server(80);

String controllerAddress = "", idx = ""; // will be filled from config file in setup()

void setup() {
  ESP.wdtDisable();
  Serial.begin(115200);
  Serial.println( F("ESP Remote Switch v1.0b") );


  //  Enable pins
  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);

  // Show user time to press button
  for(int i = 0; i < 16; i++) {
    digitalWrite(LED, !digitalRead(LED));
    delay(100);
  }

  // Erase settings with switch debouncing
  if( !digitalRead(BTN) ) {
    delay(100);
    if( !digitalRead(BTN) ) {
      while( !digitalRead(BTN) );
      eraseSettings();
    }
  }

  Serial.println( F("Mounting FS...") );

  // Init SPI Flash File System
  if(!SPIFFS.begin()) {
    Serial.println( F("FS mount failed!") );
    while(1);
  }

  File configFile;

  // Check config file exists
  if( SPIFFS.exists("/config.json") ) {
    Serial.println( F("Config file found, loading...") );
    configFile = SPIFFS.open("/config.json", "r");
    if (!configFile) {
      Serial.println( F("Failed to open config file") );
      while(1);
    }
  } else {
    Serial.println( F("Config file not found, entering config mode") );
    enableConfigMode();
  }

  // Stop program, if file is too big
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    while(1);
  }

  // Load file to buffer
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  // Load buffer to JSON buffer
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  // Check JSON valid
  if (!json.success()) {
    Serial.println("Failed to parse config file");
    while(1);
  }

  // Set variables
  controllerAddress = (const char*) json["CONTROLLER"];
  idx = (const char*) json["IDX"];

  Serial.print( F("\nConnecting to: ") );
  Serial.println( (const char*) json["SSID"] );

  // Connect to WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin( (const char*) json["SSID"], (const char*) json["PSK"] );
  configFile.close();

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println( F("\nWiFi connected") );
  Serial.print( F("IP address: ") );
  Serial.println(WiFi.localIP());


  // Enable Update Over The Air (OTA)
  ArduinoOTA.setHostname( (const char*) json["OTA_HOSTNAME"] );
  ArduinoOTA.setPassword( (const char*) json["OTA_PASSWD"] );
  
  ArduinoOTA.onStart([]() {
    SPIFFS.end();
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
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
  ArduinoOTA.begin();

  // Prepare and start web server
  Serial.println( F("Starting webserver...") );
  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.on("/cmd", handleCmd);
  server.begin();
  Serial.println( F("Done\n") );
}

void loop() {

  //  Check if button is pressed and change relay state
  if( !digitalRead(BTN) ) {
    while(!digitalRead(BTN));
    digitalWrite(RELAY, !digitalRead(RELAY));
    reportRelayState();
  }

  //  Set LED state same as relay state
  if( digitalRead(RELAY) ) digitalWrite(LED, HIGH);
  else digitalWrite(LED, LOW);

  //  Handle client by server
  server.handleClient();
  ESP.wdtFeed();
  
  //  Handle OTA update
  ArduinoOTA.handle();
}

void enableConfigMode() {
  Serial.println( F("Starting config webserver...") );
  //  Start access point
  WiFi.softAP(AP_SSID, AP_PSK);
  Serial.print( F("Config webserver listening on: ") );
  Serial.println(WiFi.softAPIP());
  //  Prepare serve files
  server.serveStatic("/", SPIFFS, "/config.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.on("/save", handleConfigSave);
  //  Start server
  server.begin();
  while(1) {
    ESP.wdtFeed();
    //  Handle client
    server.handleClient();
  }
}

void handleConfigSave() {

  //  prepare buffer for new config
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  //  Check if form has been successful submited
  if( server.hasArg("ssid") == false ) {
    server.send(200, "text/plain", F("Data not received"));
    Serial.println( F("POST data not received") );
    return;
  } else {
    //  Check if SSID or password is too long
    if(server.arg("ssid").length()>32) {
      server.send(200, "text/plain", F("SSID is too long (max 32 chars)"));
      return;
    } else {
      if(server.hasArg("psk")) {
        if(server.arg("psk").length() > 32) {
          server.send(200, "text/plain", F("Password is too long (max 32 chars)"));
          return;
        } else {
          json["SSID"] = String(server.arg("ssid"));
          json["PSK"] = String(server.arg("psk"));
        }
      } else {
        json["SSID"] = String(server.arg("ssid"));
        json["PSK"] = "";
      }

      //  Create config file
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("Failed to open config file for writing");
        while(1);
      }

      //  Save config file
      json.printTo(configFile);
      configFile.close();

      //  Serve success message to client
      File file = SPIFFS.open("/configSaved.html", "r");
      size_t sent = server.streamFile(file, "text/html");
      file.close();
      file = SPIFFS.open("/style.css", "r");
      sent = server.streamFile(file, "text/css");
      file.close();

      //  Restart ESP
      delay(1500);
      ESP.restart();
    }
  }
  

}

void eraseSettings() {
  Serial.println( F("Erasing config...") );
  //  Check if file exists and remove it
  if( SPIFFS.exists("/config.json") ) SPIFFS.remove("/config.json");
  Serial.println( F("Restarting...\n\n") );
  delay(500);
  ESP.restart();
}

void handleCmd() { 
  // Check if args received
  if( server.args() > 0 ) {
    //  Return system state
    if( server.arg("cmd") == "get" ) {
      server.send(200, "application/json", "{\"btn\":\"" + String(digitalRead(BTN)?"Not pressed":"Pressed") + 
      "\",\"led\":\"" + String(digitalRead(LED)?"On":"Off") + "\",\"relay\":\"" + String(digitalRead(RELAY)?"On":"Off") + "\"}");
    } else if( server.arg("cmd") == "set" ) {
      //  Set new state
      if( server.arg("relay") != "" ) {
        digitalWrite(RELAY, (server.arg("relay")=="1")?1:0);
        server.send(200, "text/plain", F("OK"));
        reportRelayState();
      }
      else server.send(200, "text/plain", F("Bad arg."));
    } else {
      server.send(200, "text/plain", F("Bad arg."));
    }
  } else {
    server.send(200, "text/plain", F("Command site."));
  }
}

void reportRelayState() {
  //  Create http request and fill get query
  HTTPClient http;
  http.begin( String("http://") + controllerAddress + String("/json.htm?type=command&param=udevice&idx=") + idx + ("&nvalue=") + String((digitalRead(RELAY))?"1":"0"));
  http.GET();
  http.end();
}
