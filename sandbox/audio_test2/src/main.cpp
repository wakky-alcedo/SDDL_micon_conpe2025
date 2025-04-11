#include <Arduino.h>

#include <driver/i2s.h>

#define I2S_NUM_MIC                     I2S_NUM_0
#define I2S_PIN_CLK_MIC                 23
#define I2S_PIN_WS_MIC                  19
#define I2S_PIN_DOUT_MIC                -1//not use
#define I2S_PIN_DIN_MIC                 18

#define I2S_NUM_AMP                     I2S_NUM_1
#define I2S_PIN_CLK_AMP                 16
#define I2S_PIN_WS_AMP                  17
#define I2S_PIN_DOUT_AMP                21
#define I2S_PIN_DIN_AMP                 -1//not use

#define I2S_SAMPLE_RATE                 44100 //44.1kHz
#define I2S_BUFFER_COUNT                4
#define I2S_BUFFER_SIZE                 512
#define Sound_LEN                       512

uint8_t Mic_Buffer[I2S_BUFFER_SIZE];    // DMA転送バッファ
uint8_t Sound[Sound_LEN];               // サウンドデータ

int L_Data, R_Data;
int Volume = 255;

// prototype
void i2sInit_MIC();
void i2sInit_AMP();
void mic2amp();

void setup() {
  Serial.begin(115200);
  i2sInit_MIC();
  i2sInit_AMP();
  Serial.println("MicModAmp02.ino");
}

void loop() {
  mic2amp();
}


void mic2amp(){
  size_t transBytes;
  i2s_read(I2S_NUM_MIC, (char*)Mic_Buffer, I2S_BUFFER_SIZE, &transBytes, portMAX_DELAY);// Mic In
  //Convert 8 bytes data to L_Data and R_Data
  for(int i=0;i<transBytes;i=i+8){
    for(int j=0;j<4;j++){
      L_Data=L_Data<<8;
      L_Data=L_Data + Mic_Buffer[i+j];
    }
    for(int j=4;j<8;j++){
      R_Data=R_Data << 8;
      R_Data=R_Data + Mic_Buffer[i+j];
    }
    //Process L_Data and R_Data
    L_Data=L_Data * Volume;
    R_Data=R_Data * Volume;
    //Convert to 8 bytes data
    for(int j=0;j<4;j++){
      Sound[i+3-j]=(byte)(L_Data & 0xFF);
      L_Data=L_Data >> 8;
    }
    for(int j=0;j<4;j++){
      Sound[i+7-j]=(byte)(R_Data & 0xFF);
      R_Data=R_Data >> 8;
    }
  }
  i2s_write(I2S_NUM_AMP, (char*)Sound, I2S_BUFFER_SIZE, &transBytes, portMAX_DELAY);// Speak Out
}


void i2sInit_MIC() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, //stereo
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = I2S_BUFFER_COUNT,
    .dma_buf_len          = I2S_BUFFER_SIZE,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num           = I2S_PIN_CLK_MIC,
    .ws_io_num            = I2S_PIN_WS_MIC,
    .data_out_num         = I2S_PIN_DOUT_MIC,
    .data_in_num          = I2S_PIN_DIN_MIC,
  };
 
  i2s_driver_install(I2S_NUM_MIC, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_MIC, &pin_config);
}

void i2sInit_AMP() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, //stereo
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = I2S_BUFFER_COUNT,
    .dma_buf_len          = I2S_BUFFER_SIZE,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num           = I2S_PIN_CLK_AMP,
    .ws_io_num            = I2S_PIN_WS_AMP,
    .data_out_num         = I2S_PIN_DOUT_AMP,
    .data_in_num          = I2S_PIN_DIN_AMP,
  };
 
  i2s_driver_install(I2S_NUM_AMP, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_AMP, &pin_config);
}