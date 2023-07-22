  /*
    v1.0.5
      - New feature RGB LED for board Haierdryver v2.1.0
        -- new board versin  2.1.0 (red)
        -- FastLED.h in startup.h
        -- devine RGB_LED in hdv70e1.h
        -- Fixed MAC address in define zone at top of main.ino
        -- Action config in main.ino automatic set stime 10Baht per 15mins

  */

 /* v1.0.6 
   Fix bug: coin value spike  
 */

#include <Arduino.h>
#include "startup.h"

// ******** v1.0.5 ********
//#define FixedMAC "8C:AA:B5:85:9C:50"

// ************************

#define DBprintf Serial.printf
//#define FLIPUPMQTT

// ******** v1.0.5 ********
#ifdef RGB_LED // in hdv70e1.h
  #define NUM_LEDS 1
  CRGB leds[NUM_LEDS];
#endif
// ************************

digitdisplay display(CLK,DIO);


Preferences cfgdata;
Config cfginfo;

int wifiretry = 0;

int coinValue = 0;
int pricePerCoin = 10;  // 10 or 1
int paymentby = 0;  // 1 = COIN, 2 = QR, 3 = KIOSK

int cfgState = 0;
int waitFlag = 0;
bool dispflag=0;
int stateflag = 0;
bool pauseflag = false;

int timeRemain=0;
int firstExtPaid=0;
// int extTime1 = 0;
// int extTime2 = 0;
int extTimePerCoin = 15; //15
int extraPay = 0;
int extpaid = 0;

int price[3];
int stime[3]={45,60,75};  //Production  change from 60,75,90
int prodcounter=0;

float coinwaittimeout =0.5;

int keyPress=0;
int errorCode = 0;
String errorDesc = "";
String disperr="";
String disptxt="";
bool disponce = 0;

//Timer 
Timer serviceTime, waitTime, timeLeft, coinTout;
int8_t serviceTimeID,waitTimeID,timeLeftID,coinToutID;

time_t tnow;

WiFiMulti wifiMulti;

WiFiClient espclient;
PubSubClient mqclient(espclient);

#define FLIPUPMQTT
#ifdef FLIPUPMQTT
  WiFiClient fpclient;
  PubSubClient mqflipup(fpclient);
#endif



// String SoftAP_NAME PROGMEM = "BT_" + getdeviceid();
// IPAddress SoftAP_IP(192,168,8,20);
// IPAddress SoftAP_GW(192,168,8,1);
// IPAddress SoftAP_SUBNET(255,255,255,0);


String pbRegTopic PROGMEM = "payboard/register";
String pbPubTopic PROGMEM = "payboard/backend/"; // payboard/backend/<merchantid>/<uuid>
String pbSubTopic PROGMEM = "payboard/"; //   payboard/<merchantid>/<uuid>

#ifdef FLIPUPMQTT
String fpPubTopic PROGMEM = "/flipup/backend";
String fpSubTopic PROGMEM = "/flipup/";
#endif
//Remainding time 


//secureEsp32FOTA esp32OTA("HDV70E1", "1.0.2");  //Move to action ota 5 Oct 21

AsyncWebServer server(80);

// mDash 
// #ifdef MDASHPASS
// static void rpc_gpio_write(struct mg_rpc_req *r) {
//   long pin = mg_json_get_long(r->frame, "$.params.pin", -1);
//   long val = mg_json_get_long(r->frame, "$.params.val", -1);
//   if (pin < 0 || val < 0) {
//     mg_rpc_err(r, 500, "%Q", "pin and val required");
//   } else {
//     pinMode(pin, OUTPUT);
//     digitalWrite(pin, val);
//     mg_rpc_ok(r, "true");
//   }
// }
// #endif
//****************************************



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

        switch(io_num){
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
            case DSTATE:
                if(gpio_get_level(io_num) == 0){
                  Serial.printf("[intr]->Door Open\n");
                }else{
                  Serial.printf("[intr]->Door Close\n");
                }
                break;
            case MODESW:
                break;
        }  
    }
}

void init_interrupt(){
    //gpio_config_t io_conf;
    //This setting for Negative LOW coin Module
    Serial.printf("  Execute---Initial Interrupt Function\n");

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
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL); 

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

  
    gpio_isr_handler_add((gpio_num_t)COININ, gpio_isr_handler, (void*) COININ);
    //gpio_isr_handler_add((gpio_num_t)DSTATE, gpio_isr_handler, (void*) DSTATE);
    //gpio_isr_handler_add((gpio_num_t)MODESW, gpio_isr_handler, (void*) MODESW);
    //gpio_isr_handler_add((gpio_num_t)COINDOOR, gpio_isr_handler, (void*) COINDOOR);

}


//********************************* End of Interrupt Function ********************************





//********************************* Start Setup Function Here ********************************

void setup() {

  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // This port connect to Dryer
  display.begin();
  display.setBacklight(30);
  //******* v1.0.5 **********
  #ifdef RGB_LED 
    FastLED.addLeds<SK6812, RGB_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
  #endif

  Serial.printf("\n\nDelete NV-RAM data?  y/Y to delete or any to continue.\n");
  Serial.printf("Wait for key 15 secs ");
  display.scrollingText("-Cnd-",2); // wait for delete NV-RAM
  display.print("nd");
  int wtime = 0;
  while((Serial.available() < 1) && (wtime <30)){
    Serial.print("*");
    wtime++;
    delay(500);
  }
  //************ v1.05 wait for NV-RAM ************
  byte keyin = ' ';
  while(Serial.available() > 0){
    keyin = Serial.read();
      //Serial.println(keyin);
  }

  if((keyin == 121) || (keyin == 89)){
    Serial.println("Deleting NV-RAM data.");
    display.scrollingText("-dEL-dAtA",2);
    nvs_flash_erase(); // erase the NVS partition and...
    nvs_flash_init(); // initialize the NVS partition.
  }
  //**********************************************
  //*************************

  Serial.printf("\nSetting up this device ...\n");
  display.scrollingText("-SEtdE-",2); //Setup
  
  //******* Initial GPIO
  initGPIO(INPUT_SET,OUTPUT_SET);

  //******* Initial Interrupt
  display.scrollingText("-It-",2); //Setup
  init_interrupt();

  //******* WiFi Setting
  display.scrollingText("-Cn-",2); //Config Network
  WiFi.mode(WIFI_STA);
  
  wifiMulti.addAP("myWiFi","1100110011");
  wifiMulti.addAP("Home173-AIS","1100110011");
  
  //wifiMulti.addAP("Home259_2G","1100110011");
  for(int i=0;i<loadWIFICFG(cfgdata,cfginfo);i++){
    wifiMulti.addAP(cfginfo.wifissid[i].ssid.c_str(),cfginfo.wifissid[i].key.c_str());
    Serial.printf("AddAP SSID[%d]: %s, Key[%d]: %s\n",i+1,cfginfo.wifissid[i].ssid.c_str(),i+1,cfginfo.wifissid[i].key.c_str());
  }

  Serial.printf("WiFi connecting...\n"); 
  wtime=0;
  while ( (WiFi.status() != WL_CONNECTED) && (wtime < 100)) {  
     display.scrollingText("-nF-",1);
     display.print("nF");
     wifiMulti.run();
     Serial.print(".");
     wtime++;
     delay(500);
  }

  if(wtime > 100){
    Serial.printf("WiFi connecting timeout....Restarting device. \n");
    display.scrollingText("A-Rst",2);
    ESP.restart();
  }

  blinkGPIO(WIFI_LED,400);
  Serial.printf("WiFi Connected...");


  server.begin();
  WiFiinfo();

  display.scrollingText("-Lc-",2); // Load Config
  //initCFG(cfginfo); 
  Serial.printf("Load initial configuration.\n");
  initCFG(cfginfo); 
  // Serial.printf("first showCFG\n");
  // showCFG(cfginfo);
  
  cfginfo.deviceid = getdeviceid();
  cfginfo.asset.assetid = cfginfo.deviceid;

  // To fix mac address 
  #ifdef FixedMAC  // at top of this file
    cfginfo.asset.mac = FixedMAC; //Comment this line for production.
  #else
    cfginfo.asset.mac = WiFi.macAddress();  // Using device mac address
  #endif

  coinwaittimeout = cfginfo.asset.coinwaittimeout;
  Serial.print("  Coin Wait Timeout: ");
  Serial.println(coinwaittimeout);


  Serial.println();
  Serial.println("---------Device Infomation--------");
  Serial.printf("  Device ID: %s\n",cfginfo.deviceid.c_str());
  Serial.printf("  MacAddress: %s\n",cfginfo.asset.mac.c_str());
  //Serial.println("----------------------------------");
 
  //**** Getting Configuration from NV-RAM
  Serial.println("--------- Getting Merchantid from NV-RAM --------");
  cfgdata.begin("config",false);
  if(cfgdata.isKey("merchantid")){
    cfginfo.payboard.merchantid = cfgdata.getString("merchantid");
    Serial.printf("  Use NV-RAM Merchantid: %s\n",cfginfo.payboard.merchantid.c_str());
  }else{
    if(cfginfo.payboard.merchantid.isEmpty()){
      cfginfo.payboard.merchantid = "1000000104";  //this is default mmerchant id
    }
    Serial.printf("  Use Temporary Mechantid: %s\n",cfginfo.payboard.merchantid.c_str());
  }

  if(cfgdata.isKey("fixedmac")){
    cfginfo.asset.mac = cfgdata.getString("fixedmac");
    Serial.printf("  Using Fixed MacAddress: %s\n",cfginfo.asset.mac.c_str());
  }else{
    #ifdef FixedMAC
       Serial.printf("  Use Define Fixed MacAddress: %s\n",cfginfo.asset.mac.c_str());
    #else
       Serial.printf("  Use MacAddress: %s\n",cfginfo.asset.mac.c_str());
    #endif
  }

  if(cfgdata.isKey("sku1")){
    prodcounter=getnvProduct(cfgdata,cfginfo); //copy cfgdata(NV) to cfginfo
    Serial.printf("  Use NV-RAM product information\n");
  }else{
    Serial.printf("  Use temporary Service time\n");
  }

  cfgdata.end();

  //int sz = sizeof(cfginfo.product)/sizeof(cfginfo.product[0]);
  
  int sz = 3;
  Serial.printf("  There are: %d products.\n",sz);
  for(int i=0;i<sz;i++){
    price[i] = int(cfginfo.product[i].price);
    stime[i] = cfginfo.product[i].stime;
    // Serial.printf("Price[%d]:%d\n",i,price[i]);
    // Serial.printf("Stime[%d]:%d\n",i,stime[i]);
  }

  //*** Set price per coin
  if(cfginfo.asset.coinModule){
    pricePerCoin = 1;  //CoinModule is 1 or enum Multi
  }else{
    pricePerCoin = 10; //CoinModule is 0 or enum SINGLE
  }

  cfgdata.begin("config",false); //***<<<<<<<<< config preferences
  // Get UUID
  if(cfgdata.isKey("uuid")){
    cfginfo.payboard.uuid = cfgdata.getString("uuid","").c_str();
    Serial.printf("  Getting UUID from NV-RAM: %s\n",cfginfo.payboard.uuid.c_str());
  }else{//Device not register
    Serial.printf("Device not register.\n");
    display.scrollingText("-CF-UUId-",2); // Device Fail
    delay(200);
    Serial.printf("  Automatic register to backend...Please wait.\n");
    Serial.printf("  MAC Address: %s\n",cfginfo.asset.mac.c_str());
    Serial.printf("  MerchantID: %s\n",cfginfo.payboard.merchantid.c_str());
    payboard backend;
    backend.uri_register = cfginfo.payboard.apihost + "/v1.0/device/register";
    backend.merchantID=cfginfo.payboard.merchantid;
    backend.merchantKEY=cfginfo.payboard.merchantkey;
    backend.appkey=cfginfo.payboard.apikey;

    int rescode = backend.registerDEV(cfginfo.asset.mac.c_str(),cfginfo.payboard.uuid);

    if(rescode == 200){
      Serial.printf("  This devicd got uuid: "); Serial.print(cfginfo.payboard.uuid);
      cfgdata.putString("uuid",cfginfo.payboard.uuid);
      Serial.printf(" saved.\n");
    }else{
      Serial.printf("Rescode: %d\n",rescode);
    }
  }
  cfgdata.end();

  showCFG(cfginfo);

  pbPubTopic = pbPubTopic  + String(cfginfo.payboard.merchantid) +"/"+ String(cfginfo.payboard.uuid);
  pbSubTopic = pbSubTopic + String(cfginfo.payboard.merchantid) +"/"+ String(cfginfo.payboard.uuid);

  #ifdef FLIPUPMQTT
  fpPubTopic = fpPubTopic + String(cfginfo.asset.merchantid) +"/"+ String(cfginfo.asset.assetid);
  fpSubTopic = fpSubTopic + String(cfginfo.asset.merchantid) +"/"+ String(cfginfo.asset.assetid);
  #endif

    //Keep WiFi connection
  while(!WiFi.isConnected()){
    display.scrollingText("-nF-",1);
    display.print("nF");
    digitalWrite(WIFI_LED,LOW);
    wifiMulti.run();
    delay(2000);
  }
  blinkGPIO(WIFI_LED,400); 

  //**** Connecting MQTT
  display.scrollingText("-qtt-",2);  // Host Failed
  pbBackendMqtt();

  //Setting  Time from NTP Server
  display.scrollingText("-ntP-",2); // Time Failed
  display.print("nt");
  delay(200);
  Serial.printf("\nConnecting to TimeServer --> ");
  // cfginfo.asset.ntpServer1 = "1.th.pool.ntp.org";   // Comment on 1 Nov 65
  // cfginfo.asset.ntpServer2 = "asia.pool.ntp.org";   // Comment on 1 Nov 65
  configTime(6*3600,3600,cfginfo.asset.ntpServer1.c_str(),cfginfo.asset.ntpServer2.c_str());
  printLocalTime();
  //Save lastest boot time.

  
  time(&tnow);
  Serial.printf("timestamp: %ld\n",tnow);
  Serial.printf("Time Now: %s\n",ctime(&tnow));
  cfgdata.begin("lastboot",false);
  cfgdata.putULong("epochtime",tnow);
  cfgdata.putString("timestamp",ctime(&tnow));
  cfgdata.end();
  DBprintf("Lastest booting: (%ld) -> %s.\n",tnow,ctime(&tnow));
  
  //******  Check stateflag 
  display.scrollingText("-StFLA-",2); // StateFlag
  cfgdata.begin("config",false);
  if(cfgdata.isKey("stateflag")){
    stateflag = cfgdata.getInt("stateflag",0);
    Serial.printf("stateflag before: %d\n",stateflag);
    String jsonmsg;
    StaticJsonDocument<100> doc;

    switch (stateflag){
      case 1: // After Reboot
          display.scrollingText("Fi-RSt",2);
          doc["response"]="reboot";
          doc["merchantid"]=cfginfo.payboard.merchantid;
          doc["uuid"]=cfginfo.payboard.uuid;
          doc["state"]="reboot_done";
          doc["desc"]="Reboot after action:Reboot complete";
          serializeJson(doc,jsonmsg);  

          if(!mqclient.connected()){
            pbBackendMqtt();
          }else{
            mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
          }

          #ifdef FLIPUPMQTT
            if(!mqflipup.connected()){
              fpBackendMqtt();
            }else{
              mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
            }
          #endif

          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          Serial.printf("stateflag after: %d\n",stateflag);
          cfgState = 3;
        break;
      case 2: // After OTA
          display.scrollingText("Fi-OtA",2);
          display.scrollingText((cfginfo.asset.firmware).c_str(),2);
          doc["response"]="ota";
          doc["merchantid"]=cfginfo.payboard.merchantid;
          doc["uuid"]=cfginfo.payboard.uuid;
          doc["state"]="ota_done";
          doc["desc"]="Reboot after action:OTA complete";
          doc["firmware"]=cfginfo.asset.firmware;
          serializeJson(doc,jsonmsg);  

          if(!mqclient.connected()){
            pbBackendMqtt();
          }else{
            mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
          }

          #ifdef FLIPUPMQTT
            if(!mqflipup.connected()){
              fpBackendMqtt();
            }else{
              mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
            }
          #endif

          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          Serial.printf("stateflag after: %d\n",stateflag);
          cfgState=3;      
        break;
      case 3: // After NVS Delete
          display.scrollingText("Fi-n-dEL",2);
          doc["response"]="nvsdelete";
          doc["merchantid"]=cfginfo.payboard.merchantid;
          doc["uuid"]=cfginfo.payboard.uuid;
          doc["state"]="nvs_done";
          doc["desc"]="Reboot after action:nvs_delete complete";
          doc["firmware"]=cfginfo.asset.firmware;
          serializeJson(doc,jsonmsg);  

          if(!mqclient.connected()){
            pbBackendMqtt();
          }else{
            mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
          }

          #ifdef FLIPUPMQTT
            if(!mqflipup.connected()){
              fpBackendMqtt();
            }else{
              mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
            }
          #endif

          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          Serial.printf("stateflag after: %d\n",stateflag);
          cfgState=3;      
        break;
      case 4: // Mac Address set 
          display.scrollingText("Fi-SEt-Addr",2);
          doc["response"]="setmac";
          doc["merchantid"]=cfginfo.payboard.merchantid;
          doc["uuid"]=cfginfo.payboard.uuid;
          doc["state"]="macAddress Added";
          doc["desc"]="Reboot after action: setmac completed";
          doc["firmware"]=cfginfo.asset.firmware;
          serializeJson(doc,jsonmsg);  

          if(!mqclient.connected()){
            pbBackendMqtt();
          }else{
            mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
          }

          #ifdef FLIPUPMQTT
            if(!mqflipup.connected()){
              fpBackendMqtt();
            }else{
              mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
            }
          #endif          

          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          Serial.printf("stateflag after: %d\n",stateflag);
          cfgState = 3;
        break;
      case 5: // Job Resume from Power Outage
          display.scrollingText("J-Cont",2);
          display.print("JC"); //Power Failed
          cfgState = stateflag;
          dispflag = 1;

          timeRemain = cfgdata.getInt("timeremain",0);  //Get timeRemain
          if(timeRemain >0){
            HDV70E1 dryer;

            Serial.printf("Resume job for orderID: %s for [%d] minutes remain.\n",cfginfo.asset.orderid.c_str(),timeRemain);
            
            if(dryer.isMachineON(MACHINEDC)){ // Found Machine On
              Serial.printf("Found Machine ON. Then Continue counter down time for previous Job\n");
              dryer.powerCtrl(POWER_RLY, MACHINEDC,dryer.TURNOFF);
              delay(5500);
              dryer.powerCtrl(POWER_RLY, MACHINEDC,dryer.TURNON);
              dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
              //digitalWrite(ENPANEL,HIGH);
            }else{ //Machine Off
              Serial.print("Found job but Machine not on. Then turn machine on\n");
              digitalWrite(ENPANEL,LOW);
              dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
              //digitalWrite(ENPANEL,HIGH);
              // digitalWrite(ENPANEL,LOW);
              // dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
            }

            serviceTimeID = serviceTime.after(60*1000*timeRemain,serviceEnd);
            timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
          }else{
            Serial.printf("State is 5 but no timeRemain. Then clear state\n");
            stateflag = 0;
            cfgdata.putInt("stateflag",0);
            cfgState = 3;
            timeRemain = 0 ;// Added 28 July 22
          }      
        break;
      case 6: // Delete macAddress
          display.scrollingText("Fi-dEL-Addr",2);
          doc["response"]="delmac";
          doc["merchantid"]=cfginfo.payboard.merchantid;
          doc["uuid"]=cfginfo.payboard.uuid;
          doc["state"]="macAddress deleted";
          doc["desc"]="Reboot after action: delmac completed";
          doc["firmware"]=cfginfo.asset.firmware;
          serializeJson(doc,jsonmsg);  

          if(!mqclient.connected()){
            pbBackendMqtt();
          }else{
            mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
          }

          #ifdef FLIPUPMQTT
            if(!mqflipup.connected()){
              fpBackendMqtt();
            }else{
              mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
            }
          #endif          

          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          Serial.printf("stateflag after: %d\n",stateflag);
          cfgState = 3;
        break;
      case 10: // Device Offline
          display.scrollingText("oFFLinE",2);
          display.print("OF");
          cfgState = 10;
          Serial.printf("******************** This Asset is OFFLINE. ******************** \n");
        break;
      default: // Time Remain error clear old job.
          cfgdata.putInt("stateflag",0);
          stateflag = 0;
          cfgState = 3;
          if(timeRemain != 0){ 
            cfgdata.putInt("timeremain",0);
            timeRemain = cfgdata.getInt("timeremain");
            Serial.printf("Reseting TimeRemain: %d\n",timeRemain);
          }      
        break;
    }

    //***************************************************************
    /*
    if(stateflag == 1){ // After Action Reboot
      doc["response"]="reboot";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="reboot_done";
      doc["desc"]="Reboot after action:Reboot complete";
      serializeJson(doc,jsonmsg);  

      if(!mqclient.connected()){
        pbBackendMqtt();
      }else{
        mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
      }

      #ifdef FLIPUPMQTT
        if(!mqflipup.connected()){
          fpBackendMqtt();
        }else{
          mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
        }
      #endif

      cfgdata.putInt("stateflag",0);
      stateflag = 0;
      Serial.printf("stateflag after: %d\n",stateflag);
      cfgState = 3;
    }else if(stateflag ==2){ // After Action OTA
      display.scrollingText((cfginfo.asset.firmware).c_str(),2);
      doc["response"]="ota";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="ota_done";
      doc["desc"]="Reboot after action:OTA complete";
      doc["firmware"]=cfginfo.asset.firmware;
      serializeJson(doc,jsonmsg);  

      if(!mqclient.connected()){
        pbBackendMqtt();
      }else{
        mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
      }

      #ifdef FLIPUPMQTT
        if(!mqflipup.connected()){
          fpBackendMqtt();
        }else{
          mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
        }
      #endif

      cfgdata.putInt("stateflag",0);
      stateflag = 0;
      Serial.printf("stateflag after: %d\n",stateflag);
      cfgState=3;

    }else if(stateflag == 3){ //After nvsdelete
      display.scrollingText("n-dEL",2);
      doc["response"]="nvsdelete";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="nvs_done";
      doc["desc"]="Reboot after action:nvs_delete complete";
      doc["firmware"]=cfginfo.asset.firmware;
      serializeJson(doc,jsonmsg);  

      if(!mqclient.connected()){
        pbBackendMqtt();
      }else{
        mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());
      }

      #ifdef FLIPUPMQTT
        if(!mqflipup.connected()){
          fpBackendMqtt();
        }else{
          mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
        }
      #endif

      cfgdata.putInt("stateflag",0);
      stateflag = 0;
      Serial.printf("stateflag after: %d\n",stateflag);
      cfgState=3;
    }else if(stateflag == 5){ // Job Resume.
      display.print("J1"); //Power Outage
      cfgState = stateflag;
      dispflag = 1;

      timeRemain = cfgdata.getInt("timeremain",0);  //Get timeRemain
      if(timeRemain >0){
        HDV70E1 dryer;

        Serial.printf("Resume job for orderID: %s for [%d] minutes remain.\n",cfginfo.asset.orderid.c_str(),timeRemain);
        
        if(dryer.isMachineON(MACHINEDC)){ // Found Machine On
          Serial.printf("Found Machine ON. Then Continue counter down time for previous Job\n");
          dryer.powerCtrl(POWER_RLY, MACHINEDC,dryer.TURNOFF);
          delay(5500);
          dryer.powerCtrl(POWER_RLY, MACHINEDC,dryer.TURNON);
          dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
          //digitalWrite(ENPANEL,HIGH);
        }else{ //Machine Off
          Serial.print("Found job but Machine not on. Then turn machine on\n");
          digitalWrite(ENPANEL,LOW);
          dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
          //digitalWrite(ENPANEL,HIGH);
          // digitalWrite(ENPANEL,LOW);
          // dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);
        }

        serviceTimeID = serviceTime.after(60*1000*timeRemain,serviceEnd);
        timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
      }else{
        Serial.printf("State is 5 but no timeRemain. Then clear state\n");
        stateflag = 0;
        cfgdata.putInt("stateflag",0);
        cfgState = 3;
        timeRemain = 0 ;// Added 28 July 22
      }
    }else{
      cfgState = 3;
      if(timeRemain != 0){ 
        cfgdata.putInt("timeremain",0);
        timeRemain = cfgdata.getInt("timeremain");
        Serial.printf("Reseting TimeRemain: %d\n",timeRemain);
      }
    }
    */
    //***************************************************************
  }else{ // not found "stateflag in NV-RAM"
    stateflag = 0;
    cfgdata.putInt("stateflag",stateflag);
    cfgState = 3;
    Serial.printf("StateFlag is 0 (zero) and CfgFlage is 3. \n");
  }
  cfgdata.end();

  display.scrollingText("Func",2); 
  
  Serial.printf("\n\n");
  Serial.printf("******************************************************\n");
  Serial.printf("*      System Ready for service. Firmware: %s      *\n",cfginfo.asset.firmware.c_str());  
  Serial.printf("******************************************************\n");  

  display.scrollingText(cfginfo.asset.firmware.c_str(),2); // Finish 
  delay(500);   



// HDV70E1 dryer1;
// dryer1.powerCtrl(POWER_RLY,MACHINEDC,dryer1.TURNON);
// dryer1.showPanel();
// digitalWrite(ENPANEL,LOW);
// delay(500);
// digitalWrite(ENPANEL,HIGH);

  
  
  //selftest();
}
//----------------------------------- END of Setup Function Here -----------------------------------




//----------------------------------- Start LOOP Function Here -----------------------------------
void loop() {

  //Serial.println(digitalRead(ENPANEL));
  //delay(500);
  keyPress=display.comReadByte();
  switch(keyPress){
    case 244:
      break;
    case 245:
      break;
    case 246:
      break;
    case 247:
      break;          
  }

  if(WiFi.isConnected()){
    blinkGPIO(WIFI_LED,400);
    if(!mqclient.connected() && (cfgState >= 2)){
      pbBackendMqtt();
    }

    #ifdef FLIPUPMQTT
      if(!mqflipup.connected() && (cfgState >= 2)){
        fpBackendMqtt();
      }    
    #endif

    switch(cfgState){
      case 3:  // System Ready and waiting for service
          digitalWrite(ENCOIN,HIGH);   // Waiting for Coin

          //******* v1.0.5 **********
          #ifdef RGB_LED  // look at hdv70ed.h
            leds[0] = CRGB::Green;
            FastLED.show();
          #endif
          //************** **********

          if(coinValue > 0){
            cfgState = 4;
            //******** v1.0.6 ********
            Serial.println("Set coin timeout.");
            coinToutID = coinTout.after(60000*2,clearcoin); // wait for 2 mins then clear coinvalue=0
            //************************
          }else{
            String dispPrice;
            if(price[0]!=0){
              dispPrice = dispPrice + "--" + String(price[0]);
            }
            if(price[1]!=0){
              dispPrice = dispPrice + "--" + String(price[1]);
            }
            if(price[2]!=0){
              dispPrice = dispPrice + "--" + String(price[2]);
            }
            dispPrice = dispPrice + "--";
            display.scrollingText(dispPrice.c_str(),1);
            Serial.println(dispPrice.c_str());
            display.setColonOn(false);
          }
          break;
      case 4:  // System Accept request for service  (First coin inserted)
          display.setBacklight(30);
          display.print(coinValue);
          display.setColonOn(true);

          // Serial.print("Coin WaitTimeout: ");
          // Serial.println(coinwaittimeout);
      
          // stime[0] = 1; // For deveopment only
          // extTimePerCoin = 1; //For development only;
         
          if( (coinValue == price[0]) &&  (waitFlag == 0)  ){  // For program 40 Bath 60Mins
            waitFlag++;
            //extraPay = 1;
            coinTout.stop(coinToutID);  //****** v1.0.6 *****
            timeRemain = stime[0];
            waitTimeID = waitTime.after(60*1000*coinwaittimeout,progstart); // Change from 0.166 to coinwaittimeout  25 Jul 22
            //progstart();
          }else if( (coinValue == price[1]) &&  (waitFlag == 1)   ) {  // For program 50 Bath 75Mins
            waitFlag++;   //Waitflag now = 2
            //extraPay=2;
            waitTime.stop(waitTimeID);
            coinTout.stop(coinToutID);  //****** v1.0.6 *****
            timeRemain = stime[1];
            //timeRemain = stime[0]+ extTimePerCoin;
            waitTimeID = waitTime.after(60*1000*coinwaittimeout,progstart);  // Change from 0.166 to 0.25  25 Jul 22
            //progstart();
            Serial.printf("Pay more 1 coinValue: %d\n",coinValue);
            Serial.printf("Check TimeRemain-1: %d\n",timeRemain);
          }else if( (coinValue == price[2]) && (waitFlag == 2) ) {  // For program 60 Bath 90Mins
            //digitalWrite(ENCOIN,LOW);
            waitFlag++;
            //extraPay = 3;
            waitTime.stop(waitTimeID);
            coinTout.stop(coinToutID);  //****** v1.0.6 *****
            timeRemain = stime[2];
            //timeRemain = stime[0]+ (2* extTimePerCoin);
            progstart();
            Serial.printf("Pay more 2 coinValue: %d\n",coinValue);
            Serial.printf("Check TimeRemain-2: %d\n",timeRemain);
          }else{

          }
          
          if(waitFlag == (prodcounter-1)){
            digitalWrite(ENCOIN,LOW);
          }
          break;
      case 5:  // System running job.
          //-------------- Pause and Resume drying and remaintime -----------------
          if(digitalRead(MACHINEDC)){
            if(digitalRead(DSTATE)){ // High = Door Close
              if(pauseflag == true){
                HDV70E1 dryer;

                digitalWrite(ENPANEL,LOW);
                //dryer.ctrlRotary(dryer.MIN120);
                dryer.ctrlbytes[4] = dryer.MIN90; //0x05 = 120M
                dryer.ctrlButton(dryer.START);
                Serial.printf("Timer resume for %d mins\n",timeRemain);
                serviceTime.resume(serviceTimeID);
                timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
                pauseflag = false;
                //digitalWrite(ENPANEL,HIGH);
              }
            }else{ // LOW = Door Open
              if(pauseflag == false){
                Serial.printf("Timer pause, remain is : %d\n",timeRemain);
                display.print("PU"); //Pause
                serviceTime.pause(serviceTimeID);
                timeLeft.stop(timeLeftID);
                pauseflag = true;
              }
            }
          }
          //---------------End pause and resume  ------------------


          //-------------- Extrapay to exten time -----------------

          //Serial.printf("Extrapay now: %d\n",extraPay);
          if(paymentby == 1){ // by Coin
            if( (coinValue > price[0]+extpaid) && (waitFlag ==1) ){  //coinValue change from price[0] to price[1]
              //Mark to new state
              waitFlag ++;
              extraPay = 2;

              //Stop previous timer
              waitTime.stop(waitTimeID);
              timeLeft.stop(timeLeftID);
              serviceTime.stop(serviceTimeID);

              extpaid = coinValue - (price[0]);       
              Serial.printf("1st timeremain now: %d\n",timeRemain); 
              timeRemain = timeRemain + extTimePerCoin;

              Serial.printf("1st extraPay is %d\n",extpaid);
              Serial.printf("1st set new time: %d\n",timeRemain);
              waitTimeID = waitTime.after(60*1000*coinwaittimeout,progstart);  // Change from 0.166 to 0.25  25 Jul 22
              
            }else if( (coinValue > price[1]) && (waitFlag ==2)) { //change price[1] to price[2]
              //Disable coinModule
              digitalWrite(ENCOIN,LOW);

              //Mark to new state
              waitFlag++;
              extraPay = 3;

              //Stop previous timer
              waitTime.stop(waitTimeID); 
              timeLeft.stop(timeLeftID);
              serviceTime.stop(serviceTimeID);

              
              // if(firstExtPaid){
              //   extpaid = coinValue - (price[1]-extpaid);
              //   firstExtPaid = 0;
              // }else{
                extpaid = coinValue - price[1];
                firstExtPaid = 0;
              //}
              
              timeRemain = timeRemain + extTimePerCoin;
              
              Serial.printf("2nd extraPay is %d\n",extpaid);
              Serial.printf("2nd set new time: %d\n",timeRemain);
              //prog3start();
              progstart();
            }
          }
          //-------------- End Extrapay to exten time -----------------
          

          //----------------- Display operation --------------------
          display.setColonOn(false);
          disptxt="";
          if(!disperr.isEmpty()){// Show Error to 7-Segment.
            disptxt = disperr +"-";
            display.scrollingText(disptxt.c_str(),1);
          }else{
            if(timeRemain <10){
              disptxt = disptxt+ "0"+String(timeRemain);
            }else{
              disptxt = disptxt+ "T-" + String(timeRemain); // Edit "T-" 21 Oct 65
            } 
            display.scrollingText(disptxt.c_str(),1);
            delay(1000);
            display.animation3(display,300,2); 
          }
          //----------------- END Display operation --------------------
          break;
      case 6:
          break;
      case 10: //Ofline
          display.scrollingText("-OFFLInE-",2);
          display.print("oL"); // Machine Offline
          if(!disponce){
            Serial.printf("******************** This Asset is OFFLINE. ******************** \n");
            disponce = 1;
          }
          break;
    }

  }else{

    while(!WiFi.isConnected() && (wifiretry<5)){
      wifiretry++;
      display.scrollingText("-netF-",3);
      display.print("nF");
      digitalWrite(WIFI_LED,LOW);
      digitalWrite(0,LOW);
      Serial.printf("WiFi Connecting.....retry: %d\n",wifiretry);
      wifiMulti.run();
      delay(3000);
    }

    if(wifiretry > 5){
      Serial.printf("Rebooting due to retry over the wifiretry limit.: %d\n",wifiretry);
      ESP.restart();
    }

    if(WiFi.isConnected()){
      digitalWrite(WIFI_LED,HIGH);
      digitalWrite(0,HIGH);
      Serial.println("WiFi reconnected");
      WiFiinfo();
      Serial.print("cfgState: ");
      Serial.println(cfgState);

      Serial.print("Time Remain: ");
      Serial.println(timeRemain);
    }  
  }


  serviceTime.update();
  waitTime.update();
  timeLeft.update();
  coinTout.update();
  mqclient.loop();
  #ifdef FLIPUPMQTT
   mqflipup.loop();
  #endif
}
//----------------------------------- END Of Loop Function Here -----------------------------------


void clearcoin(){ // ********* v 1.0.6 *********
  Serial.println("clear Coin");

  digitalWrite(ENCOIN,LOW);
  
  timeRemain = 0;
  coinValue = 0;
  paymentby = 0;

  cfgState = 3;
  waitFlag = 0;
  dispflag = 0;
  pauseflag = false;

  firstExtPaid = 0;
  extpaid = 0;
  extraPay = 0;
  disperr="";
  disptxt="";
  disponce = 0;

}

void progstart(){

  payboard backend;
  String response;
  int rescode = 0;
  HDV70E1 dryer;

  //digitalWrite(ENCOIN,LOW); // Disable Coin Module

  Serial.printf("Starting progstart , Paymentby %d\n",paymentby);
  Serial.printf("waitFlag: %d\n",waitFlag);


  backend.merchantID=cfginfo.payboard.merchantid;
  backend.merchantKEY=cfginfo.payboard.merchantkey;
  backend.appkey=cfginfo.payboard.apikey;

  while(!WiFi.isConnected()){
    wifiMulti.run();
  }

  switch(paymentby){
    case 1: // by Coin
      backend.uri_countCoin = cfginfo.payboard.apihost + "/v1.0/device/countcoin";
      if(cfgState == 4){
        rescode = backend.coinCounter(cfginfo.payboard.uuid.c_str(),coinValue,response);
      }else if(cfgState == 5){
        if(extraPay == 2){ //1st extrapay
          firstExtPaid = 1;
        }
        Serial.printf("[progstart]->Update extrapay to payboard for %d bath\n",extpaid);
        rescode = backend.coinCounter(cfginfo.payboard.uuid.c_str(),extpaid,response);
      }

      if(rescode == 200){
        Serial.printf("Response trans: %s\n",response.c_str());
        cfgdata.putString("orderid",response); //Save Order id
      }else{
        Serial.printf("Response code: %d\n",rescode);
        Serial.printf("Response trans: %s\n",response.c_str());
      }
      break;
    case 2:// by QR
      digitalWrite(ENCOIN,LOW);
      Serial.printf("Sending QR acknoloedge to backend\n");
      cfgdata.putString("orderid",cfginfo.asset.orderid);
      backend.uri_deviceStart = cfginfo.payboard.apihost + "/v1.0/device/start";
      rescode = backend.deviceStart(cfginfo.asset.orderid.c_str(),response);

      if(rescode == 200){
        if(response == "success"){
          Serial.printf("Pro1Start backend undated\n");
        } 
      }else{
        Serial.printf("Rescode: %d\n",rescode);
      }
      break;
    case 3: // by Kiosk
      break;
    case 4:
      break;  // Free Admin activate it.
  }
  


  // Starting Machine  Comment on 24 Aug 2022
  digitalWrite(ENPANEL,LOW);
  disperr="";
  disptxt="";
  cfgState = 5;
  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",cfgState);
  cfgdata.putInt("timeremain",timeRemain);
  cfgdata.end();  
      

  if(dryer.isMachineON(MACHINEDC)){
    Serial.printf("Extend time started job\n");
    display.print("Et");
    delay(3000);
    serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
    timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
  }else{
    Serial.printf("On service of Program-1 for %d minutes\n",timeRemain);
    delay(3000);
    int dryercode = dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN90,display,errorCode);

    switch(timeRemain){
      case 45:
        break;
      case 60:
        break;
      case 75:
        break;
    }

    if(dryercode){ //On service successfuly save state.
      serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
      timeLeftID = timeLeft.every(60*1000*1,serviceLeft);

      //******* v1.0.5 **********
      #ifdef RGB_LED  // look at hdv70ed.h
        leds[0] = CRGB::Blue;
        FastLED.show();
      #endif
      //*************************
    }else{
      //Notice Error to backend
      //Power On machine, but machine not on.
      Serial.print("[progstart]->Power on machine. But machine not response command. (not turn on)\n");
      disperr="PE";

      //******* v1.0.5 **********
      #ifdef RGB_LED  // look at hdv70ed.h
        leds[0] = CRGB::Red;
        FastLED.show();
      #endif  
      //*************************
    } 
  }  
}







void pbCallback(char* topic, byte* payload, unsigned int length){
  String jsonmsg;
  DynamicJsonDocument doc(1024);
  HDV70E1 dryer;

  Serial.println();
  Serial.println("Message arrived pbCallback with topic: ");
  Serial.println(topic);
  Serial.println();

  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  DeserializationError error = deserializeJson(doc, payload);
  // Test if parsing succeeds.

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  String action = doc["action"].as<String>();
  Serial.print("\nThis action paramater: ");
  Serial.println(action);

  if(action == "config"){
    display.scrollingText("ConF",2); // Accepted Config
    int sz = doc["detail"].size();
    Serial.printf("detail array size: %d\n",sz);

    cfgdata.begin("config",false);
    //cfgdata.putString("merchantid",cfginfo.payboard.merchantid);


    for(int x=0;x<(3-sz);x++){
      Serial.printf(" delete index: %d\n",3-x);
      cfgdata.putString(("sku"+ String(3-x)).c_str(),"");
      cfgdata.putFloat(("price"+String(3-x)).c_str(),0);
      cfgdata.putInt(("stime"+ String(3-x)).c_str(),0);      
      price[2-x]=0;

    }  

    for(int i=0;i<sz;i++){
      String sku = doc["detail"][i]["sku"].as<String>();
      price[i] = doc["detail"][i]["price"].as<float>();
      stime[i] = doc["detail"][i]["stime"].as<int>();
      if(stime[i] == 0){
       stime[i] = (price[i]/10)*15;
      }

      Serial.print("sku"+ String(i+1) +": ");
      Serial.println(sku);
      Serial.print("price"+String(i+1)+": ");
      Serial.println(price[i]);
      Serial.print("stime"+String(i+1)+": ");
      Serial.println(stime[i]);

      cfgdata.putString(("sku"+ String(i+1)).c_str(),sku);
      cfgdata.putFloat(("price"+String(i+1)).c_str(),price[i]);
      cfgdata.putInt(("stime"+ String(i+1)).c_str(),stime[i]);

      cfginfo.product[i].sku = sku;
      cfginfo.product[i].price = price[i];
      cfginfo.product[i].stime = stime[i];

      Serial.printf("Saving stime[%d]: %d\n",i,stime[i]);
    }

    cfgdata.end();
    doc.clear();
    doc["response"]="config";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;      
    doc["state"]="saved";
    doc["desc"]="config saved";
    
    cfgState = 3;
    
  }else if(action == "setmac"){
    display.scrollingText("S-Addr",2); // Accepted Config
    Serial.printf("Response for action: setmac.\n");
    String newmac = doc["mac"].as<String>();

    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",4);
    cfgdata.putString("fixedmac",newmac);
    
    if(cfgdata.isKey("uuid")){
      cfgdata.remove("uuid");
    }
    cfginfo.asset.mac = cfgdata.getString("fixedmac");
    cfgdata.end();

    //cfginfo.asset.mac = newmac;
   
    Serial.printf("New Mac Address:  %s\n",cfginfo.asset.mac.c_str());

    doc.clear();
    doc["response"]="setmac";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;      
    doc["mac"]=cfginfo.asset.mac;
    doc["state"]="saved";
    doc["desc"]="New mac address saved";
    serializeJson(doc,jsonmsg);
    //Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
    if(!mqclient.connected()){
      pbBackendMqtt();
    }
    mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

    #ifdef FLIPUPMQTT
      if(!mqflipup.connected()){
        fpBackendMqtt();
      }
      mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
    #endif
    delay(500); 
    ESP.restart();

  }else if(action == "delmac"){
    display.scrollingText("d_Addr",2);
    Serial.printf("Response for action: delete mac.\n");

    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",6);
    if(cfgdata.isKey("fixedmac")){
      cfgdata.remove("fixedmac");
      cfgdata.remove("uuid");
    }

    cfgdata.end();
    cfginfo.asset.mac = "";
    Serial.printf("Deleted Mac Address:  \"%s\" \n",cfginfo.asset.mac.c_str());
    doc.clear();
    doc["response"]="delmac";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;      
    doc["state"]="deleted";
    doc["desc"]="Deleted Mac Address.";
    serializeJson(doc,jsonmsg);
    //Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
    if(!mqclient.connected()){
      pbBackendMqtt();
    }
    mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

    #ifdef FLIPUPMQTT
      if(!mqflipup.connected()){
        fpBackendMqtt();
      }
      mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
    #endif
    delay(500); 

    ESP.restart(); 
  }else if(action == "paid"){
    display.scrollingText("PAId",2); // Accepted Config
    //Paid and then start service.
    Serial.printf("Response for action: paid.\n");

    paymentby = 2; // 1=coin, 2= qr, 3=kiosk , 4 = free
    coinValue = doc["price"].as<int>();
    
    if(coinValue == price[0]){
      waitFlag = 0;
    }else if(coinValue == price[1]){
      waitFlag = 1;
    }else if(coinValue == price[2]){
      waitFlag = 2;
    }

    cfginfo.asset.orderid = doc["orderNo"].as<String>();
    cfgdata.begin("config",false);
    cfgdata.putString("orderid",cfginfo.asset.orderid);
    cfgdata.end();
  
    Serial.printf(" [PAID]->Customer paid for: %d\n",coinValue);
    Serial.printf(" [PAID]->Orderid: %s\n",cfginfo.asset.orderid.c_str());
    doc.clear();
    doc["response"] = "paid";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;  
    doc["state"]="accepted";
    doc["desc"]="accepted orderid: " + cfginfo.asset.orderid;

  }else if(action == "countcoin"){
    /*  Not use Jul64
    Serial.printf("Response for action: coincount.\n");
    //{"action":"countcoin","orderNo":"02210526162202176","price":"10.00"}
    // two idea 1st: after insert coin machine work immediately.
    // 2nd: machine not operate if cannot update to backend.

    String resmsg = doc["StatusCode"].as<String>();
    String msg = doc["Message"].as<String>();
    String trans = doc["ResultValues"]["transactionId"].as<String>();

    cfginfo.asset.orderid = trans;


    doc["response"] = "countcoin";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="accepted";
    doc["desc"]="accepted transaction: " + trans;
    */
  }else if(action == "ping"){
    display.scrollingText("Ping",2);
    Serial.printf("response action PING\n");

    doc.clear();
    doc["response"]="ping";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["rssi"]=WiFi.RSSI();
    switch(cfgState){
      case 3: // Available (waiting for job)
          doc["state"] = "Available";
          break;
      case 4: // Available (waiting for job)
          doc["state"] = "Booked"; // Coin inserted
          break;
      case 5: // Available (waiting for job)
          doc["state"] = "Busy"; // On Service
          break;
      case 10:
          doc["state"] = "Offline";
          break;
    }
    //doc["state"]=cfgState;
    doc["firmware"]=cfginfo.asset.firmware;
    doc["timeRemain"] = timeRemain;

  }else if( (action == "reset") || (action=="reboot") || (action=="restart")){
      //set stateflag = 2 flag
    display.scrollingText("RSt",2);  
    Serial.printf("Accept request action reboot\n");

    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",1);
    cfgdata.end();

    doc.clear();
    doc["response"]="reboot";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;  
    doc["state"]="accepted";
    doc["disc"] = "Asset rebooting.";

    serializeJson(doc,jsonmsg);
    Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
    if(!mqclient.connected()){
      pbBackendMqtt();
    }
    mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

    #ifdef FLIPUPMQTT
      if(!mqflipup.connected()){
        fpBackendMqtt();
      }
      mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
    #endif
    delay(500);    
    
    ESP.restart();

  }else if(action == "ota"){
    display.scrollingText("OtA",2);
    secureEsp32FOTA esp32OTA("HDV70E1", cfginfo.asset.firmware.c_str());
    WiFiClientSecure clientForOta;
    digitalWrite(ENCOIN,LOW); // ENCoin off
    //esp32OTA._host ="enigma.openlandscape.cloud";
    //esp32OTA._descriptionOfFirmwareURL="/swift/v1/AUTH_574cacf4b45348f4be8d6a36fce1b6b3/firmware/HDV70E1/firmware.json";
    

    esp32OTA._host="www.flipup.net"; //e.g. example.com
    esp32OTA._descriptionOfFirmwareURL="/firmware/HDV70E1/firmware.json"; //e.g. /my-fw-versions/firmware.json
    
    //esp32OTA._certificate=root_ca;   //uncomment if use https with secure
    esp32OTA.clientForOta=clientForOta;
    Serial.print("Current Version:");
    Serial.println(cfginfo.asset.firmware);
    esp32OTA._firwmareVersion = cfginfo.asset.firmware;
  
    bool shouldExecuteFirmwareUpdate=esp32OTA.execHTTPSCheck();
    if(shouldExecuteFirmwareUpdate){
      cfginfo.asset.firmware = esp32OTA._firwmareVersion.c_str();    
      //set stateflag = 2 flag
      cfgdata.begin("config",false);
      cfgdata.putInt("stateflag",2);
      cfgdata.putString("firmware",cfginfo.asset.firmware);
      cfgdata.end();

      doc.clear();
      doc["response"] = "ota";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["firmware"] = cfginfo.asset.firmware;
      doc["state"] = "accepted";
      doc["disc"] = "Firmware upgrading then rebooting in few second."; 

      serializeJson(doc,jsonmsg);
      Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
      Serial.print("Ver: "); Serial.println(cfginfo.asset.firmware.c_str());
      Serial.println("Firmware updating, It's will take few second");

      if(!mqclient.connected()){
        pbBackendMqtt();
      }
      mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

      #ifdef FLIPUPMQTT
        if(!mqflipup.connected()){
          fpBackendMqtt();
        }
        mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
      #endif

      display.scrollingText("F-otA-",2); // Upgrade Failed
      display.print("UF"); // Upgrade Failed
      esp32OTA.executeOTA_REBOOT();
    }else{
      doc["merchanttid"] = cfginfo.payboard.merchantid;
      doc["uuid"] = cfginfo.payboard.uuid;
      doc["response"] = "ota";
      doc["state"] = "FAILED";
      doc["desc"] = "Firmware update failed, version may be the same."; 

      serializeJson(doc,jsonmsg);
      Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
      if(!mqclient.connected()){
        pbBackendMqtt();
      }
      mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

      #ifdef FLIPUPMQTT
        if(!mqflipup.connected()){
          fpBackendMqtt();
        }
        mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
      #endif
    }
    delay(1000);
  }else if(action == "firmware"){  
    display.scrollingText("S-FiE",2); // Accepted Config
    doc.clear();
    doc["response"] = "firmware";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["firmware"]=cfginfo.asset.firmware;
    doc["desc"]="Current firmware version: " + cfginfo.asset.firmware;
  }else if(action == "setwifi"){
    // {"action":"setwifi","index":"1","ssid":"Home173-AIS","key":"1100110011","reconnect":"1"}
    display.scrollingText("S-SSid",2); // 
    String ssid = doc["ssid"].as<String>();
    String key = doc["key"].as<String>();
    int index = doc["index"].as<int>();
    int wifireconn = doc["reconnect"].as<int>();

    cfgdata.begin("wificfg",false);
    cfgdata.putString(("ssid"+ (String)(index)).c_str(),ssid);
    cfgdata.putString(("key"+ (String)(index)).c_str(),key);
      Serial.printf("Setting WiFi with following\n");
      Serial.print(("ssid"+ (String)(index))+": ");
      Serial.println(cfgdata.getString(("ssid"+ (String)(index)).c_str()));
      Serial.print(("key"+ (String)(index))+": ");
      Serial.println(cfgdata.getString(("key"+ (String)(index)).c_str()));
    cfgdata.end();

    doc.clear();
    doc["response"] = "setwifi";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    
    wifiMulti.addAP(ssid.c_str(),key.c_str());
    if(wifireconn){
      WiFi.disconnect();
      wifiMulti.run();
      doc["state"]="Reconnected";
      doc["desc"]="setWiFi completed  and reconnected";
    }else{
      doc["state"]="WiFi_Changed";
      doc["desc"]="setWiFi conpleted but will effect next boot.";
    }
  
  }else if(action == "coinmodule"){ // {"action":"coinmodule","coinmodule":"SINGLE"}
    display.scrollingText("C-tPE",2); // Accepted Config
    cfginfo.asset.coinModule = (doc["coinmodule"].as<String>() == "single")?SINGLE:MULTI; //  SINGLE=0, MULTI=1
    (cfginfo.asset.coinModule == MULTI)?pricePerCoin=1:pricePerCoin=10;

    doc.clear();
    doc["response"] = "coinmodule";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Coin_changed";

    (cfginfo.asset.coinModule == MULTI)?doc["desc"]="Change coinModule to: MULTI":doc["desc"]="Change coinModule to: SINGLE";

  }else if(action == "coinwaittimeout"){ // // {"action":"coinwaittimeout","coinwaittimeout":3}
    display.scrollingText("C-TOut",2);

    cfginfo.asset.coinwaittimeout = doc["coinwaittimeout"].as<float>();
    cfgdata.begin("config",false);
    cfgdata.putFloat("coinwaittimeout",cfginfo.asset.coinwaittimeout);
    cfgdata.end();

    doc.clear();
    doc["response"] = "coinwaittimeout";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="CoinWaitTimeout Changed";
    doc["desc"] = "Set CoinWaitTimeout: " + String(cfginfo.asset.coinwaittimeout);

  }else if(action == "orderid"){  // {"action":"orderid"}
    display.scrollingText("A_oId",2);
  }else if(action == "assettype"){  // {"action":"assettype","assettype":"0"}  0=Washer , 1=Dryer
    display.scrollingText("ASSt",2);
    
    String mtype = doc["assettype"].as<String>();
    mtype.toUpperCase();

    if(mtype == "WASHER"){
      cfginfo.asset.assettype = 0;
    }else if(mtype == "DRYER"){
      cfginfo.asset.assettype = 1;
    }
  
    cfgdata.begin("config",false);
    cfgdata.putInt("assettype",cfginfo.asset.assettype);
    cfgdata.end();

    doc.clear();
    doc["response"] = "assettype";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Asset_changed"; 

    if(cfginfo.asset.assettype){// 1 = Dryer
      doc["desc"]="Set assetType to DRYER";
    }else{// 0 = Washer
      doc["desc"]="Set assetType to WASHER";
    }
    
  }else if(action == "payboard"){// To set payboard parameter
    display.scrollingText("PbCFg",2);
    String params = doc["params"];
    bool merchantflag = 0;

    cfgdata.begin("config",false);
    if(params.equals("all")){
        cfginfo.payboard.uuid = doc["uuid"].as<String>();
        cfgdata.putString("uuid",cfginfo.payboard.uuid);
      
        cfginfo.payboard.merchantid = doc["merchantid"].as<String>();;
        cfginfo.payboard.merchantkey = doc["merchantkey"].as<String>();
        cfgdata.putString("merchantid",cfginfo.payboard.merchantid);
        cfgdata.putString("merchantkey",cfginfo.payboard.merchantkey);
      
        cfginfo.payboard.apihost = doc["apihost"].as<String>();
        cfginfo.payboard.apikey = doc["apikey"].as<String>();
        cfgdata.putString("apihost",cfginfo.payboard.apihost);
        cfgdata.putString("apikey",cfginfo.payboard.apikey);
      
        cfginfo.payboard.mqtthost = doc["mqtthost"].as<String>();
        cfginfo.payboard.mqttport = doc["mqttport"].as<int>();
        cfginfo.payboard.mqttuser = doc["mqttuser"].as<String>();
        cfginfo.payboard.mqttpass = doc["mqttpass"].as<String>();
        cfgdata.putString("mqtthost",cfginfo.payboard.mqtthost);
        cfgdata.putInt("mqttport",cfginfo.payboard.mqttport);
        cfgdata.putString("mqttuser",cfginfo.payboard.mqttuser);
        cfgdata.putString("mqttpass",cfginfo.payboard.mqttpass);
    }else{
    
      if(params.equals("uuid")){
        cfginfo.payboard.uuid = doc["uuid"].as<String>();
        Serial.print("This is new uuid: ");
        Serial.println(cfginfo.payboard.uuid);
        cfgdata.putString("uuid",cfginfo.payboard.uuid);
      }else if(params.equals("merchantid")){
        // {"action":"payboard","params":"merchantid","merchantid":""}
        cfginfo.payboard.merchantid = doc["merchantid"].as<String>();;
        cfgdata.putString("merchantid",cfginfo.payboard.merchantid);
        merchantflag = true;
      }else if(params.equals("merchantkey")){
        // {"action":"payboard","params":"merchantid","merchantkey":""}
        cfginfo.payboard.merchantkey = doc["merchantkey"].as<String>();
        cfgdata.putString("merchantkey",cfginfo.payboard.merchantkey);    
      }else if(params.equals("apihost")){
        cfginfo.payboard.apihost = doc["apihost"].as<String>();
        cfgdata.putString("apihost",cfginfo.payboard.apihost);
      }else if(params.equals("apikey")){
        cfginfo.payboard.apikey = doc["apikey"].as<String>();
        cfgdata.putString("apikey",cfginfo.payboard.apikey);  
      }else if(params.equals("mqtthost")){
        cfginfo.payboard.mqtthost = doc["mqtthost"].as<String>();
        cfginfo.payboard.mqttport = doc["mqttportt"].as<int>();
        cfginfo.payboard.mqttuser = doc["mqttuser"].as<String>();
        cfginfo.payboard.mqttpass = doc["mqttpass"].as<String>();
        cfgdata.putString("mqtthost",cfginfo.payboard.mqtthost);
        cfgdata.putInt("mqttport",cfginfo.payboard.mqttport);
        cfgdata.putString("mqttuser",cfginfo.payboard.mqttuser);
        cfgdata.putString("mqttpass",cfginfo.payboard.mqttpass);
      }
    }
    cfgdata.end();
    doc.clear();
    doc["response"] = "payboard";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Pb_Changed"; 
    if(merchantflag){
      mqclient.disconnect();
      #ifdef FLIPUPMQTT
        mqflipup.disconnect();
      #endif
    }
  }else if(action == "backend"){
    display.scrollingText("A_BECF",2);
  }else if(action == "jobcancel"){ // {"action":"jobcancel"}
    display.scrollingText("J-CAn",2); // Job Cancel
    serviceEnd();
    doc.clear();
    doc["response"] = "jobcancel";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Job_Canceled";
    doc["desc"]="Manual cancel job.";

  }else if(action == "jobcreate"){   // {"action":"jobcreate","price",40,"paymentby":4}
    display.scrollingText("J-Add",2); // Job Addewd
    coinValue = doc["price"].as<int>();
    paymentby = doc["paymentby"].as<int>();  //1 = coin , 2 = qr, 3 = kiosk , 4 = free

    if(coinValue == price[0]){
      waitFlag = 0;
    }else if(coinValue == price[1]){
      waitFlag = 1;
    }else if(coinValue == price[2]){
      waitFlag = 2;
    }

    Serial.printf("CoinValue: %d\n",coinValue);
    Serial.printf("waitFlag: %d\n",waitFlag);
    

    doc.clear();
    doc["response"] = "jobcreate";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Job_Created";
    doc["desc"]="Manual create job.";

  }else if(action == "nvsdelete"){  // {"action":"nvsdelete"}
    String msg;
    display.scrollingText("n-dEL",2); // Nvs Deleted
    Serial.printf("NVS size before delete: %d\n",cfgdata.freeEntries());
    nvs_flash_erase(); // erase the NVS partition and...
    nvs_flash_init(); // initialize the NVS partition.
    msg = "NVS size after delete: "+ (String)cfgdata.freeEntries();
    Serial.println(msg);

    doc.clear();
    doc["response"] = "nvsdelete";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="NVS_Deleted";
    doc["desc"]=msg;    

    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",3);
    cfgdata.end();
    delay(3000);
    ESP.restart();
  }else if(action == "offline"){  // {"action":"offline"}
    display.scrollingText("A_OFFLInE-",2); // Nvs Deleted
    digitalWrite(ENCOIN,LOW); // ENCoin off
    cfgState = 10;// CFGState 10 offline
    timeRemain = 0;
    disponce = 0;
    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",cfgState);
    cfgdata.putInt("timeremain",timeRemain);
    cfgdata.end();   

    doc.clear();
    doc["response"] = "offline";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="offline";
    doc["desc"]="Asset is in OFFLINE mode";    

  }else if(action == "online"){ // {"action":"online"}
    display.scrollingText("A_OnLInE",2); // Enable Machine
    resetState();
    doc.clear();
    doc["response"] = "online";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="online";
    doc["desc"]="Asset is in ONLINE mode";       
  }else if(action == "turnon"){ //Manual turn machine on   {"action":"turnon"}
    display.scrollingText("t-On",2); // Enable Machine

    //Check Machine is ON ? if on reject this action
    bool mstate=false;
    mstate = dryer.controlByPanel(POWER_RLY, MACHINEDC,ENPANEL);

    if(mstate){
      doc.clear();
      doc["response"] = "TurnON";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="Completed";
      doc["desc"]="Machine is ON. Action completed.";     
    }else{
      doc.clear();
      doc["response"] = "TurnON";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="Failed";
      doc["desc"]="Machine not turn on. Action failed.";     
    }
  }else if(action == "turnoff"){ //Manual turn machine on  {"action":"turnoff"}
    display.scrollingText("t-OFF",2); // Enable Machine

    //Check Machine is ON ? if on reject this action
    bool mstate=false;
    mstate = dryer.powerCtrl(POWER_RLY, MACHINEDC,dryer.TURNOFF);

    if(mstate){
      doc.clear();
      doc["response"] = "TurnOFF";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="Completed";
      doc["desc"]="Machine is OFF. Action completed.";     
    }else{
      doc.clear();
      doc["response"] = "TurnOFF";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="Failed";
      doc["desc"]="Machine not turn off. Action failed.";     
    }
  }else if(action == "setntp"){ // {"action":"setntp","ntpinx":1,"ntpserver":"xxx.xxx.xxx.xxx"}
    display.scrollingText("SetntP",2);

    int ntpInx = doc["ntpinx"].as<int>();
    String ntpValue = doc["value"].as<String>();

    (ntpInx == 1)?cfginfo.asset.ntpServer1=ntpValue : cfginfo.asset.ntpServer2 = ntpValue;

    doc.clear();
    doc["response"] = "setntp";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="SetNTP";
    doc["desc"]="New NTP server updated. Please reboot to active it.";  

  }

  serializeJson(doc,jsonmsg);
  Serial.println();
  Serial.print("pbPubTopic: "); Serial.println(pbPubTopic);
  Serial.print("Jsonmsg: ");Serial.println(jsonmsg);

  if(!mqclient.connected()){
    pbBackendMqtt();
  }
  mqclient.publish(pbPubTopic.c_str(),jsonmsg.c_str());

  #ifdef FLIPUPMQTT
    if(!mqflipup.connected()){
      fpBackendMqtt();
    }
    mqflipup.publish(fpPubTopic.c_str(),jsonmsg.c_str());
  #endif
  delay(500);
}


#ifdef FLIPUPMQTT
  void fpCallback(char* topic, byte* payload, unsigned int length){
    pbCallback(topic,payload,length);
  }

void fpBackendMqtt(){
    if(!mqflipup.connected()){
      mqflipup.setServer(cfginfo.backend.mqtthost.c_str(),cfginfo.backend.mqttport);
      mqflipup.setCallback(fpCallback);

    
      Serial.printf("Backend-2 Mqtt connecting ...");
      while(!mqflipup.connect(cfginfo.deviceid.c_str(),cfginfo.backend.mqttuser.c_str(), cfginfo.backend.mqttpass.c_str())){
        Serial.printf(".");
        delay(500);
      }
      Serial.printf("connected\n");
      mqflipup.subscribe(fpSubTopic.c_str());
      Serial.printf("   Subscribe Topic: %s\n",fpSubTopic.c_str());
    }        
}
#endif


void pbBackendMqtt(){
    if(!mqclient.connected()){
      mqclient.setServer(cfginfo.payboard.mqtthost.c_str(),cfginfo.payboard.mqttport);
      mqclient.setCallback(pbCallback);

      
      pbSubTopic = "payboard/" + String(cfginfo.payboard.merchantid) + "/" + String(cfginfo.payboard.uuid);
    
      Serial.printf("Backend-1 Mqtt connecting ...");
      while(!mqclient.connect(cfginfo.deviceid.c_str(),cfginfo.payboard.mqttuser.c_str(), cfginfo.payboard.mqttpass.c_str())){
        Serial.printf(".");
        delay(500);
      }
      Serial.printf("connected\n");
      mqclient.subscribe(pbSubTopic.c_str());
      Serial.printf("   Subscribe Topic: %s\n",pbSubTopic.c_str());
    }        
}



void serviceLeft(){
  Serial.printf("Service Time remain: %d\n",--timeRemain);
  cfgdata.begin("config",false);
  cfgdata.putInt("timeremain",timeRemain);
  cfgdata.end();
}


void serviceEnd(){
  payboard backend;
  String response;
  int rescode;
  HDV70E1 dryer;

  dryer.powerCtrl(POWER_RLY,MACHINEDC,dryer.TURNOFF);  //Trun off machine.

  serviceTime.stop(serviceTimeID);
  timeLeft.stop(timeLeftID);

  backend.merchantID=cfginfo.payboard.merchantid;
  backend.merchantKEY=cfginfo.payboard.merchantkey;
  backend.appkey=cfginfo.payboard.apikey;

  switch(paymentby){
    case 0:
      Serial.printf("Waiting for job.\n");
      break;
    case 1: // by Coin
      Serial.printf("Coin Job Finished.\n");
      break;
    case 2: //by QR
      Serial.printf("QR Job Finished.\n");
      cfgdata.putString("orderid",cfginfo.asset.orderid);
      backend.uri_deviceStart = cfginfo.payboard.apihost + "/v1.0/device/stop";
      rescode = backend.deviceStart(cfginfo.asset.orderid.c_str(),response);

      if(rescode == 200){
        if(response == "success"){
          Serial.printf("ProgStart backend updated\n");
        } 
      }else{
        Serial.printf("Rescode: %d\n",rescode);
      }
      break;
    case 3: //by Kiosk
      Serial.printf("Kiosk Job finished.\n");
      break;
    case 4: // by Admin
      Serial.printf("Admin Job finished\n");
      break;
  }

  timeRemain = 0;
  coinValue = 0;
  paymentby = 0;

  cfgState = 3;
  waitFlag = 0;
  dispflag = 0;
  pauseflag = false;

  firstExtPaid = 0;
  extpaid = 0;
  extraPay = 0;
  disperr="";
  disptxt="";
  disponce = 0;

  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",0);
  cfgdata.putString("orderid","");
  cfgdata.putInt("timeremain",0);
  cfgdata.end();
 

  //resetState();
  Serial.printf("Job Finish.  Poweroff machine soon.\n");
}



void resetState()
{
  timeRemain = 0;
  coinValue = 0;
  paymentby = 0;

  cfgState = 3;
  waitFlag = 0;
  dispflag = 0;
  pauseflag = false;

  firstExtPaid = 0;
  extpaid = 0;
  extraPay = 0;
  disperr="";
  disptxt="";
  disponce = 0;

  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",0);
  cfgdata.putString("orderid","");
  cfgdata.putInt("timeremain",0);
  cfgdata.end();

}