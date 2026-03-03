#include <SPI.h>
#include <SD.h>

#define SD_CS 5
const char* LOG_PATH = "/water_log.csv";

bool ensureLogHeader() {
  if (!SD.exists(LOG_PATH)) {
    File f = SD.open(LOG_PATH, FILE_WRITE);
    if (!f) return false;
    f.println("timestamp,temp,ph,ec,turbidity,battery,wqi");
    f.close();
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  if (SD.begin(SD_CS)) {
    Serial.println("SD Card OK");
    ensureLogHeader();
  } else {
    Serial.println("SD Card Failed");
  }
}

void loop() {
  File f = SD.open(LOG_PATH, FILE_APPEND);
  if (f) {
    f.println("1000,25.5,7.1,300,3.2,4.0,85");
    f.close();
    Serial.println("Logged to SD");
  }
  delay(5000);
}