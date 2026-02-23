#include <SPI.h>
#include <SD.h>

#define SD_CS 5

void setup() {
  Serial.begin(115200);

  SPI.begin(18, 19, 23, 5);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Failed!");
    return;
  }

  Serial.println("SD Card OK!");

  File file = SD.open("/test.txt", FILE_WRITE);

  if (file) {
    file.println("ESP32 SD Test");
    file.close();
    Serial.println("File Written!");
  }
}

void loop() {
}