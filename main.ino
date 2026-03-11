#include "network_sensors.h"
#include "dashboard.h"
#include "sdcard_module.h"
#include "firebase_module.h"

void setup() {

Serial.begin(115200);

connectWiFi();

initSDCard();

server.on("/", [](){
server.send(200,"text/html",htmlPage());
});

server.begin();

}

void loop() {

server.handleClient();

logToSD();

uploadToFirebase();

delay(5000);

}