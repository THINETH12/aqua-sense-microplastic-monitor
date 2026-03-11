#ifndef NETWORK_SENSORS_H
#define NETWORK_SENSORS_H

#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WIFI
const char* ssid = "Janithr";
const char* password = "sparrow@ucsc";

// SENSOR PINS
#define ONE_WIRE_BUS   4
#define TURBIDITY_PIN  1
#define EC_PIN         2
#define PH_PIN         3

WebServer server(80);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// GLOBAL SENSOR VALUES
float temperatureC = 0.0;
float phValue = 0.0;
float ecValue = 0.0;
float turbidityNTU = 0.0;
float wqiScore = 0.0;

String waterStatus = "Unknown";
String usability = "Checking";

// WIFI CONNECTION
void connectWiFi() {

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

#endif
