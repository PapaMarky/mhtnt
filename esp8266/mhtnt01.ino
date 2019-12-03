/*
 * mhtnt01 : Marky's House Time N' Tempurature
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

extern "C" {
#include "user_interface.h"
}

void setup() {
  Serial.begin(115200);
  SetupSpiffs();
  load_config();
  SetupTempSensor();
  SetupWifi();
  SetupWebServer();
  SetupNTP();
  Serial.print("Flash Memory Size: ");
  Serial.println(system_get_flash_size_map());
  FSInfo info;
  if(SPIFFS.info(info)) {
    Serial.println("FS INFO");
    Serial.print(" - Total Bytes: "); Serial.println(info.totalBytes);
    Serial.print(" -  Used Bytes: "); Serial.println(info.usedBytes);
    Serial.print(" - Avail Bytes: "); Serial.println(info.totalBytes - info.usedBytes);
  } else {
    Serial.println(" --- FAILED TO GET SPIFFS INFO!");
  }
  //system_print_meminfo();
  //Serial.println(system_get_time());
}

void loop() {
  WebServerLoop();
  ntp_loop();
  check_history();
  MDNS.update();
}
