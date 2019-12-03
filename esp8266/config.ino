uint32_t timeOffset = 420;
String timeZone = "America/Los_Angeles";
//Time Zone Selection
//const int TZ_OFFSET = 4*3600;  //AST UTC-4
//const int TZ_OFFSET = 5*3600;  //EST UTC-5
//const int TZ_OFFSET = 6*3600;  //CST UTC-6
//const int TZ_OFFSET = 7*3600;  //MST UTC-7
const int TZ_OFFSET = 8*3600;  //PST UTC-8
//const int TZ_OFFSET = 9*3600;  //AKST UTC-9
//const int TZ_OFFSET = 10*3600;  //HST UTC-10
uint32_t get_time_offset() {
  return TZ_OFFSET;
}

int parse_line(String& buf, int pos, int len) {
  char c = buf[pos++];
  String name;
  String value;
  while (pos < len && c != ' ' && c != ':' && c != '\n' && c != '\0') {
    // Serial.print("C: "); Serial.println(c);
    name += c;
    c = buf[pos++];
  }
  // Serial.print("NAME: ");
  // Serial.println(name);
  // skip separator
  while (pos < len && (c == ' ' || c == ':')) {
    // Serial.print("C: "); Serial.println(c);
    c = buf[pos++];
  }
  while (pos < len && c != '\n' && c != '\0') {
    value += c;
    c = buf[pos++];
  }
  // Serial.print("VALUE: ");
  // Serial.println(value);
  if (name == "timeOffset") {
    timeOffset = value.toInt();
    Serial.print("timeOffset: "); Serial.println(timeOffset);
  } else if (name == "timeZone") {
    timeZone = value;
    Serial.print("timeZone: "); Serial.println(timeZone);
  } else if (name == "") {
    return -1;
  }
  return pos;
}
void load_config() {
  Serial.println("loading config...");
  File file = SPIFFS.open("/config.txt", "r");
  //String content;
  int len = file.available();
  uint8_t buf[256];
  file.read(buf, len);
  buf[len] = '\0';
  String content((char*)buf);
  // Serial.println("CONFIG");
  // Serial.println(content);
  String name;
  String value;
  int pos = 0;
  while (pos >= 0) {
    pos = parse_line(content, pos, len);
  }
}
