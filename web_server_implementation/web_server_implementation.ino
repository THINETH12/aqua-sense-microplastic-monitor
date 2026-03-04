#include <WiFi.h>
#include <WebServer.h>

#define WIFI_SSID "Janithr"
#define WIFI_PASSWORD "sparrow@ucsc"

WebServer server(80);

float temperature = 25.0;
float ph = 7.0;
float wqi = 85.0;

void handleRoot() {
  server.send(200, "text/html",
  "<h1>Water Monitoring</h1>"
  "<p>Temperature: 25 C</p>"
  "<p>pH: 7.0</p>"
  "<p>WQI: 85</p>");
}

void handleAPI() {
  String json = "{";
  json += "\"temperature\":25,";
  json += "\"ph\":7,";
  json += "\"wqi\":85";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/api", handleAPI);
  server.begin();
}

void loop() {
  server.handleClient();
}
