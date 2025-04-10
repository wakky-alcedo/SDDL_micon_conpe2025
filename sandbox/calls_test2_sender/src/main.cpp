#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>

// WiFi設定
const char* ssid = "SDDLnet";
const char* password = "smallbear";

// UDP設定
IPAddress receiverIP(192, 168, 0, 218);  // 受信機のIPアドレス
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

// デバッグ設定
#define DEBUG_AUDIO_VALUES 1              // 1: オーディオ値をシリアル出力する
#define DEBUG_SAMPLE_INTERVAL 50          // サンプルごとのデバッグ出力間隔
#define DEBUG_PACKETS 1                   // 1: パケット情報をシリアル出力する
#define DEBUG_VOLUME 1                    // 1: 音量情報をシリアル出力する
#define VOLUME_DISPLAY_BARS 20            // 音量表示のバーの数
#define VOLUME_DISPLAY_INTERVAL 500       // 音量表示の更新間隔（ミリ秒）

// グローバル変数
WiFiUDP udp;
int16_t audioBuffer[SAMPLES_PER_PACKET];  // オーディオサンプルを格納するバッファ
unsigned long packetCounter = 0;          // 送信パケットカウンタ
unsigned long lastPacketTime = 0;         // 最後にパケットを送信した時間
unsigned long lastVolumeDisplayTime = 0;  // 最後に音量を表示した時間

// 関数プロトタイプ宣言
void setupWiFi();
void setupI2S();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n======================================");
  Serial.println("I2S Microphone to UDP Audio Streamer (with Volume Display)");
  Serial.println("======================================");

  // WiFi接続
  setupWiFi();
  
  // I2S初期化
  setupI2S();
  
  Serial.println("Ready to capture and stream audio data");
}

void loop() {
  size_t bytesRead = 0;
  
  // I2Sからオーディオデータを読み込む
  esp_err_t result = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);
  
  if (result != ESP_OK) {
    Serial.printf("ERROR: i2s_read failed with code %d\n", result);
    delay(1000);
    return;
  }
  
  if (bytesRead > 0) {
    // 受信したサンプル数を計算
    int samplesRead = bytesRead / sizeof(int16_t);
    
    unsigned long currentTime = millis();
    unsigned long timeSinceLastPacket = currentTime - lastPacketTime;
    lastPacketTime = currentTime;
    packetCounter++;
    
    if (DEBUG_PACKETS) {
      Serial.printf("[PKT #%lu] Read %d samples (%d bytes), time since last: %lu ms\n", 
                    packetCounter, samplesRead, bytesRead, timeSinceLastPacket);
    }
    
    // データの統計情報と音量計算
    int16_t minVal = 32767, maxVal = -32768;
    long sum = 0;
    long sumOfSquares = 0;  // RMS計算用
    for (int i = 0; i < samplesRead; i++) {
      if (audioBuffer[i] < minVal) minVal = audioBuffer[i];
      if (audioBuffer[i] > maxVal) maxVal = audioBuffer[i];
      sum += audioBuffer[i];
      sumOfSquares += ((long)audioBuffer[i] * (long)audioBuffer[i]);
    }
    float avg = (float)sum / samplesRead;
    float rms = sqrt((float)sumOfSquares / samplesRead);  // RMS（二乗平均平方根）
    int peakToPeak = maxVal - minVal;                     // ピーク間値
    float dB = 20 * log10(rms / 32768.0f);                // dB計算（0dBは最大振幅）
    
    // 音量情報表示
    if (DEBUG_VOLUME && (currentTime - lastVolumeDisplayTime > VOLUME_DISPLAY_INTERVAL)) {
      lastVolumeDisplayTime = currentTime;
      Serial.printf("\n----- VOLUME INFO [PKT #%lu] -----\n", packetCounter);
      Serial.printf("RMS: %.2f (%.1f%% of max)\n", rms, (rms / 32768.0f) * 100.0f);
      Serial.printf("Peak-to-Peak: %d (%.1f%% of max)\n", peakToPeak, (peakToPeak / 65536.0f) * 100.0f);
      Serial.printf("dB (relative to full scale): %.1f dB\n", dB);
      
      // 音量バー表示
      Serial.print("Volume: [");
      int bars = (int)((rms / 32768.0f) * VOLUME_DISPLAY_BARS);
      for (int i = 0; i < VOLUME_DISPLAY_BARS; i++) {
        if (i < bars) {
          Serial.print("#");
        } else {
          Serial.print(" ");
        }
      }
      Serial.println("]");
      
      // 周波数分布の簡易表示（オプションとして実装可能）
      
      Serial.printf("Audio stats - Min: %d, Max: %d, Avg: %.2f, Range: %d\n", 
                    minVal, maxVal, avg, maxVal - minVal);
                    
      // 最初と最後の数サンプルの値を表示
      if (DEBUG_AUDIO_VALUES) {
        Serial.println("Sample values (16-bit PCM):");
        for (int i = 0; i < min(5, samplesRead); i++) {
          Serial.printf("  [%d]: %d\n", i, audioBuffer[i]);
        }
        
        if (samplesRead > 10) {
          Serial.println("  ...");
          for (int i = samplesRead - 5; i < samplesRead; i++) {
            Serial.printf("  [%d]: %d\n", i, audioBuffer[i]);
          }
        }
      }
    }
    
    // UDPパケットとして送信
    unsigned long sendStartTime = micros();
    udp.beginPacket(receiverIP, receiverPort);
    size_t bytesSent = udp.write((uint8_t*)audioBuffer, bytesRead);
    bool sendResult = udp.endPacket();
    unsigned long sendEndTime = micros();
    
    if (DEBUG_PACKETS) {
      Serial.printf("[PKT #%lu] Sent %d bytes in %lu us, result: %s\n", 
                    packetCounter, bytesSent, sendEndTime - sendStartTime, 
                    sendResult ? "SUCCESS" : "FAILED");
    }
  } else {
    Serial.println("WARNING: No data read from I2S!");
    delay(100);
  }
}

void setupWiFi() {
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    if (millis() - startTime > 20000) {
      Serial.println("\nWiFi connection timeout! Restarting...");
      ESP.restart();
    }
  }
  
  Serial.println();
  Serial.print("Connected successfully! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.print("Receiver IP set to: ");
  Serial.println(receiverIP.toString());
}

void setupI2S() {
  esp_err_t err;
  
  Serial.println("Configuring I2S...");
  
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
  Serial.println("Installing I2S driver...");
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
  Serial.println("Setting I2S pins...");
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while (1);
  }
  
  Serial.println("I2S initialized successfully with the following settings:");
  Serial.printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("  Bits per Sample: %d\n", SAMPLE_BITS);
  Serial.printf("  DMA Buffer Count: %d\n", DMA_BUF_COUNT);
  Serial.printf("  DMA Buffer Length: %d\n", DMA_BUF_LEN);
  Serial.printf("  SCK Pin: %d\n", I2S_MIC_SERIAL_CLOCK);
  Serial.printf("  WS Pin: %d\n", I2S_MIC_LEFT_RIGHT_CLOCK);
  Serial.printf("  SD Pin: %d\n", I2S_MIC_SERIAL_DATA);
  
  // I2Sが正常に動作しているか確認するためのテスト読み込み
  int16_t testBuffer[100];
  size_t bytesRead = 0;
  err = i2s_read(I2S_NUM_0, testBuffer, sizeof(testBuffer), &bytesRead, 500);
  
  if (err != ESP_OK) {
    Serial.printf("WARNING: Test read failed with error: %d\n", err);
  } else if (bytesRead == 0) {
    Serial.println("WARNING: Test read returned 0 bytes!");
  } else {
    Serial.printf("Test read successful: %d bytes read\n", bytesRead);
    
    // テスト読み込みの音量情報を表示
    int samplesRead = bytesRead / sizeof(int16_t);
    int16_t minVal = 32767, maxVal = -32768;
    long sumOfSquares = 0;
    for (int i = 0; i < samplesRead; i++) {
      if (testBuffer[i] < minVal) minVal = testBuffer[i];
      if (testBuffer[i] > maxVal) maxVal = testBuffer[i];
      sumOfSquares += ((long)testBuffer[i] * (long)testBuffer[i]);
    }
    float rms = sqrt((float)sumOfSquares / samplesRead);
    float dB = 20 * log10(rms / 32768.0f);
    
    Serial.println("Initial audio signal information:");
    Serial.printf("  RMS: %.2f (%.1f%% of max)\n", rms, (rms / 32768.0f) * 100.0f);
    Serial.printf("  Peak-to-Peak: %d\n", maxVal - minVal);
    Serial.printf("  dB: %.1f dB\n", dB);
    Serial.println("First few samples:");
    for (int i = 0; i < min(10, samplesRead); i++) {
      Serial.printf("  Sample[%d]: %d\n", i, testBuffer[i]);
    }
  }
}