#include <Arduino.h>

// アナログマイク接続ピン (ESP32 ADCピン)
const int micPin = 34; // 例: GPIO34

// スピーカー接続ピン (ESP32 DACピン)
const int speakerPin = 25; // 例: GPIO25 (DAC1)

// サンプリングレート (Hz)
const int sampleRate = 8000;

// バッファサイズ (サンプルの個数)
const int bufferSize = 256;

void setup() {
  Serial.begin(115200);
  Serial.println("Audio Passthrough Test");
  // DACの初期設定 (必要に応じて)
  dacWrite(speakerPin, 0); // 初期値を0に設定
}

void loop() {
  // アナログマイクからデータを読み取る
  for (int i = 0; i < bufferSize; i++) {
    int rawValue = analogRead(micPin);

    // 12ビットのADC値を8ビットのDACの範囲 (0-255) にマッピング
    // 必要に応じて、オフセット調整やスケール調整を行ってください
    int dacValue = map(rawValue, 0, 4095, 0, 255);

    // DACに書き込んで音を出す
    dacWrite(speakerPin, dacValue);

    // サンプリングレートに合わせて遅延を入れる
    delayMicroseconds(1000000 / sampleRate);
  }
}