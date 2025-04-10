#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/dac.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const int receiverPort = 12345;

WiFiUDP udp;
IPAddress senderIP;
int senderPortNumber;

const int bitsPerSample = 16;
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 128;
const int packetSize = samplesPerPacket * bytesPerSample;
static uint8_t pcmBuffer[packetSize];

// DAC設定
const int dacPin = 26; // DAC出力ピン (GPIO25 または GPIO26)
const int sampleRate = 32000; // 送信側と一致させる
const int numChannels = 1; // モノラル (送信側と一致させる)

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 UDP Receiver with Internal DAC");

  // WiFi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // UDP初期化
  udp.begin(receiverPort);
  Serial.printf("UDP receiver started on port %d\n", receiverPort);

  // DAC初期化
  dac_output_enable(DAC_CHANNEL_1); // DACチャンネル1 (GPIO25) を有効化
  // DACチャンネル2 (GPIO26) を使用する場合は dac_output_enable(DAC_CHANNEL_2); を使用
}

void loop() {
  int packetSizeReceived = udp.parsePacket();
  if (packetSizeReceived == packetSize) {
    // パケットを受信
    udp.read(pcmBuffer, packetSize);
    senderIP = udp.remoteIP();
    senderPortNumber = udp.remotePort();
    // Serial.printf("Received %d bytes from %s:%d\n", packetSizeReceived, senderIP.toString().c_str(), senderPortNumber);

    // PCMデータをDACに書き込む
    for (int i = 0; i < samplesPerPacket; i++) {
      int16_t pcmValue = (int16_t)((pcmBuffer[i * 2 + 1] << 8) | pcmBuffer[i * 2]); // リトルエンディアンから16ビット整数へ

      // 16ビットのPCM値を0-255のDAC出力範囲に変換 (符号付きを符号なしへシフト)
      int dacValue = map(pcmValue, -32768, 32767, 0, 255);
      dac_output_voltage(DAC_CHANNEL_1, dacValue); // DACチャンネル1に出力
      // DACチャンネル2に出力する場合は dac_output_voltage(DAC_CHANNEL_2, dacValue);
      delayMicroseconds(1000000 / sampleRate); // サンプリングレートに基づいた遅延
    }
  } else if (packetSizeReceived > 0) {
    // 受信したパケットサイズが期待されるサイズと異なる場合
    Serial.printf("Received unexpected packet size: %d\n", packetSizeReceived);
    // 受信バッファをクリア
    while (udp.available()) {
      udp.read();
    }
  }
  delay(1); // CPU負荷軽減
}