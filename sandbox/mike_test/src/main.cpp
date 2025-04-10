#include <Arduino.h>

#include <driver/i2s.h>

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

uint8_t samples[I2S_BUFFER_SIZE]; //buffer

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println(F("Configuring I2S..."));
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
  // put your main code here, to run repeatedly:
  size_t num_bytes_read;  //data length
  // read
  esp_err_t err = i2s_read(i2s_port, 
                          (char*)samples, 
                          I2S_BUFFER_SIZE, 
                          &num_bytes_read, 
                          portMAX_DELAY); //no timeout
  if(err == ESP_OK && num_bytes_read == I2S_BUFFER_SIZE){
    for(int i = 0; i < num_bytes_read; i += 4){
      int32_t* val = (int32_t*)&samples[i];
      *val = (*val >> volume) + val_offset;
      Serial.println(*val);
    }
  }

  delay(10);
}