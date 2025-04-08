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

// prototype
void setup();
void loop();
uint8_t encodeMuLaw(int16_t sample);

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
}


void loop() {
  // 送信処理
  for (int i = 0; i < samplesPerPacket; i++) {
    // int rawValue = analogRead(micPin);
    // 12ビットのADC値を16ビットの符号付き整数にマッピング
    pcmValue = map(analogRead(micPin), 0, 4095, -32768, 32767);
    
    // μ-law圧縮を適用（16bit PCMから8bitへ圧縮）
    // pcmBuffer[i] = encodeMuLaw(map(analogRead(micPin), 0, 4095, -32768, 32767));
    // 符号ビットを取得
    // μ-law圧縮 - ステップ1: 符号を取得
    pcmBuffer[i] = (pcmValue < 0) ? 0 : 0x80;
    
    // ステップ2: 絶対値を取得
    pcmValue = (pcmValue < 0) ? -pcmValue : pcmValue;
    
    // ステップ3: クリッピング
    pcmValue = (pcmValue > 32767) ? 32767 : pcmValue;
    
    // ステップ4: バイアスを加える
    pcmValue = pcmValue + 132;
    
    // ステップ5: 指数部の計算
    if (pcmValue >= 32768) {
      pcmBuffer[i] |= (7 << 4) | (((pcmValue >> 7) - 128) & 0x0F);
    } else if (pcmValue >= 16384) {
      pcmBuffer[i] |= (6 << 4) | (((pcmValue >> 6) - 128) & 0x0F);
    } else if (pcmValue >= 8192) {
      pcmBuffer[i] |= (5 << 4) | (((pcmValue >> 5) - 128) & 0x0F);
    } else if (pcmValue >= 4096) {
      pcmBuffer[i] |= (4 << 4) | (((pcmValue >> 4) - 128) & 0x0F);
    } else if (pcmValue >= 2048) {
      pcmBuffer[i] |= (3 << 4) | (((pcmValue >> 3) - 128) & 0x0F);
    } else if (pcmValue >= 1024) {
      pcmBuffer[i] |= (2 << 4) | (((pcmValue >> 2) - 128) & 0x0F);
    } else if (pcmValue >= 512) {
      pcmBuffer[i] |= (1 << 4) | (((pcmValue >> 1) - 128) & 0x0F);
    } else {
      pcmBuffer[i] |= (0 << 4) | ((pcmValue - 128) & 0x0F);
    }
    
    // ステップ6: μ-law仕様に従って反転
    pcmBuffer[i] = ~pcmBuffer[i];

    delayMicroseconds(1000000 / sampleRate);
  }

  // パケットサイズが半分になる（各サンプルが1バイトになるため）
  int compressedPacketSize = samplesPerPacket;
  udp.beginPacket(receiverIP, receiverPort);
  udp.write(pcmBuffer, compressedPacketSize);
  udp.endPacket();
  // Serial.printf("Sent %d bytes (PCM)\n", packetSize);
  // ヒープメモリの使用量を確認
  Serial.printf("Free heap: %d, Min free heap: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  // delay(10); // 10msの遅延を追加
}

uint8_t exponent = 0;
// μ-law圧縮関数
uint8_t encodeMuLaw(int16_t sample) {
  // 符号ビットを取得
  // uint8_t sign = (sample < 0) ? 0 : 0x80;
  
  // 絶対値を取得
  if (sample < 0) {
    sample = -sample;
  }
  
  // サンプルをクリップ
  if (sample > 32767) {
    sample = 32767;
  }
  
  // バイアスをかける (μ-law式に従って)
  sample = sample + 132;
  
  // 上位ビットを検出してエンコード
  exponent = 0;
  for (int i = 0; i < 8; i++) {
    if (sample >= 256) {
      sample >>= 1;
      exponent++;
    }
  }
  
  // uint8_t mantissa = ((sample - 128) >> (exponent)) & 0x0F;
  // uint8_t mulaw = ~(sign | (exponent << 4) | mantissa);
  
  // return mulaw
  return ~(((sample < 0) ? 0 : 0x80) | (exponent << 4) | ((sample - 128) >> (exponent)) & 0x0F);
}