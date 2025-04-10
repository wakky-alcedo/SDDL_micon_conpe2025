#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>

#include <driver/i2s.h>

const char* ssid = "SDDLnet";
const char* password = "smallbear";
const char* receiverIP = "192.168.0.237"; // 受信側のESP32のIPアドレス
const int receiverPort = 12345;
const int senderPort = 54321;

WiFiUDP udp;

const int bitsPerSample = 16; // 送信するPCMデータのビット深度
const int bytesPerSample = bitsPerSample / 8;
const int samplesPerPacket = 128; // 1パケットあたりのサンプル数 (調整可能)
const int packetSize = samplesPerPacket * bytesPerSample; // パケットサイズ (バイト)

int16_t pcmValue;
static uint8_t pcmBuffer[packetSize];

// I2S
#define I2S_PIN_CLK 33  //BCLK
#define I2S_PIN_WS  32  //LRCLK
#define I2S_PIN_DOUT I2S_PIN_NO_CHANGE
#define I2S_PIN_DIN 27

#define I2S_SAMPLE_RATE 32000
#define I2S_BUFFER_COUNT 4
#define I2S_BUFFER_SIZE 8 //512

// I2S configurations
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
// https://lang-ship.com/blog/work/esp32-i2s-mic/
// http://k-hiura.cocolog-nifty.com/blog/2019/06/post-60619f.html
const i2s_port_t i2s_port = I2S_NUM_0;
const i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,  // kHz
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,  // 32 bits only
    .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT, // monaural (stereo: I2S_CHANNEL_FMT_RIGHT_LEFT)
    .communication_format = I2S_COMM_FORMAT_I2S,  // I2S
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,  // interrupt level 1
    .dma_buf_count        = I2S_BUFFER_COUNT,  // number of buffers
    .dma_buf_len          = I2S_BUFFER_SIZE,   // samples per buffer
    .use_apll             = false,  // external clock source (APLL not used)
    //.tx_desc_auto_clear   = false,  // clear TX buffer when under buffer occures
    .fixed_mclk           = 0,      // APLL clock
};

const i2s_pin_config_t pin_config = {
  .bck_io_num   = I2S_PIN_CLK, // BCLK
  .ws_io_num    = I2S_PIN_WS, // LRCLK
  .data_out_num = I2S_PIN_DOUT, // not used
  .data_in_num  = I2S_PIN_DIN,  // DOUT
};

const static int volume = 13;    // ボリューム調整
const static int val_offset= 0; // 0点補正

uint8_t i2s_read_buffer[I2S_BUFFER_SIZE]; // I2S読み込み用バッファ

void setup() {
  Serial.begin(115200);

  // WiFi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  udp.begin(senderPort);
  Serial.printf("UDP sender started on port %d\n", senderPort);

  // I2S setup
  esp_err_t _err;
  // installin driver for I2S
  _err = i2s_driver_install(i2s_port, &i2s_config, 0, NULL);
  if(_err != ESP_OK){
    Serial.printf("Failed installing driver: %d\n", _err);
    while(true);
  }
  // pin configuration
  _err = i2s_set_pin(i2s_port, &pin_config);
  if(_err != ESP_OK){
    Serial.printf("Failed setting pin: %d\n", _err);
    while(true);
  }
  Serial.println(F("I2S driver installed."));
}


void loop() {
  // I2Sからのデータ読み込み
  size_t num_bytes_read;  //data length
  // read
  esp_err_t err = i2s_read(i2s_port,
                            (char*)i2s_read_buffer,
                            I2S_BUFFER_SIZE,
                            &num_bytes_read,
                            portMAX_DELAY); //no timeout
  // データを格納する
  if (err == ESP_OK && num_bytes_read > 0) {
    int num_samples_read = num_bytes_read / 4; // 32ビットなので4で割る

    // PCMデータとして送信するバッファを準備
    int pcmBufferIndex = 0;
    for (int i = 0; i < num_samples_read && pcmBufferIndex < packetSize; i++) {
      // 32bitのデータを16bitに変換 (MSBを16bitとして扱う)
      pcmValue = (int16_t)((((uint32_t)i2s_read_buffer[i * 4 + 2]) << 8) | i2s_read_buffer[i * 4 + 3]);

      pcmValue += val_offset; // 0点補正
      pcmValue = constrain(pcmValue, -32768, 32767); // 範囲制限

      pcmBuffer[pcmBufferIndex++] = (uint8_t)(pcmValue & 0xFF);       // 下位バイト
      pcmBuffer[pcmBufferIndex++] = (uint8_t)((pcmValue >> 8) & 0xFF); // 上位バイト

      // パケットサイズを超えないようにチェック
      if (pcmBufferIndex >= packetSize) {
        break;
      }
    }

    // UDPパケットを送信
    if (pcmBufferIndex > packetSize) {
      udp.beginPacket(receiverIP, receiverPort);
      udp.write(pcmBuffer, pcmBufferIndex);
      udp.endPacket();
      // Serial.printf("Sent %d bytes (PCM)\n", pcmBufferIndex);
    }
  } else {
    Serial.printf("I2S read error: %d, bytes read: %d\n", err, num_bytes_read);
    delay(10); // エラー時の遅延
    return;
  }

  // ヒープメモリの使用量を確認
  // Serial.printf("Free heap: %d, Min free heap: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  // delay(1); // 必要に応じて遅延を追加 (UDP送信レート調整)
}