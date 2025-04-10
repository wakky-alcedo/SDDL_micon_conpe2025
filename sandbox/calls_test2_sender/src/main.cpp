#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>

// WiFi設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// UDP設定
IPAddress receiverIP(192, 168, 0, 237);  // 受信機のIPアドレス
const int receiverPort = 12345;           // 受信機のポート番号
// const int senderPort = 54321;

// I2S設定
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT  // モノラルマイクを使用
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_26           // I2S SCK
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_22       // I2S WS (Word Select)
#define I2S_MIC_SERIAL_DATA GPIO_NUM_21            // I2S SD (マイクからのデータ入力)

// オーディオバッファ設定
#define SAMPLE_RATE 16000                 // サンプリングレート (Hz)
#define SAMPLE_BITS 16                    // サンプルあたりのビット数
#define DMA_BUF_COUNT 8                   // DMAバッファの数
#define DMA_BUF_LEN 1024                  // DMAバッファの長さ
#define SAMPLES_PER_PACKET 512            // パケットあたりのサンプル数

// グローバル変数
WiFiUDP udp;
int16_t audioBuffer[SAMPLES_PER_PACKET];  // オーディオサンプルを格納するバッファ

// 関数プロトタイプ宣言
void setupWiFi();
void setupI2S();

void setup() {
  Serial.begin(115200);
  Serial.println("I2S Microphone to UDP Audio Streamer");

  // WiFi接続
  setupWiFi();
  
  // I2S初期化
  setupI2S();
}

void loop() {
  size_t bytesRead = 0;
  
  // I2Sからオーディオデータを読み込む
  i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);
  
  if (bytesRead > 0) {
    // 受信したサンプル数を計算
    int samplesRead = bytesRead / sizeof(int16_t);
    // Serial.printf("Read %d samples\n", samplesRead);

    // 受信したサンプルをデバッグ出力
    for (int i = 0; i < samplesRead; i++) {
      Serial.printf("%d ", audioBuffer[i]);
    }
    Serial.println();
    
    // UDPパケットとして送信
    udp.beginPacket(receiverIP, receiverPort);
    udp.write((uint8_t*)audioBuffer, bytesRead);
    udp.endPacket();
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

void setupI2S() {
  esp_err_t err;
  
  // I2S設定
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_MIC_CHANNEL,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  // I2Sドライバーをインストール
  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    while (1);
  }
  
  // I2Sピン設定
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,  // 出力ピンは使用しない
    .data_in_num = I2S_MIC_SERIAL_DATA
  };
  
  // I2Sピンを設定
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while (1);
  }
  
  Serial.println("I2S initialized successfully");
}