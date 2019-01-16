/*

  https://github.com/Raspi6842/RemoteSwitch-ESP8266
  GNU General Public License v3.0

  First steps are located in README.md file

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define AP_SSID "RemoteSwitch_1_0"
#define AP_PSK  "switch config"

#define RELAY 12
#define LED   13
#define BTN   0

ESP8266WebServer server(80);

struct {
  bool firstStart;
  char ssid[32];
  char psk[32];
} ap_config;

void setup() {
  ESP.wdtDisable();
  Serial.begin(115200);
  Serial.println( F("ESP Remote Switch v1.0a") );

  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  
  EEPROM.begin(128);
  EEPROM.get(0, ap_config);

  for(int i = 0; i < 16; i++) {
    digitalWrite(LED, !digitalRead(LED));
    delay(100);
  }

  if( !digitalRead(BTN) ) {
    delay(100);
    if( !digitalRead(BTN) ) {
      while( !digitalRead(BTN) );
      eraseSettings();
    }
  }

  if( ap_config.firstStart ) {
    Serial.println( F("Entering config mode") );
    enableConfigMode();
  }
  
  Serial.print( F("\nConnecting to: ") );
  Serial.println(String(ap_config.ssid));
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ap_config.ssid, ap_config.psk);

  while(WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  
  Serial.println( F("\nWiFi connected") );
  Serial.print( F("IP address: ") );
  Serial.println(WiFi.localIP());
  
  Serial.println( F("Starting webserver...") );
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();
  Serial.println( F("Done\n") );
}

void loop() {
  if( !digitalRead(BTN) ) {
    digitalWrite(RELAY, !digitalRead(RELAY));
    delay(250);
  }
  if( digitalRead(RELAY) ) digitalWrite(LED, HIGH);
  else digitalWrite(LED, LOW);
  server.handleClient();
}

void enableConfigMode() {
  Serial.println( F("Starting config webserver...") );
  WiFi.softAP(AP_SSID, AP_PSK);
  Serial.print( F("Config webserver listening on: ") );
  Serial.println(WiFi.softAPIP());
  server.on("/", handleConfigRoot);
  server.on("/save", handleConfigSave);
  server.begin();
  while(1) {
    server.handleClient();
  }
}

void handleConfigRoot() {
  server.send(200, "text/html", F(
  "<html>" \
    "<head>" \
      "<title>Remote Switch Config Page</title>" \
      "<meta charset='utf-8' />" \
    "</head>" \
    "<body>" \
    "<h1>Remote Switch configuration page</h1>" \
    "<form action='/save' method='POST'>" \
      "<input type='text' placeholder='SSID' name='ssid'/><br />" \
      "<input type='password' placeholder='WiFi password' name='psk' /><br />" \
      "<input type='submit' value='Save' />" \
    "</form>" \
    "</body>" \
  "</html>"
  ));
}

void handleConfigSave() {
  if( server.hasArg("ssid") == false ) {
    server.send(200, "text/plain", F("Data not received"));
    Serial.println( F("POST data not received") );
    return;
  } else {
    if(server.arg("ssid").length()>32) {
      server.send(200, "text/plain", F("SSID is too long (max 32 chars)"));
      return;
    } else {
      if(server.hasArg("psk")) {
        if(server.arg("psk").length() > 32) {
          server.send(200, "text/plain", F("Password is too long (max 32 chars)"));
          return;
        } else {
          strcpy(ap_config.ssid, String(server.arg("ssid")).c_str());
          strcpy(ap_config.psk, String(server.arg("psk")).c_str());
        }
      } else {
        strcpy(ap_config.ssid, String(server.arg("ssid")).c_str());
        memset(ap_config.psk, 0, 32);
      }
      ap_config.firstStart = false;
      EEPROM.put(0, ap_config);
      EEPROM.commit();
      server.send(200, "text/plain", F("Settings saved. Restarting device."));
      ESP.restart();
    }
  }
}

void eraseSettings() {
  Serial.println( F("Erasing config...") );
  memset(ap_config.ssid, 0, 32);
  memset(ap_config.psk, 0, 32);
  ap_config.firstStart = true;
  EEPROM.put(0, ap_config);
  EEPROM.commit();
  Serial.println( F("Restarting...\n\n") );
  delay(500);
  ESP.restart();
}

void handleRoot() {
  server.send(200, "text/plain", F("It's working!") );
}

void handleCmd() {
  if( server.args() > 0 ) {
    if( server.arg("cmd") == "get" ) {
      server.send(200, "application/json", "{\"btn\":\"" + String(digitalRead(BTN)?"Not pressed":"Pressed") + 
      "\",\"led\":\"" + String(digitalRead(LED)?"On":"Off") + "\",\"relay\":\"" + String(digitalRead(RELAY)?"On":"Off") + "\"}");
    } else if( server.arg("cmd") == "set" ) {
      if( server.arg("relay") != "" ) {
        digitalWrite(RELAY, (server.arg("relay")=="1")?1:0);
        server.send(200, "text/plain", F("OK"));
      }
      else server.send(200, "text/plain", F("Bad arg."));
    } else {
      server.send(200, "text/plain", F("Bad arg."));
    }
  } else {
    server.send(200, "text/plain", F("Command site."));
  }
}
