#include <WiFiUdp.h>

#include "time.h"

WiFiUDP UDP;

IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "pool.ntp.org";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
boolean need_boot_log = true;
boolean time_is_set = false;

String make_timestamp(struct tm *t) {
  char buffer[128];
  sprintf(buffer, "%4d/%02d/%02d %02d:%02d:%02d", 
    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
    t->tm_hour, t->tm_min, t->tm_sec);

   return String(buffer);
}
const char * const RST_REASONS[] = {
  "REASON_DEFAULT_RST",
  "REASON_WDT_RST",
  "REASON_EXCEPTION_RST",
  "REASON_SOFT_WDT_RST",
  "REASON_SOFT_RESTART",
  "REASON_DEEP_SLEEP_AWAKE",
  "REASON_EXT_SYS_RST"
};
void add_boot_log() {
  if (!SPIFFS.exists("/bootlog.txt")) {
    File f = SPIFFS.open("/bootlog.txt", "w");
    f.write("DATETIME, BOOT REASON, EXCCAUSE, EPC1, EPC2, EPC3, EXCVADDR, DEPC\n");
    f.close();
  }
  File file = SPIFFS.open("/bootlog.txt", "a");
  time_t t = get_localtime();
  
  if (t == 0) {
    return;
  }
  struct rst_info* info = system_get_rst_info();
  struct tm *timeinfo = localtime(&t);
  if (!timeinfo) {
    Serial.println("Failed to get local time!");
    return;
  }
  // exccause: https://arduino-esp8266.readthedocs.io/en/latest/exception_causes.html
  String time_str = make_timestamp(timeinfo);
  Serial.print("Logging boot: "); Serial.println(time_str);
  file.write(time_str.c_str());  
  file.write(", "); file.write(RST_REASONS[info->reason]);  
  file.write(", "); file.write(String(info->exccause).c_str()); 
  file.write(", 0x"); file.write(String(info->epc1, HEX).c_str()); 
  file.write(", 0x"); file.write(String(info->epc2, HEX).c_str()); 
  file.write(", 0x"); file.write(String(info->epc3, HEX).c_str()); 
  file.write(", 0x"); file.write(String(info->excvaddr, HEX).c_str()); 
  file.write(", 0x"); file.write(String(info->depc, HEX).c_str()); 
  file.write('\n');
  file.close();
  need_boot_log = false;
}
void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println("DNS lookup failed. Not Rebooting.");
    Serial.flush();
    // ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);  
}

void SetupNTP() {
  startUDP();
}
uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  time_is_set = true;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}

unsigned long intervalNTP = 3600000; // Request NTP time every hour
// unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;

unsigned long prevActualTime = 0;

void ntp_loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If an hour has passed since last NTP request
    prevNTP = currentMillis;
    Serial.print("\r\nSending NTP request to ");
    Serial.print(NTPServerName);
    Serial.println("...");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
    if (need_boot_log) {
      add_boot_log();
    }
  } else if ((currentMillis - lastNTPResponse) > 12 * intervalNTP) {
    Serial.println("More than 1 hour since last NTP response. Not Rebooting.");
    Serial.flush();
    // ESP.reset();
  }
  /*
  uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse)/1000;
  if (actualTime != prevActualTime && timeUNIX != 0) { // If a second has passed since last print
    if (getMinutes(prevActualTime) != getMinutes(actualTime)) {
      Serial.printf("\rUTC time:\t%d:%d:%d", getHours(actualTime), getMinutes(actualTime), getSeconds(actualTime));
      Serial.println();
      uint32_t localTime = actualTime - (get_time_offset() * 60);
      Serial.printf("\rLOCAL time:\t%d:%d", getHours(localTime), getMinutes(localTime), getSeconds(localTime));
      Serial.println();
    }
    prevActualTime = actualTime;
  }
    */
}
int dstOffset (unsigned long unixTime)
{
  //Receives unix epoch time and returns seconds of offset for local DST
  //Valid thru 2099 for US only, Calculations from "http://www.webexhibits.org/daylightsaving/i.html"
  //Code idea from jm_wsb @ "http://forum.arduino.cc/index.php/topic,40286.0.html"
  //Get epoch times @ "http://www.epochconverter.com/" for testing
  //DST update wont be reflected until the next time sync
  time_t t = unixTime;
  struct tm *timeinfo = localtime(&t);
  //int beginDSTDay = (14 - (1 + timeinfo->tm_year * 5 / 4) % 7); 
  //int beginDSTMonth=3;
  //int endDSTDay = (7 - (1 + timeinfo->tm_year * 5 / 4) % 7);
  //int endDSTMonth=11;
  //if (((timeinfo->tm_mon > beginDSTMonth) && (timeinfo->tm_mon < endDSTMonth))
  //  || ((timeinfo->tm_mon == beginDSTMonth) && (timeinfo->tm_mday > beginDSTDay))
  //  || ((timeinfo->tm_mon == beginDSTMonth) && (timeinfo->tm_mday == beginDSTDay) && (timeinfo->tm_hour >= 2))
  //  || ((timeinfo->tm_mon == endDSTMonth) && (timeinfo->tm_mday < endDSTDay))
  //  || ((timeinfo->tm_mon == endDSTMonth) && (timeinfo->tm_mday == endDSTDay) && (timeinfo->tm_hour < 1)))
  if (timeinfo->tm_isdst) 
    return (3600);  //Add back in one hours worth of seconds - DST in effect
  else
    return (0);  //NonDST
}

uint32_t get_localtime() {
  if (timeUNIX == 0) return 0;
  time_t UTCTime = timeUNIX + (millis() - lastNTPResponse)/1000;
  uint32_t localTime = UTCTime - (get_time_offset() + dstOffset(UTCTime));
  return localTime;
}

String get_time_json() {
  time_t t = get_localtime();
  // Serial.print("LinuxTime: "); Serial.println(timeUNIX);
  // Serial.print("localTime: "); Serial.println(ltime);
  // int h = getHours(ltime);
  // Serial.print("Hour: "); Serial.println(h);
  // while (h > 12) h -= 12;
  // Serial.print("Hour: "); Serial.println(h);
  struct tm *timeinfo = localtime(&t);
  if (!timeinfo) {
    Serial.println("Failed to get local time!");
    //return;
  }
  String r = "{\"hour\": " + String(timeinfo->tm_hour) + ", \"minute\": " + String(timeinfo->tm_min) + "}";
  // Serial.print("Time JSON: ");
  // Serial.println(r);
  return r;
}
