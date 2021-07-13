#include <Arduino.h>
#include <SevenSegmentTM1637.h>
#include <Preferences.h>


void initGPIO(unsigned long long INP, unsigned long long OUTP){
  gpio_config_t io_config;
  //*** Initial INTERRUPT PIN
//   io_config.intr_type = GPIO_INTR_NEGEDGE;
//   io_config.pin_bit_mask = INTR;
//   io_config.mode = GPIO_MODE_INPUT;
//   io_config.pull_up_en = GPIO_PULLUP_ENABLE;
//   gpio_config(&io_config);
  
  //*** Initial INPUT PIN
  io_config.pin_bit_mask = INP;
  io_config.intr_type = GPIO_INTR_DISABLE;
  io_config.mode = GPIO_MODE_INPUT;
  io_config.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_config);


  //*** Initial INPUT & OUTPUT PIN
  io_config.pin_bit_mask = OUTP;
  io_config.intr_type = GPIO_INTR_DISABLE;
  io_config.mode = GPIO_MODE_INPUT_OUTPUT;
  gpio_config(&io_config);
}

void saveRemainTime(SevenSegmentTM1637 &disp, Preferences &nvdata){

}