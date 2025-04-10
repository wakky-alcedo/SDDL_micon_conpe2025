#include <Arduino.h>

#include <driver/i2s.h>

#define I2S_CH_NUM                             I2S_NUM_0
#define I2S_OUT_BCK                            16 // Bit クロック
#define I2S_OUT_LCK                            17 // Data Word Clock
#define I2S_OUT_DIN                            21 // Data Input
#define I2S_OUT_NC                             -1 // Non Connect

#define I2S_SAMPLE_RATE                        44100 // サンプリング周波数(Hz)
#define I2S_BUFFER_COUNT                      4
#define I2S_BUFFER_SIZE                       512
#define SOUND_BUFFER_SIZE                     512

uint8_t sound_buffer[SOUND_BUFFER_SIZE];                                                // サウンドデータ
float pi = 3.1415926;

void i2s_initialize();

/*
 * セットアップ
 */
void setup() {
  i2s_initialize();
  Serial.begin(115200);
  Serial.println("I2S Sound Test");
}

/*
 * ループ
 */
void loop() {
  size_t transBytes = SOUND_BUFFER_SIZE;

  for (uint32_t i = 0; i < transBytes; i = i + 8) {
    int32_t L_data = sin(2.0 * 3.1415926 * i / SOUND_BUFFER_SIZE) * 32767 + 65536;
    int32_t R_data = sin(4.0 * 3.1415926 * i / SOUND_BUFFER_SIZE) * 32767 + 65536;
    *((int32_t *)(sound_buffer + i)) = L_data;
    *((int32_t *)(sound_buffer + i + 4)) = R_data;
  }

  i2s_write(I2S_CH_NUM, (char *)sound_buffer, I2S_BUFFER_SIZE, &transBytes, portMAX_DELAY);

}

/*
 * I2C 初期化
 */
void i2s_initialize() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // stereo
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = I2S_BUFFER_COUNT,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_OUT_BCK,
    .ws_io_num = I2S_OUT_LCK,
    .data_out_num = I2S_OUT_DIN,
    .data_in_num = I2S_OUT_NC,
  };

  i2s_driver_install(I2S_CH_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_CH_NUM, &pin_config);
}