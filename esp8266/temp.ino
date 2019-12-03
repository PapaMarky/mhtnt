#include <Adafruit_MCP9808.h>

// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
unsigned int last_temp = 0;
float temp_f = 0.0;
float temp_c = 0.0;

uint32_t lastSampleTime = 0;

const int SAMPLE_HOURS = 24; // hours
const int SAMPLE_INTERVAL = 5; // minutes
const int HISTORY_SIZE = (SAMPLE_HOURS * 60 / SAMPLE_INTERVAL) + 1;
const int SAMPLE_TIME_LIMIT = SAMPLE_HOURS * 3600; // seconds of sample data
boolean history_loaded = false;
struct history_sample {
  time_t datetime;
  float f;
  float c;
} history[HISTORY_SIZE];

int history_n = 0;
int history_next = 0;
int history_first = 0;

const int H_INTERVAL = 60000 * SAMPLE_INTERVAL; // milliseconds
const char* HISTORY_FILE = "/historylog.txt";

void load_history() {
  Serial.println("Loading History from "); Serial.print(HISTORY_FILE); Serial.println("...");
  if (SPIFFS.exists(HISTORY_FILE)) {
    Serial.println(" - Found file");
    File f = SPIFFS.open(HISTORY_FILE, "r");
    if (!f) {
      Serial.print("- Open failed for "); Serial.println(HISTORY_FILE);
      history_loaded = true;
      return;
    }
    Serial.print("     SAMPLE_HOURS: "); Serial.println(SAMPLE_HOURS);
    Serial.print("  SAMPLE_INTERVAL: "); Serial.println(SAMPLE_INTERVAL);
    Serial.print("     HISTORY_SIZE: "); Serial.println(HISTORY_SIZE);
    Serial.print("SAMPLE_TIME_LIMIT: "); Serial.println(SAMPLE_TIME_LIMIT);
    Serial.print("       H_INTERVAL: "); Serial.println(H_INTERVAL);
    struct history_sample s;    
    time_t t = get_localtime();
    while(f.available() >= sizeof(history_sample) && history_n < HISTORY_SIZE) {
      int n = f.readBytes((char*)&s, sizeof(history_sample));
      // Serial.print(" - Sample "); Serial.print(history_n); Serial.print(", size: "); Serial.println(n);
      if (n < sizeof(history_sample)) {
        Serial.print(" - NOT ENOUGH BYTES FOR SAMPLE "); Serial.println(history_n);
        history_n--;
        break;
      }
      struct tm *tsample = localtime(&(s.datetime));
      // Serial.print("Compare ("); Serial.print(t); Serial.print(" - ");
      // Serial.print(s.datetime); Serial.print(": "); Serial.print(t - s.datetime); 
      // Serial.print(" To "); Serial.println(SAMPLE_TIME_LIMIT);
      if (t - s.datetime <= SAMPLE_TIME_LIMIT) {
        // Serial.print("Adding Sample: ");
        // Serial.println(make_timestamp(tsample));
        add_history_sample(s.datetime, s.f, s.c);
      } else {
        // Serial.print("Sample too old: ");
        // Serial.println(make_timestamp(tsample));
      }
    }
    f.close();
  } else {
    Serial.println(" - No history file");
  }
  history_loaded = true;
  save_history();
}

void save_history() {
  Serial.println("Saving History...");
  // Serial.print(" - sizeof sample: "); Serial.println(sizeof(history_sample));
  File f = SPIFFS.open(HISTORY_FILE, "w");
  // Serial.print(" - Opened file: 0x"); Serial.println(String((int)f, HEX));
  if (!f) {
    Serial.print("- Open failed for "); Serial.println(HISTORY_FILE);
    return;
  }
  for(int i = 0; i < history_n; i++) {
    // Serial.print(" - "); Serial.print(i);
    struct history_sample* hs = get_history_sample(i);
    int n = f.write((byte*)hs, sizeof(history_sample));
    // Serial.print(", Size: "); Serial.println(n);
    if (n != sizeof(history_sample)) break;
  }
  f.close();
}

void check_history() {
  if (temp_c == temp_f) return;
  if (!time_is_set) return;
  if (! history_loaded) load_history();
  if (! history_loaded) return;
  
  unsigned int nowt = millis();
  if (lastSampleTime == 0 || nowt - lastSampleTime > H_INTERVAL) {
    time_t t = get_localtime();
    struct tm *tx = localtime(&t);
    struct tm tnow = *tx;
    // Serial.print("check_history, minute: "); Serial.println(timeinfo->tm_min);
    if ((tnow.tm_min % 5) != 0) {
      // Serial.println(" - Minute is not a multiple of 5...");
      return;
    }
    if (history_n > 0) {
      struct tm *tlast = localtime(&(history[history_n - 1].datetime));
      // Serial.print("Comparing "); Serial.print(make_timestamp(tlast)); Serial.print(" to "); Serial.println(make_timestamp(&tnow));
      if (tlast->tm_min == tnow.tm_min && 
         tlast->tm_hour == tnow.tm_hour && 
         tlast->tm_mday == tnow.tm_mday && 
         tlast->tm_mon == tnow.tm_mon && 
         tlast->tm_year == tnow.tm_year) {
        // it's possible to reboot right after taking a sample which could result
        // in two samples at the same minute.
        // Serial.println("Last sample has same timestamp as new sample");
        return;
      }
    }
    lastSampleTime = nowt;
    String timestamp = make_timestamp(&tnow);
    Serial.print("SAMPLE AT "); Serial.println(timestamp);
    add_history();
  }
}

void add_history_sample(time_t datetime, float f, float c) {
  // Serial.print("add_history_sample: history_n = "); Serial.println(history_n);
  // Serial.print(", t: "); Serial.println(datetime);
  if (history_next == history_first && history_n > 0) {
    history_first++;
    if(history_first >= HISTORY_SIZE) history_first = 0;
  } else {
    history_n++;
  }
  history[history_next].c = c;
  history[history_next].f = f;
  history[history_next].datetime = datetime;
  history_next++;
  if (history_next >= HISTORY_SIZE) {
    history_next = 0;
  }
}

int convert_sample_index(int i) {
  int I = i + history_first;
  while (I >= HISTORY_SIZE) I -= HISTORY_SIZE;
  return I;  
}
struct history_sample* get_history_sample(int i) {
  // Serial.print("get_history_sample("); Serial.print(i); Serial.println(")");
  int I = convert_sample_index(i);
  // Serial.print(" get_history_sample: returning sample "); Serial.println(I);
  return (history + I);
}
void add_history() {
  get_temp();
  time_t t1 = get_localtime();
  struct tm *ti1 = localtime(&t1);
  ti1->tm_sec = 0;
  time_t t = mktime(ti1);

  add_history_sample(t, temp_f, temp_c);
  save_history();  
  
  // uint32_t free = system_get_free_heap_size();
  // Serial.print("MEM FREE: "); Serial.println(free);
  /***
  struct tm *ti = localtime(&t);
  String timestamp = make_timestamp(ti);
  char buffer[64];
  Serial.print("SAMPLE AT:  "); 
  Serial.println(sprintf(buffer, "%4d/%02d/%02d %02d:%02d:%02d", 
              ti->tm_year, ti->tm_mon, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec));
  File file = SPIFFS.open("/memlog.txt", "a");
  file.write(timestamp.c_str()); file.write(", ");
  file.write(String(free).c_str()); file.write("\n");
  file.close();
  */
}

float get_temp() {
  unsigned int nowt = millis();
  if (nowt - last_temp > 10000) {
    tempsensor.wake();   // wake up, ready to read!
    // Read and print out the temperature, also shows the resolution mode used for reading.
    // Serial.print("Resolution in mode: ");
    // Serial.println (tempsensor.getResolution());
    temp_c = tempsensor.readTempC();
    temp_f = tempsensor.readTempF();
    last_temp = nowt;
  }
}

float get_temp_f() {
  get_temp();
  return temp_f;
}

float get_temp_c() {
  get_temp();
  return temp_c;
}

void SetupTempSensor() {
  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example, also can be left in blank for default address use
  // Also there is a table with all addres possible for this sensor, you can connect multiple sensors
  // to the same i2c bus, just configure each sensor with a different address and define multiple objects for that
  //  A2 A1 A0 address
  //  0  0  0   0x18  this is the default address
  //  0  0  1   0x19
  //  0  1  0   0x1A
  //  0  1  1   0x1B
  //  1  0  0   0x1C
  //  1  0  1   0x1D
  //  1  1  0   0x1E
  //  1  1  1   0x1F
  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
    while (1);
  }
    
  Serial.println("Found MCP9808!");
  tempsensor.setResolution(3); // sets the resolution mode of reading, the modes are defined in the table bellow:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
}

String get_temp_json() {
  get_temp();
  String r = "{\"c\": " + String(temp_c, 1) + ", \"f\": " + String(temp_f, 1) + "}";
  // Serial.print("Temp JSON: ");
  // Serial.println(r);
  return r;
}

String get_history_json() {
  //Serial.println("get_history_json");
  //Serial.print(" - N: "); Serial.println(history_n);
  struct history_sample* hmax = 0;
  int max_i = 0;
  struct history_sample* hmin = 0;
  int min_i = 0;
  struct history_sample* current = 0;
  String r = "{\"history\": [";
  // Serial.println("> get_history_json: past setup");
  if (history_n > 0) {
    // Serial.println("> get_history_json: have samples");
    current = get_history_sample(history_n - 1);
    hmax = hmin = get_history_sample(0);
    for (int i = 1; i < history_n; i++) {
      struct history_sample* h = get_history_sample(i);
      if (h->f >= hmax->f) {
        hmax = h;
        max_i = i;
      }
      if (h->f < hmin->f) {
        hmin = h;
        min_i = i;
      }
    }
    for (int i = 0; i < history_n; i++) {
      // Serial.print("H: "); Serial.println(history[i].datetime);
      if (i > 0) {
        r += ",\n";
      }
      struct history_sample* H = get_history_sample(i);
      time_t t = H->datetime;
      struct tm *timeinfo = localtime(&t);
      String timestamp = make_timestamp(timeinfo);
      timestamp.remove(16); // timestamp.length() - 3
      r += "[\"" + timestamp  + "\", " 
        + String(current->f, 1)  + ", "
        + String(hmin->f, 1)  + ", "
        + String(hmax->f, 1)  + ", "
        + String(H->f, 1) + "]";
    }
  }
  // Serial.println("> get_history_json: past sample processing");
  r += "], \"max\": " + String(max_i);
  r += ", \"min\": " + String(min_i);
  r += "}";
  //Serial.println(r);
  return r;
}
