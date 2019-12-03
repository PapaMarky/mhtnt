#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#ifndef STASSID
#define STASSID "marky00wireGenkan"
#define STAPSK  "welcome to markys network"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* host = "esp8266fs";

bool IsWifiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}
void SetupWifi() {
 //WIFI INIT
  Serial.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFiSleepType_t sleeper = WiFi.getSleepMode();
  Serial.print("SLEEP MODE: "); Serial.println(sleeper);
  if (sleeper != WIFI_NONE_SLEEP) {
    Serial.println("Setting Sleep mode to NONE");
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFiSleepType_t sleeper = WiFi.getSleepMode();
    Serial.print("SLEEP MODE: "); Serial.println(sleeper);
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin(host);
  Serial.print("Open http://");
  Serial.print(host);
  Serial.println(".local/edit to see the file browser");
}
