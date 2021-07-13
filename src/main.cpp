#ifdef NVS
  #include <nvs_flash.h>
#endif

#include <Arduino.h>
#include "hdv70e1.h"
#include "driver/gpio.h"
#include "esp32fota.h"

//#include <NTPClient.h>
#include <time.h>
//#include <WiFiUdp.h>//

#include<Preferences.h>

//Display Library
#include <SevenSegmentTM1637.h>
#include <SevenSegmentExtended.h>
#include <SevenSegmentFun.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include "config.h"
//#include <WiFi.h>

#include "timer.h"
#include "animation.h"


int coinValue = 0;
int pricePerCoin = 10;  // 10 or 1
int paymentby = 0;  // 1 = COIN, 2 = QR, 3 = KIOSK

WiFiMulti wifiMulti;
Preferences cfgdata;
int cfgState = 0;


//Timer 
Timer serviceTime, waitTime;
int8_t serviceTimeID,waitTimeID;

Timer timer1;
int8_t timer1_ID;

//Remainding time 
int timeRemain=10;

//digitdisplay display(CLK,DIO);
SevenSegmentTM1637 display(CLK,DIO);

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void everyMIN(){
    time_t now;

    time(&now);
    Serial.printf("timestamp: %ld\n",now);
    Serial.printf("Time now: %s\n",ctime(&now));
    
    display.print(--timeRemain);
}


//********************************* Interrupt Function **********************************
gpio_config_t io_config;
xQueueHandle gpio_evt_queue = NULL;

void IRAM_ATTR gpio_isr_handler(void* arg)
{
  long gpio_num = (long) arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void gpio_task(void *arg){
    gpio_num_t io_num;  

    for(;;){
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {  
            Serial.printf("\nGPIO[%d] intr, val: %d \n", io_num, gpio_get_level(io_num));
        } 

        switch (io_num){
            case COININ:
                if(gpio_get_level(io_num) == 0){
                  paymentby = 1;  // Set 1 = paymentby COIN
                  coinValue = coinValue + pricePerCoin;
                  Serial.printf("CoinValue: %d\n", coinValue);
                }  
                break;
            // case BILLIN:
            //     (gpio_get_level(io_num) == 0)?bill++:bill=bill; 
            //     break;
            case MODESW:
                break;
            default:
                break;
        }  
    }
}

void init_interrupt(){
    //gpio_config_t io_conf;
    //This setting for Negative LOW coin Module
    io_config.intr_type = GPIO_INTR_NEGEDGE;    
    io_config.pin_bit_mask = INTERRUPT_SET;
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_up_en = (gpio_pullup_t)1;

    //configure GPIO with the given settings
    gpio_config(&io_config);

    //gpio_set_intr_type((gpio_num_t)COININ, GPIO_INTR_NEGEDGE);

    /*********** create a queue to handle gpio event from isr ************/
    gpio_evt_queue = xQueueCreate(10, sizeof(long));

    /*********** Set GPIO handler task ************/
    xTaskCreate(gpio_task, "gpio_task", 1024, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

  
    gpio_isr_handler_add((gpio_num_t)COININ, gpio_isr_handler, (void*) COININ);
    //gpio_isr_handler_add((gpio_num_t)MODESW, gpio_isr_handler, (void*) MODESW);
    //gpio_isr_handler_add((gpio_num_t)COINDOOR, gpio_isr_handler, (void*) COINDOOR);

}

//********************************* End of Interrupt Function ********************************





//********************************* Start Setup Function Here ********************************

void setup() {

  Serial.begin(115200);
  Serial.printf("Start setup device ...\n");

  //display.print("FE");
  display.begin();
  display.setBacklight(30);
  display.print("CF");
  
  #ifdef NVS
    Serial.printf("Delete all data in NV-RAM\n");
    nvs_flash_erase(); // erase the NVS partition and...
    nvs_flash_init(); // initialize the NVS partition.
    while(true);
  #endif

  //******* Initial GPIO
  initGPIO(INPUT_SET,OUTPUT_SET);

  //******* Initial Interrupt
  init_interrupt();

  //******* WiFi Setting
  WiFi.mode(WIFI_AP_STA);
  wifiMulti.addAP("Home173-AIS", "1100110011");
  wifiMulti.addAP("Home173-AIS","1100110011");
  wifiMulti.addAP("myWiFi","1100110011");

  Serial.printf("Connection WiFi...\n");
  while (WiFi.status() != WL_CONNECTED) {  
     wifiMulti.run();
     Serial.print(".");
     delay(500);
  }
  //blinkLED.oscillate(WIFI_LED,1000,HIGH);
  Serial.printf("WiFi Connected...");
  Serial.println(WiFi.softAPIP());

  //Getting Time from NTP Server
  Serial.printf("Time from NTP: \n");
  configTime(6*3600,3600,ntpServer1.c_str(),ntpServer2.c_str());
  printLocalTime();
  //Save lastest boot time.

  time_t sttime, entime;
  time(&sttime);
  Serial.printf("timestamp: %ld\n",sttime);
  Serial.printf("Time Now: %s\n",ctime(&sttime));
  sleep(5);

  time(&entime);
  Serial.println(entime);  
  Serial.printf("Diff Time %f\n",difftime(entime,sttime));

  Serial.printf("Start Counter timer1\n");

  //Test timer.every
  timer1_ID=timer1.every(60*1000*1,everyMIN);



  
  //selftest();
}
//----------------------------------- END of Setup Function Here -----------------------------------




//----------------------------------- Start LOOP Function Here -----------------------------------
void loop() {

  while(WiFi.status() != WL_CONNECTED){

  }
  //blinkLED.pulse(WIFI_LED,650,HIGH);

  switch(cfgState){
    case 3:  // System Ready and waiting for service
        break;
    case 4:  // System Accept request for service  (First coin inserted)
        break;
    case 5:  // System running job as request.
        break;
    case 6:
        break;
  }

  timer1.update();
  serviceTime.update();
  waitTime.update();
}
//----------------------------------- END Of Loop Function Here -----------------------------------



