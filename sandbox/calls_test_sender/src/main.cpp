#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const char* receiverIP = "192.168.0.237"; // 受信側のESP32のIPアドレス
const int receiverPort = 12345;
const int senderPort = 54321;

WiFiUDP udp;

const int micPin = 34;
const int sampleRate = 8000;
const int bitsPerSample = 16; // 1サンプルあたり16ビット
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 16; // 1パケットあたりのサンプル数
const int packetSize = samplesPerPacket * bytesPerSample; // パケットサイズ (バイト)

int16_t pcmValue;
static uint8_t pcmBuffer[packetSize];

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  udp.begin(senderPort);
  Serial.printf("UDP sender started on port %d\n", senderPort);
  // for (int i = 0; i < samplesPerPacket; i++) {
  //   pcmValue = (i % 256) - 128; // テスト用のデータ
  //   pcmBuffer[i * 2] = (pcmValue >> 0) & 0xFF;     // 下位バイト
  //   pcmBuffer[i * 2 + 1] = (pcmValue >> 8) & 0xFF; // 上位バイト
  //   delayMicroseconds(1000000 / sampleRate);
  // }
}

// int16_t pcmValue;
// static uint8_t pcmBuffer[packetSize];
void loop() {
  // static uint8_t pcmBuffer[packetSize];
  for (int i = 0; i < samplesPerPacket; i++) {
    int rawValue = analogRead(micPin);
    // 12ビットのADC値を16ビットの符号付き整数にマッピング
    pcmValue = map(rawValue, 0, 4095, -32768, 32767);
    pcmBuffer[i * 2] = (pcmValue >> 0) & 0xFF;     // 下位バイト
    pcmBuffer[i * 2 + 1] = (pcmValue >> 8) & 0xFF; // 上位バイト
    delayMicroseconds(1000000 / sampleRate);
  }
  // 一旦コメントアウトして、テスト用のデータを送信
  // for (int i = 0; i < samplesPerPacket; i++) {
  //   pcmValue = (i % 256) - 128; // テスト用のデータ
  //   pcmBuffer[i * 2] = (pcmValue >> 0) & 0xFF;     // 下位バイト
  //   pcmBuffer[i * 2 + 1] = (pcmValue >> 8) & 0xFF; // 上位バイト
  //   delayMicroseconds(1000000 / sampleRate);
  // }

  udp.beginPacket(receiverIP, receiverPort);
  udp.write(pcmBuffer, packetSize);
  udp.endPacket();
  // Serial.printf("Sent %d bytes (PCM)\n", packetSize);
  // ヒープメモリの使用量を確認
  Serial.printf("Free heap: %d, Min free heap: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  // delay(10); // 10msの遅延を追加
}