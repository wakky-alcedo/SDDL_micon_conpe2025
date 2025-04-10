#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/dac.h>

// WiFi設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// UDP設定
const int localPort = 12345;  // 送信側と同じポート番号

// オーディオ設定
#define SAMPLE_RATE 16000                 // サンプリングレート (Hz)
#define SAMPLE_BITS 16                    // サンプルあたりのビット数
#define SAMPLES_PER_PACKET 512            // パケットあたりのサンプル数
#define SPEAKER_PIN 26                    // DACスピーカーピン

// ESP32 DAC出力設定
#define DAC_CHANNEL DAC_CHANNEL_2         // GPIO26はDAC_CHANNEL_2に対応

// グローバル変数
WiFiUDP udp;
uint8_t packetBuffer[SAMPLES_PER_PACKET * sizeof(int16_t)]; // 受信バッファ
int16_t audioBuffer[SAMPLES_PER_PACKET];  // オーディオサンプルバッファ

// 関数プロトタイプ宣言
void setupWiFi();
void setupUDP();
void setupDAC();
void playAudio(int sampleCount);

void setup() {
  Serial.begin(115200);
  Serial.println("UDP Audio Receiver with DAC Speaker Output");

  // WiFi接続
  setupWiFi();
  
  // UDP初期化
  setupUDP();
  
  // DAC初期化
  setupDAC();
}

void loop() {
  // UDPパケットを受信
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    // Serial.printf("Received %d bytes from %s, port %d\n", 
    //               packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    
    // パケットを読み込む
    int len = udp.read(packetBuffer, sizeof(packetBuffer));
    if (len > 0) {
      // バイトデータをaudioBufferに変換
      memcpy(audioBuffer, packetBuffer, len);
      
      // サンプル数を計算
      int samplesReceived = len / sizeof(int16_t);
      Serial.printf("Playing %d samples\n", samplesReceived);
      
      // DACを通じてオーディオを再生
      playAudio(samplesReceived);
    }
  }
}

void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void setupUDP() {
  Serial.printf("Setting up UDP listener on port %d\n", localPort);
  udp.begin(localPort);
}

void setupDAC() {
  // DACチャンネルを初期化
  dac_output_enable(DAC_CHANNEL);
  Serial.printf("DAC initialized on pin %d\n", SPEAKER_PIN);
}

void playAudio(int sampleCount) {
  // サンプルをDACに出力
  for (int i = 0; i < sampleCount; i++) {
    // 16ビットのPCM値を8ビットのDAC値に変換 (0-255)
    uint8_t dacValue = map(audioBuffer[i], -32768, 32767, 0, 255);
    
    // DACに出力
    dac_output_voltage(DAC_CHANNEL, dacValue);
    
    // サンプリングレートに合わせて遅延
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
}