#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const int receiverPort = 12345;

WiFiUDP udp;
const int speakerPin = 26;
const int sampleRate = 8000;
const int bitsPerSample = 16;
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 16;
const int packetSize = samplesPerPacket * bytesPerSample;
byte pcmBuffer[packetSize];

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  udp.begin(receiverPort);
  Serial.printf("UDP receiver started on port %d\n", receiverPort);
  dacWrite(speakerPin, 0);
}

void loop() {
  int packetSizeReceived = udp.parsePacket();
  if (packetSizeReceived == packetSize) {
    udp.read(pcmBuffer, packetSize);
    // 受信したPCMデータをDACに出力
    for (int i = 0; i < samplesPerPacket; i++) {
      int16_t pcmValue = (pcmBuffer[i * 2 + 1] << 8) | pcmBuffer[i * 2];
      // 16ビットのPCM値を8ビットのDACの範囲 (0-255) にマッピング
      int dacValue = map(pcmValue, -32768, 32767, 0, 255);
      dacWrite(speakerPin, dacValue);
      delayMicroseconds(1000000 / sampleRate);
    }
    Serial.printf("Received packet of size %d\n", packetSizeReceived);
  } else if (packetSizeReceived > 0) {
    Serial.printf("Received unexpected packet size: %d\n", packetSizeReceived);
  }
  delay(1);
}