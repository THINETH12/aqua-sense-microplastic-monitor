#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS  4
#define PH_PIN        34
#define EC_PIN        35
#define TURB_PIN      32
#define BATTERY_PIN   33

#define ADC_MAX 4095.0
#define VREF 3.3
#define BATTERY_DIVIDER 2.0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

float temperature, phValue, ecValue, turbidity, batteryVoltage, WQI;

float readBattery() {
  int raw = analogRead(BATTERY_PIN);
  return (raw / ADC_MAX) * VREF * BATTERY_DIVIDER;
}

float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / ADC_MAX) * VREF;
  return 7 + ((2.5 - voltage) / 0.18);
}

float readEC() {
  int raw = analogRead(EC_PIN);
  float voltage = (raw / ADC_MAX) * VREF;
  return voltage * 1000;
}

float readTurbidity() {
  int raw = analogRead(TURB_PIN);
  float voltage = (raw / ADC_MAX) * VREF;
  return voltage * 300;
}

float calculateWQI() {
  float w_temp = 0.20;
  float w_ph   = 0.30;
  float w_ec   = 0.25;
  float w_turb = 0.25;

  float q_temp = 100 - abs(temperature - 20) * 4;
  float q_ph   = 100 - abs(phValue - 7) * 20;
  float q_ec   = (ecValue >= 150 && ecValue <= 500) ? 100 : 60;
  float q_turb = (turbidity <= 5) ? 100 : 50;

  if (q_temp < 0) q_temp = 0;
  if (q_ph < 0) q_ph = 0;

  return (q_temp*w_temp + q_ph*w_ph + q_ec*w_ec + q_turb*w_turb);
}

void setup() {
  Serial.begin(115200);
  ds18b20.begin();
}

void loop() {
  ds18b20.requestTemperatures();
  temperature = ds18b20.getTempCByIndex(0);
  phValue = readPH();
  ecValue = readEC();
  turbidity = readTurbidity();
  batteryVoltage = readBattery();
  WQI = calculateWQI();

  Serial.println("Sensors + WQI Working");
  delay(5000);
}