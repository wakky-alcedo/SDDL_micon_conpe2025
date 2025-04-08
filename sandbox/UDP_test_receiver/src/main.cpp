#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const int receiverPort = 12345;

WiFiUDP udp;
char packetBuffer[256];

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  udp.begin(receiverPort);
  Serial.printf("UDP receiver started on port %d\n", receiverPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.printf("Received %d bytes from %s:%d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
      Serial.printf("UDP packet contents: %s\n", packetBuffer);
    }
  }
  delay(1);
}