#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const char* receiverIP = "192.168.0.237"; // 受信側のESP32のIPアドレス
const int receiverPort = 12345;
const int senderPort = 54321;

WiFiUDP udp;
const char* messageToSend = "Hello from ESP32 #1!";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  udp.begin(senderPort);
  Serial.printf("UDP sender started on port %d\n", senderPort);
}

void loop() {
  udp.beginPacket(receiverIP, receiverPort);
  udp.print(messageToSend);
  udp.endPacket();
  Serial.println("Message sent");
  delay(1000); // 1秒ごとに送信
}