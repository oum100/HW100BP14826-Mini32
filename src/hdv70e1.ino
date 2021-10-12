#include <Arduino.h>
#include "startup.h"

#define DBprintf Serial.printf
//#define FLIPUPMQTT

digitdisplay display(CLK,DIO);

Preferences cfgdata;
Config cfginfo;


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


int keyPress=0;
int errorCode = 0;
String errorDesc = "";
String disperr="";
String disptxt="";
bool disponce = 0;

//Timer 
Timer serviceTime, waitTime, timeLeft;
int8_t serviceTimeID,waitTimeID,timeLeftID;


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

//********************************* Webserial Callback function **********************************
void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String msg = "";
  for(int i=0; i < len; i++){
    msg += char(data[i]);
  }
  WebSerial.println(msg);
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
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  

  Serial.printf("Start setup device ...\n");

  display.begin();
  display.setBacklight(30);
  display.print("SS"); //Setup

  delay(200);
  

  //******* Initial GPIO
  initGPIO(INPUT_SET,OUTPUT_SET);

  //******* Initial Interrupt
  init_interrupt();
  delay(500);


  //******* WiFi Setting
  display.print("Cn"); //Config Network

  delay(200);
  WiFi.mode(WIFI_STA);
  
  wifiMulti.addAP("Home173-AIS","1100110011");
  wifiMulti.addAP("myWiFi","1100110011");
  //wifiMulti.addAP("Home259_2G","1100110011");
  for(int i=0;i<loadWIFICFG(cfgdata,cfginfo);i++){
    wifiMulti.addAP(cfginfo.wifissid[i].ssid.c_str(),cfginfo.wifissid[i].key.c_str());
    Serial.printf("AddAP SSID[%d]: %s, Key[%d]: %s\n",i+1,cfginfo.wifissid[i].ssid.c_str(),i+1,cfginfo.wifissid[i].key.c_str());
  }

  Serial.printf("Connection WiFi...\n");
  while (WiFi.status() != WL_CONNECTED) {  
     display.print("nF");

     //WiFi.begin("Home173-AIS","1100110011");
     wifiMulti.run();
     Serial.print(".");
     delay(800);
  }
  blinkGPIO(WIFI_LED,400);
  Serial.printf("WiFi Connected...");
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
  server.begin();

  WiFiinfo();

  display.print("LC"); // Load Config
  WebSerial.println("[LC]->Loading Configuration");
  delay(200);
  //initCFG(cfginfo); 
  Serial.printf("Load initial configuration.\n");
  initCFG(cfginfo); 
  // Serial.printf("first showCFG\n");
  // showCFG(cfginfo);
  
  cfginfo.deviceid = getdeviceid();
  cfginfo.asset.assetid = cfginfo.deviceid;
  cfginfo.asset.mac = WiFi.macAddress();


  Serial.println();
  Serial.printf("Device ID: %s\n",cfginfo.deviceid.c_str());
  Serial.printf("MacAddress: %s\n",cfginfo.asset.mac.c_str());

 

  //**** Getting Configuration from NV-RAM
  cfgdata.begin("config",false);
  if(cfgdata.isKey("merchantid")){
    cfginfo.payboard.merchantid = cfgdata.getString("merchantid");
    Serial.printf("Use NV-RAM merchantid: %s\n",cfginfo.payboard.merchantid.c_str());
  }else{
    cfginfo.payboard.merchantid = "1000000104";  //this is default mmerchant id
    Serial.printf("Use temporary mechantid: %s\n",cfginfo.payboard.merchantid.c_str());
  }
  if(cfgdata.isKey("sku1")){
    getnvProduct(cfgdata,cfginfo); //copy cfgdata(NV) to cfginfo
    Serial.printf("Use NV-RAM product information\n");
  }else{
    Serial.printf("Use temporary Serive time\n");
  }
  cfgdata.end();

  //int sz = sizeof(cfginfo.product)/sizeof(cfginfo.product[0]);
  
  int sz = 3;
  Serial.printf(" Number Product %d\n",sz);
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
    Serial.printf("Getting UUID from NV-RAM: %s\n",cfginfo.payboard.uuid.c_str());
  }else{//Device not register
    Serial.printf("Device not register.\n");
    display.print("dF"); // Device Fail
    WebSerial.println("[dF]->Device not register");
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
      Serial.printf("This devicd got uuid: "); Serial.println(cfginfo.payboard.uuid);
      cfgdata.putString("uuid",cfginfo.payboard.uuid);
      Serial.printf("Save uuid completed: %s\n",cfginfo.payboard.uuid.c_str());
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
    display.print("nF");
    WebSerial.println("[nF]->WiFi Connected");
    digitalWrite(WIFI_LED,LOW);
    wifiMulti.run();
    delay(2000);
  }
  blinkGPIO(WIFI_LED,400); 

  //**** Connecting MQTT
  display.print("HE");  // Host Failed
  WebSerial.println("[HE]->MQTT server connection error.");
  delay(200);
  pbBackendMqtt();

  //Setting  Time from NTP Server
  display.print("tE"); // Time Failed
  WebSerial.println("[tE]->Time server connection error");
  delay(200);
  Serial.printf("\nConnecting to TimeServer --> ");
  cfginfo.asset.ntpServer1 = "0.asia.pool.ntp.org";
  cfginfo.asset.ntpServer2 = "1.asia.pool.ntp.org";
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
  display.print("SF"); // StateFlag
  WebSerial.println("[SF]->StateFlag checking");
  cfgdata.begin("config",false);
  if(cfgdata.isKey("stateflag")){
    stateflag = cfgdata.getInt("stateflag",0);
    Serial.printf("stateflag before: %d\n",stateflag);
    String jsonmsg;
    StaticJsonDocument<100> doc;

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
    }else if(stateflag == 5){ // Last Service not finish but may be power off.
      display.print("F1"); //Power Failed
      WebSerial.println("[F1]->Found job not finish. possible power failure.");
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
          dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
          //digitalWrite(ENPANEL,HIGH);
        }else{ //Machine Off
          Serial.print("Found job but Machine not on. Then turn machine on\n");
          digitalWrite(ENPANEL,LOW);
          dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
          //digitalWrite(ENPANEL,HIGH);
          // digitalWrite(ENPANEL,LOW);
          // dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
        }

        serviceTimeID = serviceTime.after(60*1000*timeRemain,serviceEnd);
        timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
      }else{
        Serial.printf("State is 5 but no timeRemain. Then clear state\n");
        stateflag = 0;
        cfgdata.putInt("stateflag",0);
        cfgState = 3;
      }


    }else{

      cfgState = 3;
      if(timeRemain != 0){
        //cfgdata.begin("config",false);
        cfgdata.putInt("timeremain",0);
        timeRemain = cfgdata.getInt("timeremain");
        Serial.printf("Reseting TimeRemain: %d\n",timeRemain);
        //cfgdata.end();
      }
    }
  }else{
    Serial.printf(" It is here \n");
    stateflag = 0;
    cfgdata.putInt("stateflag",stateflag);
    cfgState = 3;
  }
  cfgdata.end();

  display.print("Fn"); // Finish 
  WebSerial.println("[Fn]->Setup finish");
  Serial.printf("\n\n");
  Serial.printf("******************************************************\n");
  Serial.printf("*      System Ready for service. Firmware:%s      *\n",cfginfo.asset.firmware.c_str());  
  Serial.printf("******************************************************\n");  
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

          if(coinValue > 0){
            cfgState = 4;
          }else{
            String dispPrice;
            if(price[0]!=0){
              dispPrice = String(price[0]);
            }
            if(price[1]!=0){
              dispPrice = dispPrice + "--" + String(price[1]);
            }
            if(price[2]!=0){
              dispPrice = dispPrice + "--" + String(price[2]);
            }
            display.scrollingText(dispPrice.c_str(),1);
          }
          break;
      case 4:  // System Accept request for service  (First coin inserted)
          display.setBacklight(30);
          display.print(coinValue);
          display.setColonOn(true);
      
          // stime[0] = 1; // For deveopment only
          // extTimePerCoin = 1; //For development only;
         
          if( (coinValue == price[0]) &&  (waitFlag == 0)  ){  // For program 40 Bath 60Mins
            waitFlag++;
            //extraPay = 1;
            timeRemain = stime[0];
            waitTimeID = waitTime.after(60*1000*0.166,progstart);
            //progstart();
          }else if( (coinValue == price[1]) &&  (waitFlag == 1)   ) {  // For program 50 Bath 75Mins
            waitFlag++;   //Waitflag now = 2
            //extraPay=2;
            waitTime.stop(waitTimeID);

            timeRemain = stime[1];
            //timeRemain = stime[0]+ extTimePerCoin;
            waitTimeID = waitTime.after(60*1000*0.166,progstart);
            //progstart();
            Serial.printf("Pay more 1 coinValue: %d\n",coinValue);
            Serial.printf("Check TimeRemain-1: %d\n",timeRemain);
          }else if( (coinValue == price[2]) && (waitFlag == 2) ) {  // For program 60 Bath 90Mins
            digitalWrite(ENCOIN,LOW);
            waitFlag++;
            //extraPay = 3;
            waitTime.stop(waitTimeID);

            timeRemain = stime[2];
            //timeRemain = stime[0]+ (2* extTimePerCoin);
            progstart();
            Serial.printf("Pay more 2 coinValue: %d\n",coinValue);
            Serial.printf("Check TimeRemain-2: %d\n",timeRemain);
          }
          break;
      case 5:  // System running job as request.
    
          //-------------- Pause and Resume drying and remaintime -----------------
          if(digitalRead(MACHINEDC)){
            if(digitalRead(DSTATE)){ // High = Door Close
              if(pauseflag == true){
                HDV70E1 dryer;

                digitalWrite(ENPANEL,LOW);
                //dryer.ctrlRotary(dryer.MIN120);
                dryer.ctrlbytes[4] = 0x05;
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
                WebSerial.println("[PU]->Job pause by user open door");
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
              waitTimeID = waitTime.after(60*1000*0.166,progstart);
              
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
          display.setColonOn(0);
          disptxt="";
          if(!disperr.isEmpty()){
            disptxt = disperr +"-";
          }

          if(timeRemain <10){
            disptxt = disptxt+ "0"+String(timeRemain);
          }else{
            disptxt = disptxt+ String(timeRemain);
          } 

          display.scrollingText(disptxt.c_str(),1);
          delay(1000);
          display.animation3(display,300,2);
          //----------------- END Display operation --------------------
          
          //display.scrollingText(String(timeRemain+"--").c_str(),1);
          break;
      case 6:
          break;
      case 10: //Ofline
          display.scrollingText("--OFF--",1);
          if(!disponce){
            Serial.println("Asset is in OFFLINE mode");
            WebSerial.println("Asset is in OFFLINE mode");
            disponce = 1;
          }
          break;
    }

  }else{
    digitalWrite(WIFI_LED,LOW);
    digitalWrite(0,LOW);
    Serial.printf("WiFi Connecting.....\n");
    wifiMulti.run();
     
    delay(1500);
    if(WiFi.isConnected()){
      Serial.println("connected");
      WiFiinfo();

      Serial.print("cfgState: ");
      Serial.println(cfgState);
    }  
  }


  serviceTime.update();
  waitTime.update();
  timeLeft.update();
  mqclient.loop();
  #ifdef FLIPUPMQTT
   mqflipup.loop();
  #endif
}
//----------------------------------- END Of Loop Function Here -----------------------------------


void progstart(){

  payboard backend;
  String response;
  int rescode = 0;
  HDV70E1 dryer;

  //digitalWrite(ENCOIN,LOW); // Disable Coin Module

  Serial.printf("Starting progstart , Paymentby %d\n",paymentby);
  WebSerial.print("[progstart]->Starting with payment by ");
  WebSerial.println(paymentby);

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
  


  // Starting Machine
  digitalWrite(ENPANEL,LOW);
  disperr="";
  disptxt="";
  cfgState = 5;
  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",cfgState);
  cfgdata.putInt("timeremain",timeRemain);
  cfgdata.end();   
  if(dryer.isMachineON(MACHINEDC)){
    Serial.printf("Extend time for to started job\n");
    display.print("Et");
    WebSerial.println("[Et]->User insert more coin, Extended time for current job");
    delay(3000);
    serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
    timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
  }else{
    Serial.printf("On service of Program-1 for %d minutes\n",timeRemain);
    WebSerial.println("[progstart]->Job accepted, Power on machine.");
    delay(3000);
    int dryercode = dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
    if(dryercode){ //On service successfuly
      serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
      timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
      WebSerial.println("[progstart]->Power on machine successful");
    }else{
      //Notice Error to backend
      //Power On machine, but machine not on.
      Serial.print("[progstart]->Power on machine. But machine not response command. (not turn on)\n");
      disperr="PE";
      WebSerial.println("[progstart]->Power on machine, But machine not response. ");
    } 
  }  
}







void pbCallback(char* topic, byte* payload, unsigned int length){
  String jsonmsg;
  DynamicJsonDocument doc(1024);

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

  String action = doc["action"];
  Serial.print("\nThis action paramater: ");
  Serial.println(action);

  if(action == "config"){

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
      // stime[]={30,40,40};

      Serial.print("sku"+ String(i+1) +": ");
      Serial.println(sku);
      Serial.print("price"+String(i+1)+": ");
      Serial.println(price[i]);

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
    display.scrollingText("A-CF",1);

  }else if(action == "paid"){
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
    
    display.scrollingText("A-PA",1);
    delay(200);
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

      display.scrollingText("UF",1);
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
  }else if(action == "setwifi"){
    // {"action":"setwifi","index":"1","ssid":"Home173-AIS","key":"1100110011","reconnect":"1"}

    String ssid = doc["ssid"].as<String>();
    String key = doc["key"].as<String>();
    int wifireconn = doc["reconnect"].as<int>();
    int index = doc["index"].as<int>();
    
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
  
  }else if(action == "coinmodule"){
    cfginfo.asset.coinModule = (doc["coinmodule"].as<String>() == "single")?SINGLE:MULTI; //  SINGLE=0, MULTI=1
    (cfginfo.asset.coinModule == MULTI)?pricePerCoin=1:pricePerCoin=10;

    doc.clear();
    doc["response"] = "coinmodule";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Coin_changed";

    (cfginfo.asset.coinModule == MULTI)?doc["desc"]="Change coinModule to: MULTI":doc["desc"]="Change coinModule to: SINGLE";

  }else if(action == "orderid"){

  }else if(action == "assettype"){  
    // {"action":"assettype","assettype":"0"}
    cfginfo.asset.assettype = doc["assettype"].as<int>();
    cfgdata.begin("config",false);
    cfgdata.putInt("assettype",0);
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

    display.scrollingText("A-Pb",1);
  }else if(action == "backend"){

  }else if(action == "stateflag"){ //stateflag is flag for mark action before reboot  ex 1 is for reboot action, 2 for ota action

  }else if(action == "jobcancel"){

    serviceEnd();
    doc.clear();
    doc["response"] = "jobcancel";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Job_Canceled";
    doc["desc"]="Manual cancel job.";
    display.scrollingText("J-CAn",1);
    delay(5000);

  }else if(action == "jobcreate"){
    coinValue = doc["price"].as<int>();
    paymentby = doc["paymentby"].as<int>();  //1 = coin , 2 = qr, 3 = kiosk , 4 = free

    Serial.print("waitFlag: ");
    Serial.println(waitFlag);

    doc.clear();
    doc["response"] = "jobcreate";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="Job_Created";
    doc["desc"]="Manual create job.";
    display.scrollingText("J-Add",1);
    delay(5000);
  }else if(action == "nvsdelete"){
    String msg;

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

    display.scrollingText("n-dEL",1);

    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",3);
    cfgdata.end();
    delay(3000);

    ESP.restart();
  }else if(action == "offline"){
  
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

  }else if(action == "online"){
    resetState();

    doc.clear();
    doc["response"] = "online";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="online";
    doc["desc"]="Asset is in ONLINE mode";       
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

  dryer.powerCtrl(POWER_RLY,MACHINEDC,dryer.TURNOFF);

  serviceTime.stop(serviceTimeID);
  timeLeft.stop(timeLeftID);
  timeRemain = 0;
  coinValue = 0;
  cfgState = 3;
  waitFlag = 0;
  dispflag = 0;
  pauseflag = false;

  firstExtPaid = 0;
  extpaid = 0;
  extraPay = 0;
  disperr="";
  disptxt="";

  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",0);
  cfgdata.putString("orderid","");
  cfgdata.putInt("timeremain",0);
  cfgdata.end();


  backend.merchantID=cfginfo.payboard.merchantid;
  backend.merchantKEY=cfginfo.payboard.merchantkey;
  backend.appkey=cfginfo.payboard.apikey;

  switch(paymentby){
    case 0:
      Serial.printf("Asset comming ONLINE\n");
      break;
    case 1: // by Coin
      Serial.printf("Coin Job Finished.\n");
      break;
    case 2: //by QR
      Serial.printf("Sending QR acknoloedge to backend\n");
      cfgdata.putString("orderid",cfginfo.asset.orderid);
      backend.uri_deviceStart = cfginfo.payboard.apihost + "/v1.0/device/stop";
      rescode = backend.deviceStart(cfginfo.asset.orderid.c_str(),response);

      if(rescode == 200){
        if(response == "success"){
          Serial.printf("Pro1Start backend undated\n");
        } 
      }else{
        Serial.printf("Rescode: %d\n",rescode);
      }
      break;
    case 3: //by Kiosk
      Serial.printf("Kiosk Job finished.\n");
      break;
    case 4: // by Admin
      Serial.printf("Free Job finish.ed\n");
      break;
  }
  Serial.printf("Job Finish.  Poweroff machine soon.\n");
  WebSerial.println("[serviceEnd]->Job Finish. Power off machine soon.");
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
