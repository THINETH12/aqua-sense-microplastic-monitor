#include "sensors.h"
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4
#define PH_PIN       34
#define EC_PIN        35
#define TURB_PIN      32
#define BATTERY_PIN   33
#define ADC_MAX       4095.0
#define VREF          3.3
#define BATTERY_DIVIDER 2.0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

SensorData data;

void readSensors() {
  ds18b20.requestTemperatures();
  data.temperature = ds18b20.getTempCByIndex(0);

  int rawPH = analogRead(PH_PIN);
  data.ph = 7 + ((2.5 - (rawPH / ADC_MAX) * VREF) / 0.18);

  int rawEC = analogRead(EC_PIN);
  data.ec = (rawEC / ADC_MAX) * VREF * 1000;

  int rawTurb = analogRead(TURB_PIN);
  data.turbidity = (rawTurb / ADC_MAX) * VREF * 300;

  int rawBat = analogRead(BATTERY_PIN);
  data.battery = (rawBat / ADC_MAX) * VREF * BATTERY_DIVIDER;

  data.wqi = calculateWQI(data);
}

SensorData getSensorData() {
  return data;
}