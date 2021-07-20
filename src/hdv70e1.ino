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
int stime[3]={60,75,90};

int keyPress=0;
int errorCode = 0;
String errorDesc = "";

//Timer 
Timer serviceTime, waitTime, timeLeft;
int8_t serviceTimeID,waitTimeID,timeLeftID;


time_t tnow;


WiFiMulti wifiMulti;

WiFiClient espclient;
PubSubClient mqclient(espclient);

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
//Remainding time 


secureEsp32FOTA esp32OTA("HDV70E1", "1.0.0");




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

  //display.print("FE");
  display.begin();
  display.setBacklight(30);
  display.print("F0"); 
  

  //******* Initial GPIO
  initGPIO(INPUT_SET,OUTPUT_SET);

  //******* Initial Interrupt
  init_interrupt();
  delay(500);


  //******* WiFi Setting
  display.print("F1");
  WiFi.mode(WIFI_AP_STA);
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
  WiFiinfo();

  display.print("F2");
  
  //initCFG(cfginfo); 
  Serial.printf("Load default configuration.\n");
  initCFG(cfginfo); 
  
  cfginfo.deviceid = getdeviceid();
  cfginfo.asset.mac = WiFi.macAddress();
  Serial.println();
  Serial.printf("Device ID: %s\n",cfginfo.deviceid.c_str());
  Serial.printf("MacAddress: %s\n",cfginfo.asset.mac.c_str());

 

  //**** Getting Configuration from NV-RAM
  cfgdata.begin("config",false);
  if(cfgdata.isKey("merchantid")){
    cfginfo.payboard.merchantid = cfgdata.getString("merchantid");
  }else{
    cfginfo.payboard.merchantid = "1000000104";  //this is default mmerchant id
  }
  if(cfgdata.isKey("sku1")){
    getnvProduct(cfgdata,cfginfo);
  }
  cfgdata.end();
  

  //int sz = sizeof(cfginfo.product)/sizeof(cfginfo.product[0]);
  
  int sz = 3;
  Serial.printf(" Number Product %d\n",sz);
  for(int i=0;i<sz;i++){
    price[i] = int(cfginfo.product[i].price);
    //stime[i] = cfginfo.product[i].stime;
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
      display.print("E4");
    }
  }
  cfgdata.end();

  showCFG(cfginfo);

  pbPubTopic = pbPubTopic  + String(cfginfo.payboard.merchantid) +"/"+ String(cfginfo.payboard.uuid);
  pbSubTopic = pbSubTopic + String(cfginfo.payboard.merchantid) +"/"+ String(cfginfo.payboard.uuid);


  //**** Connecting MQTT
  display.print("F3");
  pbBackendMqtt();

  //Setting  Time from NTP Server
  display.print("F4");
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
  display.print("F5");
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
      doc["state"]="Rebooted";
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
          mqflipup.publish(pbPubTopic.c_str(),jsonmsg.c_str());
        }
      #endif

      cfgdata.putInt("stateflag",0);
      stateflag = 0;
      Serial.printf("stateflag after: %d\n",stateflag);
      cfgState = 3;
    }else if(stateflag ==2){ // After Action OTA
      doc["response"]="ota";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["state"]="Updated";
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
          mqflipup.publish(pbPubTopic.c_str(),jsonmsg.c_str());
        }
      #endif


      cfgdata.putInt("stateflag",0);
      stateflag = 0;
      Serial.printf("stateflag after: %d\n",stateflag);
      cfgState=3;
    }else if(stateflag == 5){ // Last Service not finish but may be power off.
      display.print("PE"); //Power Outage Event
      cfgState = stateflag;
      dispflag = 1;

      timeRemain = cfgdata.getInt("timeremain",0);  //Get timeRemain
      if(timeRemain >0){
        HDV70E1 dryer;

        Serial.printf("Resume job for orderID: %s for [%d] minutes remain.\n",cfginfo.asset.orderid.c_str(),timeRemain);

        if(!dryer.isMachineON(MACHINEDC)){
          digitalWrite(ENPANEL,LOW);
          dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
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

  display.print("F6");
  Serial.printf("\n****************************************\n");
  Serial.printf("\nSystem Ready for service.\n");    
  delay(500);   





  
  //selftest();
}
//----------------------------------- END of Setup Function Here -----------------------------------




//----------------------------------- Start LOOP Function Here -----------------------------------
void loop() {

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
            waitTimeID = waitTime.after(60*1000*0.166,prog1start);
            //prog1start();
          }else if( (coinValue == price[1]) &&  (waitFlag == 1)   ) {  // For program 50 Bath 75Mins
            waitFlag++;
            //extraPay=2;
            waitTime.stop(waitTimeID);

            timeRemain = stime[1];
            //timeRemain = stime[0]+ extTimePerCoin;
            waitTimeID = waitTime.after(60*1000*0.166,prog1start);
            //prog1start();
            Serial.printf("Pay more 1 coinValue: %d\n",coinValue);
            Serial.printf("Check TimeRemain-1: %d\n",timeRemain);
          }else if( (coinValue == price[2]) && (waitFlag == 2) ) {  // For program 60 Bath 90Mins
            digitalWrite(ENCOIN,LOW);
            waitFlag++;
            //extraPay = 3;
            waitTime.stop(waitTimeID);

            timeRemain = stime[2];
            //timeRemain = stime[0]+ (2* extTimePerCoin);
            prog1start();
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
                timeLeft.resume(timeLeftID);
                pauseflag = false;
              }
            }else{ // LOW = Door Open
              if(pauseflag == false){
                Serial.printf("Timer pause, remain is : %d\n",timeRemain);
                display.print("PU");
                serviceTime.pause(serviceTimeID);
                timeLeft.pause(timeLeftID);
                pauseflag = true;
              }
            }
          }
          //---------------End pause and resume  ------------------


          //-------------- Extrapay to exten time -----------------

          //Serial.printf("Extrapay now: %d\n",extraPay);
          if(paymentby == 1){ // by Coin
            if( (coinValue > price[0]+extpaid) && (waitFlag ==1) ){
              waitFlag ++;
              extraPay = 2;
              //waitTime.stop(waitTimeID);
              timeLeft.stop(timeLeftID);
              serviceTime.stop(serviceTimeID);

              extpaid = coinValue - (price[0]);        
              timeRemain = timeRemain + extTimePerCoin;

              Serial.printf("1st extraPay is %d\n",extpaid);
              Serial.printf("1st set new time: %d\n",timeRemain);
              waitTimeID = waitTime.after(60*1000*0.166,prog1start);
              //waitTimeID = waitTime.after(60*1000*0.166,prog2start);
              
            }else if( (coinValue > price[0]+extpaid) && (waitFlag ==2)) {
      
              digitalWrite(ENCOIN,LOW);
              extraPay = 3;
              waitTime.stop(waitTimeID); 
              timeLeft.stop(timeLeftID);
              serviceTime.stop(serviceTimeID);

              if(firstExtPaid){
                extpaid = coinValue - (price[0]-extpaid);
                firstExtPaid = 0;
              }else{
                extpaid = coinValue = price[0];
                firstExtPaid = 0;
              }
              
              timeRemain = timeRemain + extTimePerCoin;
              
              Serial.printf("2nd extraPay is %d\n",extpaid);
              Serial.printf("2nd set new time: %d\n",timeRemain);
              //prog3start();
              prog1start();
            }
          }
          //-------------- End Extrapay to exten time -----------------
          

          //----------------- Display operation --------------------
          display.setColonOn(0);
          if(timeRemain <10){
            String txt;
            txt = "0"+String(timeRemain);
            display.print(txt);
          }else{
            display.print(timeRemain);
          } 
          delay(1000);
          display.animation3(display,300,2);
          //----------------- END Display operation --------------------
          
          //display.scrollingText(String(timeRemain+"--").c_str(),1);
          break;
      case 6:
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








void prog2start(){ // 1st Extra pay
  payboard backend;
  String  response;
  int rescode;


  Serial.printf("Prog2: Extend time 15 mintures\n");
  //Send update to backend
  //Serial.printf("Update to backend and 10 bath\n");


  serviceTimeID = serviceTime.after(60*1000*timeRemain,serviceEnd);
  timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
  cfgState = 5;
  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",cfgState);
  cfgdata.putInt("timeremain",timeRemain);
  cfgdata.end();
}




void prog3start(){ //2nd Extra Pay
  payboard backend;
  String  response;
  int rescode;

  Serial.printf("Prog3: Extend time 15 mintures\n");

  backend.merchantID=cfginfo.payboard.merchantid;
  backend.merchantKEY=cfginfo.payboard.merchantkey;
  backend.appkey=cfginfo.payboard.apikey;

  while(!WiFi.isConnected()){
    wifiMulti.run();
  }



  //Send update to backend
  //Serial.printf("Update to backend and 10 bath\n");


  serviceTimeID = serviceTime.after(60*1000*timeRemain,serviceEnd);
  timeLeftID = timeLeft.every(60*1000*1,serviceLeft);

  cfgState = 5;
  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",cfgState);
  cfgdata.putInt("timeremain",timeRemain);
  cfgdata.end();
}






void prog1start(){

  payboard backend;
  String response;
  int rescode;
  HDV70E1 dryer;

  //digitalWrite(ENCOIN,LOW); // Disable Coin Module

  Serial.printf("Starting Prog1Start , Paymentby %d\n",paymentby);

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
        Serial.printf("[Prog1Start]->Update extrapay to payboard for %d bath\n",extpaid);
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
  if(dryer.isMachineON(MACHINEDC)){
    Serial.printf("Extend time for to started job\n");
    serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
    timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
  }else{
    Serial.printf("On service of Program-1 for %d minutes\n",timeRemain);
    int dryercode = dryer.runProgram(POWER_RLY,MACHINEDC,DSTATE,dryer.MIN120,display,errorCode);
    if(dryercode == 1){
      serviceTimeID=serviceTime.after((60*1000*timeRemain),serviceEnd);
      timeLeftID = timeLeft.every(60*1000*1,serviceLeft);
    }else{
      //Notice Error to backend
      //Power On machine, but machine not on.
      Serial.print("[Prog1Start]->Power on machine. But machine not response command. (not turn on)\n");
    }

    cfgState = 5;
    cfgdata.begin("config",false);
    cfgdata.putInt("stateflag",cfgState);
    cfgdata.putInt("timeremain",timeRemain);
    cfgdata.end();    
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
    display.print("C3");

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


    //String trans = doc["orderNo"].as<String>();
    cfginfo.asset.orderid = doc["orderNo"].as<String>();
    // cfgdata.begin("config",false);
    // cfgdata.putString("orderid",cfginfo.asset.orderid);
    // cfgdata.end();
  
    Serial.printf(" [PAID]->Customer paid for: %d\n",coinValue);
    Serial.printf(" [PAID]->Orderid: %s\n",cfginfo.asset.orderid.c_str());
    doc.clear();
    doc["response"] = "paid";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;  
    doc["state"]="accepted";
    doc["desc"]="accepted orderid: " + cfginfo.asset.orderid;

    //coinValue = paidprice;
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
    }
    //doc["state"]=cfgState;
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
      mqflipup.publish(pbPubTopic.c_str(),jsonmsg.c_str());
    #endif


    delay(500);    
    
    ESP.restart();

  }else if(action == "ota"){
    WiFiClientSecure clientForOta;

    esp32OTA._host="www.flipup.net"; //e.g. example.com
    esp32OTA._descriptionOfFirmwareURL="/firmware/HDV70E1/firmware.json"; //e.g. /my-fw-versions/firmware.json
    //esp32OTA._certificate=test_root_ca;
    esp32OTA.clientForOta=clientForOta;
  
    bool shouldExecuteFirmwareUpdate=esp32OTA.execHTTPSCheck();
    if(shouldExecuteFirmwareUpdate){
      cfginfo.asset.firmware = esp32OTA._firwmareVersion.c_str();      
      //saveCFG(cfginfo,LITTLEFS);
      
      //set stateflag = 2 flag
      cfgdata.begin("config",false);
      cfgdata.putInt("stateflag",2);
      cfgdata.end();

      doc.clear();
      doc["response"] = "ota";
      doc["merchantid"]=cfginfo.payboard.merchantid;
      doc["uuid"]=cfginfo.payboard.uuid;
      doc["firmware"] = esp32OTA._firwmareVersion;
      doc["state"] = "accepted";
      doc["disc"] = "Firmware upgrading then rebooting in few second."; 
      serializeJson(doc,jsonmsg);
      Serial.print("Jsonmsg: ");Serial.println(jsonmsg);
      Serial.print("Ver: "); Serial.println(esp32OTA._firwmareVersion);

      Serial.println("Firmware updating, It's will take few second");
      esp32OTA.executeOTA();
    }else{
      doc["merchanttid"] = cfginfo.payboard.merchantid;
      doc["uuid"] = cfginfo.payboard.uuid;
      doc["response"] = "ota";
      doc["state"] = "FAILED";
      doc["desc"] = "Firmware update failed, version may be the same."; 
    }

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
      mqflipup.publish(pbPubTopic.c_str(),jsonmsg.c_str());
    #endif

    delay(500); 

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
      doc["state"]="Changed";
      doc["desc"]="setWiFi conpleted but will effect next boot.";
    }
  
  }else if(action == "coinmodule"){
    cfginfo.asset.coinModule = (doc["coinmodule"].as<String>() == "single")?SINGLE:MULTI; //  SINGLE=0, MULTI=1
    (cfginfo.asset.coinModule == MULTI)?pricePerCoin=1:pricePerCoin=10;

    doc.clear();
    doc["response"] = "coinmodule";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="changed";

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
    doc["state"]="changed"; 
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
        cfginfo.payboard.mqttport = doc["mqttportt"].as<int>();
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
    doc["state"]="changed"; 
    if(merchantflag){
      mqclient.disconnect();
      #ifdef FLIPUPMQTT
        mqflipup.disconnect();
      #endif
    }

    
  }else if(action == "backend"){

  }else if(action == "stateflag"){ //stateflag is flag for mark action before reboot  ex 1 is for reboot action, 2 for ota action

  }else if(action == "jobcancel"){
    //stateflag = 0;
    //orderid ="";
    //cfgStatte = 3;
    //waitFlag =0;
    //dispflag = 0;

  }else if(action == "jobcreate"){
    coinValue = doc["price"].as<int>();
    paymentby = doc["paymentby"].as<int>();  //1 = coin , 2 = qr, 3 = kiosk , 4 = free

    Serial.print("waitFlag: ");
    Serial.println(waitFlag);

    doc.clear();
    doc["response"] = "jobcreate";
    doc["merchantid"]=cfginfo.payboard.merchantid;
    doc["uuid"]=cfginfo.payboard.uuid;
    doc["state"]="created";
    doc["desc"]="Manual create job.";

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
    doc["state"]="deleted";
    doc["desc"]=msg;    
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
    mqflipup.publish(pbPubTopic.c_str(),jsonmsg.c_str());
  #endif
  delay(500);
}


#ifdef FLIPUPMQTT
  void fpCallback(char* topic, byte* payload, unsigned int length){
    pbCallback(topic,payload,length);
  }
#endif


void pbBackendMqtt(){
    if(!mqclient.connected()){
      mqclient.setServer(cfginfo.payboard.mqtthost.c_str(),cfginfo.payboard.mqttport);
      mqclient.setCallback(pbCallback);

      
      pbSubTopic = "payboard/" + String(cfginfo.payboard.merchantid) + "/" + String(cfginfo.payboard.uuid);
    
      Serial.printf("Backend Mqtt connecting ...");
      while(!mqclient.connect(cfginfo.deviceid.c_str(),cfginfo.payboard.mqttuser.c_str(), cfginfo.payboard.mqttpass.c_str())){
        Serial.printf(".");
        delay(500);
      }
      Serial.printf("connected\n");
      mqclient.subscribe(pbSubTopic.c_str());
      Serial.printf("   Subscribe Topic: %s\n",pbSubTopic.c_str());
    }        
}



#ifdef FLIPUPMQTT
void fpBackendMqtt(){
    if(!mqflipup.connected()){
      mqflipup.setServer(cfginfo.backend.mqtthost.c_str(),cfginfo.backend.mqttport);
      mqflipup.setCallback(fpCallback);

      pbSubTopic = "payboard/" + String(cfginfo.payboard.merchantid) + "/" + String(cfginfo.payboard.uuid);
    
      Serial.printf("Backend Mqtt connecting ...");
      while(!mqflipup.connect(cfginfo.deviceid.c_str(),cfginfo.backend.mqttuser.c_str(), cfginfo.backend.mqttpass.c_str())){
        Serial.printf(".");
        delay(500);
      }
      Serial.printf("connected\n");
      mqflipup.subscribe(pbSubTopic.c_str());
      Serial.printf("   Subscribe Topic: %s\n",pbSubTopic.c_str());
    }        
}
#endif




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
  firstExtPaid = 0;
  extpaid = 0;
  extraPay = 0;
  // extTime1 = 0;
  // extTime2 = 0;
  

  cfgdata.begin("config",false);
  cfgdata.putInt("stateflag",0);
  cfgdata.putString("orderid","");
  cfgdata.putInt("timeremain",0);
  cfgdata.end();


  backend.merchantID=cfginfo.payboard.merchantid;
  backend.merchantKEY=cfginfo.payboard.merchantkey;
  backend.appkey=cfginfo.payboard.apikey;

  switch(paymentby){
    case 1: // by Coin
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
      break;
    case 4: // by Admin
      break;
  }
  Serial.printf("Job Finish.  Poweroff machine soon.\n");


}
