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

// prototype
void setup();
void loop();
int16_t decodeMuLaw(uint8_t mulawbyte);

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
    // 受信処理
    int compressedPacketSize = samplesPerPacket;  // 各サンプルが1バイト
    udp.read(pcmBuffer, compressedPacketSize);

    // 受信したμ-lawデータをPCMに戻してDACに出力
    for (int i = 0; i < samplesPerPacket; i++) {
      // μ-law圧縮されたデータを復号
      int16_t pcmValue = decodeMuLaw(pcmBuffer[i]);
      
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

// μ-law復号関数
int16_t decodeMuLaw(uint8_t mulawbyte) {
  // まず、μ-lawの仕様に従って反転
  mulawbyte = ~mulawbyte;
  
  // 符号ビットを取得
  int sign = (mulawbyte & 0x80) ? 1 : -1;
  
  // 指数部と仮数部を抽出
  int exponent = (mulawbyte >> 4) & 0x07;
  int mantissa = mulawbyte & 0x0F;
  
  // サンプル値を計算
  int16_t sample = (((mantissa << 3) + 132) << exponent) - 132;
  
  // 符号を適用
  return sign * sample;
}