#ifndef SDCARD_MODULE_H
#define SDCARD_MODULE_H

#include <SPI.h>
#include <SD.h>

// SD CARD PINS
#define SD_CS    10
#define SD_MOSI  11
#define SD_SCK   12
#define SD_MISO  13

SPIClass sdSPI(FSPI);

void initSDCard(){

sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

if(!SD.begin(SD_CS,sdSPI))
{
Serial.println("SD card failed");
return;
}

Serial.println("SD card initialized");

}

void logToSD(){

File file = SD.open("/water_log.csv",FILE_APPEND);

if(file){

file.print(millis());
file.print(",");
file.print(temperatureC);
file.print(",");
file.print(phValue);
file.print(",");
file.print(ecValue);
file.print(",");
file.println(turbidityNTU);

file.close();

}

}

#endif
