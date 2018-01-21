#define R_PIN D1
#define G_PIN D2
#define B_PIN D3

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "FS.h"

enum Color {
  R,G,B
};
static uint8_t color[3] = {0,0,0};
static uint8_t brightness = 0;

// Wi-Fi network to connect to (if not in AP mode)

const char WiFiAPPSK[] = "";
static String ssid = "ASUS";
static String password = "1Hemmelighet?";

ESP8266WebServer server(80);

void setup() {

  pinMode(R_PIN,OUTPUT);
  pinMode(G_PIN,OUTPUT);
  pinMode(B_PIN,OUTPUT);

  Serial.begin(115200);

  EEPROM.begin(512);
  loadSettings();
  setLight();
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  }

   WiFi.mode(WIFI_STA);
   Serial.printf("Connecting to %s\n", ssid.c_str());
    if (String(WiFi.SSID()) != ssid) {
      WiFi.begin(ssid.c_str(), password.c_str());
    }
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count <20) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected! Open http://");
      Serial.print(WiFi.localIP());
      Serial.println(" in your browser");
    }
    else {
      WiFi.mode(WIFI_AP);
      // Do a little work to get a unique-ish name. Append the
      // last two bytes of the MAC (HEX'd) to "Thing-":
      uint8_t mac[WL_MAC_ADDR_LENGTH];
      WiFi.softAPmacAddress(mac);
      String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                     String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
      macID.toUpperCase();
      String AP_NameString = "ESP8266 Thing " + macID;
      char AP_NameChar[AP_NameString.length() + 1];
      memset(AP_NameChar, 0, AP_NameString.length() + 1);
      for (int i = 0; i < AP_NameString.length(); i++){
        AP_NameChar[i] = AP_NameString.charAt(i);
      }
  
      WiFi.softAP(AP_NameChar, WiFiAPPSK);
      Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
      Serial.println("and open http://192.168.4.1 in your browser");
   }
   
  server.on("/brightness", HTTP_GET, []() {sendBrightness();});
  server.on("/brightness", HTTP_POST, []() {
    String value = server.arg("value");
    setBrightness(value.toInt());
    sendBrightness();
  });

  server.on("/color", HTTP_GET, []() {sendColor();});
  server.on("/color", HTTP_POST, []() {
    String r = server.arg("r");
    String g = server.arg("g");
    String b = server.arg("b");
    setColor(r.toInt(), g.toInt(), b.toInt());
    sendColor();
  });
  server.on("/wifi", HTTP_POST, []() {
    ssid = server.arg("ssid");
    password = server.arg("pass");
    saveWifi();
  });
  server.on("/getAll", HTTP_GET, []() {getAll();});
  server.on("/save", HTTP_POST, []() {
    saveSettings();
    getAll();
  });
  server.serveStatic("/index.htm", SPIFFS, "/index.htm");
  server.serveStatic("/", SPIFFS, "/index.htm");
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}

void loadSettings() {
   brightness = EEPROM.read(0);
   color[R] = EEPROM.read(R+1);
   color[G] = EEPROM.read(G+1);
   color[B] = EEPROM.read(B+1);
   if (SPIFFS.exists("wifi.conf")) {
      loadWifi();
   }
}

bool loadWifi() {
  // open file for reading.
  File configFile = SPIFFS.open("/wifi.conf", "r");
  if (!configFile) {
    Serial.println("Failed to open wifi.conf.");
    return false;
  }
  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  content.trim();
  // Check if ther is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1) {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1) {pos = content.indexOf("\r");}
  }
  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Invalid content..");
    Serial.println(content);
    return false;
  }
  // Store SSID and PSK into string vars.
  ssid = content.substring(0, pos);
  password = content.substring(pos + le);
  ssid.trim();
  password.trim();
  return true;
}

bool saveWifi() {
  // Open config file for writing.
  File configFile = SPIFFS.open("/wifi.conf", "w");
  if (!configFile)
  {
    Serial.println("Failed to open wifi.conf for writing");
    return false;
  }
  // Save SSID and PSK.
  configFile.println(ssid);
  configFile.println(password);
  configFile.close();
  return true;
}

void setBrightness(int value) {
  brightness=value;
  setLight();
}
void sendBrightness() {
  server.send(200, "text/json", String(brightness));
}
void setColor(const int8_t &r,const int8_t &g, const int8_t &b) {
  color[R] = r;
  color[G] = g;
  color[B] = b;
  setLight();

}
void sendColor() {
  String json = "{";
  json += "\"r\":" + String(color[R]);
  json += ",\"g\":" + String(color[G]);
  json += ",\"b\":" + String(color[B]);
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void setLight() {
  analogWrite(R_PIN, (color[R]*brightness) /65025. * 1023 );
  analogWrite(G_PIN, (color[G]*brightness) /65025. * 1023 );
  analogWrite(B_PIN, (color[B]*brightness) /65025. * 1023 );
}
void saveSettings() {
  EEPROM.write(0, brightness);
  EEPROM.write(R+1, color[R]);
  EEPROM.write(G+1, color[G]);
  EEPROM.write(B+1, color[B]);
  EEPROM.commit();
}

void getAll() {
  String json = "{";
  json += "\"r\":" + String(color[R]);
  json += ",\"g\":" + String(color[G]);
  json += ",\"b\":" + String(color[B]);
  json += ",\"brightness\":" + String(brightness);
  json += "}";
  server.send(200, "text/json", json);
  json = String();
  
}

