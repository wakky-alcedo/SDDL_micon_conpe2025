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

// デバッグ設定
#define DEBUG_AUDIO_VALUES 1              // 1: オーディオ値をシリアル出力する
#define DEBUG_SAMPLE_INTERVAL 10          // サンプルごとのデバッグ出力間隔
#define DEBUG_PACKETS 1                   // 1: パケット情報をシリアル出力する
#define DEBUG_VOLUME 1                    // 1: 音量情報をシリアル出力する
#define VOLUME_DISPLAY_BARS 20            // 音量表示のバーの数
#define VOLUME_DISPLAY_INTERVAL 500       // 音量表示の更新間隔（ミリ秒）

// ESP32 DAC出力設定
#define DAC_CHANNEL DAC_CHANNEL_2         // GPIO26はDAC_CHANNEL_2に対応

// グローバル変数
WiFiUDP udp;
uint8_t packetBuffer[SAMPLES_PER_PACKET * sizeof(int16_t)]; // 受信バッファ
int16_t audioBuffer[SAMPLES_PER_PACKET];  // オーディオサンプルバッファ
unsigned long packetCounter = 0;          // 受信パケットカウンタ
unsigned long lastPacketTime = 0;         // 最後にパケットを受信した時間
unsigned long lastVolumeDisplayTime = 0;  // 最後に音量を表示した時間

// 関数プロトタイプ宣言
void setupWiFi();
void setupUDP();
void setupDAC();
void generateTestTone();
void playAudio(int sampleCount);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n======================================");
  Serial.println("UDP Audio Receiver with DAC Speaker Output (with Volume Display)");
  Serial.println("======================================");

  // WiFi接続
  setupWiFi();
  
  // UDP初期化
  setupUDP();
  
  // DAC初期化
  setupDAC();
  
  Serial.println("Ready to receive audio data");
}

void loop() {
  // UDPパケットを受信
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    unsigned long currentTime = millis();
    unsigned long timeSinceLastPacket = currentTime - lastPacketTime;
    lastPacketTime = currentTime;
    packetCounter++;
    
    if (DEBUG_PACKETS) {
      Serial.printf("[PKT #%lu] Received %d bytes from %s, port %d, time since last: %lu ms\n", 
                    packetCounter, packetSize, udp.remoteIP().toString().c_str(), 
                    udp.remotePort(), timeSinceLastPacket);
    }
    
    // パケットを読み込む
    int len = udp.read(packetBuffer, sizeof(packetBuffer));
    if (len > 0) {
      // バイトデータをaudioBufferに変換
      memcpy(audioBuffer, packetBuffer, len);
      
      // サンプル数を計算
      int samplesReceived = len / sizeof(int16_t);
      
      if (DEBUG_PACKETS) {
        Serial.printf("[PKT #%lu] Playing %d samples (expected: %d)\n", 
                      packetCounter, samplesReceived, SAMPLES_PER_PACKET);
      }
      
      // 音量情報の計算と表示
      int16_t minVal = 32767, maxVal = -32768;
      long sumOfSquares = 0;  // RMS計算用
      for (int i = 0; i < samplesReceived; i++) {
        if (audioBuffer[i] < minVal) minVal = audioBuffer[i];
        if (audioBuffer[i] > maxVal) maxVal = audioBuffer[i];
        sumOfSquares += ((long)audioBuffer[i] * (long)audioBuffer[i]);
      }
      float rms = sqrt((float)sumOfSquares / samplesReceived);  // RMS（二乗平均平方根）
      int peakToPeak = maxVal - minVal;                         // ピーク間値
      float dB = 20 * log10(rms / 32768.0f);                    // dB計算（0dBは最大振幅）
      
      // 音量バーと情報表示
      if (DEBUG_VOLUME && (currentTime - lastVolumeDisplayTime > VOLUME_DISPLAY_INTERVAL)) {
        lastVolumeDisplayTime = currentTime;
        Serial.printf("\n----- RECEIVED AUDIO VOLUME [PKT #%lu] -----\n", packetCounter);
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
        
        // 16進数ダンプ表示（最初の数バイト）
        Serial.println("Data hexdump (first 32 bytes):");
        for (int i = 0; i < min(32, len); i++) {
          Serial.printf("%02X ", packetBuffer[i]);
          if ((i + 1) % 16 == 0) Serial.println();
        }
        Serial.println();
        
        // 最初と最後の数サンプルの値を表示
        if (DEBUG_AUDIO_VALUES) {
          Serial.println("Sample values (16-bit PCM -> DAC):");
          for (int i = 0; i < min(5, samplesReceived); i++) {
            Serial.printf("  [%d]: %d -> DAC: %d\n", i, audioBuffer[i], 
                          map(audioBuffer[i], -32768, 32767, 0, 255));
          }
          
          if (samplesReceived > 10) {
            Serial.println("  ...");
            for (int i = samplesReceived - 5; i < samplesReceived; i++) {
              Serial.printf("  [%d]: %d -> DAC: %d\n", i, audioBuffer[i], 
                            map(audioBuffer[i], -32768, 32767, 0, 255));
            }
          }
        }
      }
      
      // ノイズ検出
      if (peakToPeak < 100 && rms < 50) {
        Serial.println("WARNING: Very low audio signal detected - possible noise only!");
      } else if (peakToPeak > 65000 && rms > 30000) {
        Serial.println("WARNING: Very high audio signal detected - possible clipping!");
      }
      
      // DACを通じてオーディオを再生
      playAudio(samplesReceived);
    }
  } else {
    // パケットが来ない状態が続く場合
    if (lastPacketTime > 0 && millis() - lastPacketTime > 5000) {
      Serial.println("WARNING: No packets received for 5 seconds!");
      lastPacketTime = millis(); // 警告メッセージの連続表示を防ぐ
    }
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
}

void setupUDP() {
  Serial.printf("Setting up UDP listener on port %d\n", localPort);
  if (udp.begin(localPort)) {
    Serial.println("UDP server started successfully");
  } else {
    Serial.println("ERROR: Failed to start UDP server!");
  }
}

void setupDAC() {
  // DACチャンネルを初期化
  dac_output_enable(DAC_CHANNEL);
  Serial.printf("DAC initialized on pin %d (DAC_CHANNEL_%d)\n", 
                SPEAKER_PIN, DAC_CHANNEL == DAC_CHANNEL_1 ? 1 : 2);
                
  // 初期状態で中央値を出力して確認
  dac_output_voltage(DAC_CHANNEL, 128);
  Serial.println("DAC test: output set to 128 (midpoint)");
  
  // テスト信号を出力（オプション）
  Serial.println("Generating test tone on DAC...");
  generateTestTone();
}

// 簡単なテスト信号を生成（DACが正常に動作しているか確認）
void generateTestTone() {
  // 1kHzの正弦波を生成 (短時間)
  const int samples = 32;
  const float frequency = 1000.0;  // 1kHz
  const int duration = 500;        // 0.5秒
  
  Serial.println("Playing 1kHz test tone...");
  unsigned long startTime = millis();
  
  while (millis() - startTime < duration) {
    for (int i = 0; i < samples; i++) {
      // 0-255の正弦波を生成
      uint8_t value = 128 + 127 * sin(2 * PI * i / samples);
      dac_output_voltage(DAC_CHANNEL, value);
      delayMicroseconds(1000000 / (frequency * samples));
    }
  }
  
  // 終了時に中央値に戻す
  dac_output_voltage(DAC_CHANNEL, 128);
  Serial.println("Test tone completed");
}

void playAudio(int sampleCount) {
  // 再生開始時間を記録
  unsigned long startPlayTime = micros();
  
  // サンプルごとの音量累積値（デバッグ用）
  long sumOfSquares = 0;
  
  // サンプルをDACに出力
  for (int i = 0; i < sampleCount; i++) {
    // 16ビットのPCM値を8ビットのDAC値に変換 (0-255)
    uint8_t dacValue = map(audioBuffer[i], -32768, 32767, 0, 255);
    
    // DACに出力
    dac_output_voltage(DAC_CHANNEL, dacValue);
    
    // 音量計算用
    sumOfSquares += ((long)audioBuffer[i] * (long)audioBuffer[i]);
    
    // デバッグログ
    if (DEBUG_AUDIO_VALUES && i % DEBUG_SAMPLE_INTERVAL == 0) {
      Serial.printf("Sample[%d]: PCM=%d, DAC=%d\n", i, audioBuffer[i], dacValue);
    }
    
    // サンプリングレートに合わせて遅延
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }
  
  // 再生完了時間を記録
  unsigned long endPlayTime = micros();
  unsigned long playDuration = endPlayTime - startPlayTime;
  
  // 再生中の平均音量を計算
  float playbackRms = sqrt((float)sumOfSquares / sampleCount);
  
  if (DEBUG_PACKETS) {
    float actualSampleRate = (float)sampleCount * 1000000.0f / playDuration;
    Serial.printf("[PKT #%lu] Playback completed - duration: %lu us, actual sample rate: %.2f Hz, RMS: %.2f\n", 
                  packetCounter, playDuration, actualSampleRate, playbackRms);
  }
}