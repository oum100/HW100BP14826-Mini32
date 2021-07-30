#include <Arduino.h>
#include <SevenSegmentTM1637.h>
#include <Preferences.h>
#include "config.h"
#include <timer.h>
#include <WiFi.h>
#include <backend.h>


String SoftAP_NAME PROGMEM = "BT_" + getdeviceid();
IPAddress SoftAP_IP(192,168,8,20);
IPAddress SoftAP_GW(192,168,8,1);
IPAddress SoftAP_SUBNET(255,255,255,0);


void getnvPbCFG(Preferences nvcfg, Config &cfg){
  Serial.printf("  Getting Payboard Configuration from NV\n");
  nvcfg.begin("config",false);
    cfg.payboard.merchantid = nvcfg.getString("merchantid");
    cfg.payboard.mqttuser = cfg.payboard.merchantid;
    cfg.payboard.merchantkey = nvcfg.getString("merchantkey");
    cfg.payboard.mqttpass = cfg.payboard.merchantkey;
    cfg.payboard.apihost =nvcfg.getString("apihost");
    cfg.payboard.apikey = nvcfg.getString("apikey");
    cfg.payboard.mqtthost = nvcfg.getString("mqtthost");
    cfg.payboard.mqttport = nvcfg.getInt("mqttport");
  Serial.printf("  Completed get Payboard Configuration\n");
  nvcfg.end();
}

void getnvAssetCFG(Preferences nvcfg, Config &cfg){
  Serial.printf("  Getting Asset Configuration from NV\n");
  nvcfg.begin("config",false);
    cfg.asset.assetid = nvcfg.getString("assetid");
    cfg.asset.merchantid = nvcfg.getString("merchantid");
    cfg.asset.orderid = nvcfg.getString("orderid");
    cfg.asset.firmware = nvcfg.getString("firmware");
    cfg.asset.coinModule = nvcfg.getInt("coinModule");
    cfg.asset.assettype = nvcfg.getInt("assettype");
    cfg.asset.user=nvcfg.getString("user");
    cfg.asset.pass=nvcfg.getString("pass");
  Serial.printf("  Completed get Asset Configuration\n");
  nvcfg.end();
}

void getnvBackend(Preferences nvcfg, Config &cfg){
  Serial.printf("  Getting Backend Configuration from NV\n");
  nvcfg.begin("config",false);
    cfg.backend.apihost = nvcfg.getString("apihost");
    cfg.backend.apikey = nvcfg.getString("apikey");
    cfg.backend.mqtthost = nvcfg.getString("mqtthost");
    cfg.backend.mqttpass = nvcfg.getString("mqttpass");
    cfg.backend.mqttport = nvcfg.getInt("mqttport");
    cfg.backend.mqttuser = nvcfg.getString("mqttuser");
  Serial.printf("  Completed get Backend Configuration\n");
  nvcfg.end();
}

void getnvProduct(Preferences nvcfg, Config &cfg){
  Serial.printf("  Getting Product Configuration from NV\n");
  nvcfg.begin("config",false);
    if(nvcfg.isKey("sku1")){
        cfg.product[0].sku = nvcfg.getString("sku1");
        cfg.product[0].price = nvcfg.getFloat("price1");
        cfg.product[0].stime = nvcfg.getInt("stime1");
    }

    if(nvcfg.isKey("sku2")){
        cfg.product[1].sku = nvcfg.getString("sku2");
        cfg.product[1].price = nvcfg.getFloat("price2");
        cfg.product[1].stime = nvcfg.getInt("stime2");
    }

    if(nvcfg.isKey("sku3")){
        cfg.product[2].sku = nvcfg.getString("sku3");
        cfg.product[2].price = nvcfg.getFloat("price3");
        cfg.product[2].stime = nvcfg.getInt("stime3");
    }
  Serial.printf("  Completed get Product Configuration\n");
  nvcfg.end();  
}


void getNVCFG(Preferences nvcfg, Config &cfg){
    Serial.printf("  Execute---getNVCFG Function\n");
    nvcfg.begin("config",false);

        if(nvcfg.isKey("assetid")){
            cfg.asset.assetid = nvcfg.getString("assetid");
        }

        if(nvcfg.isKey("merchantid")){
            cfg.asset.merchantid = nvcfg.getString("merchantid");
        }

        if(nvcfg.isKey("orderid")){
            cfg.asset.orderid = nvcfg.getString("orderid");
        }

        if(nvcfg.isKey("firmware")){
            cfg.asset.firmware = nvcfg.getString("firmware");
        }

        if(nvcfg.isKey("coinModule")){
            cfg.asset.coinModule = nvcfg.getInt("coinModule");
        }

        if(nvcfg.isKey("assettype")){
            cfg.asset.assettype = nvcfg.getInt("assettype");
        }

        if(nvcfg.isKey("merchantid")){
            cfg.payboard.merchantid = nvcfg.getString("merchantid");
            cfg.payboard.mqttuser = cfg.payboard.merchantid;
        }

        if(nvcfg.isKey("merchantkey")){
            cfg.payboard.merchantkey = nvcfg.getString("merchantkey");
            cfg.payboard.mqttpass = cfg.payboard.merchantkey;
        }

        if(nvcfg.isKey("apihost")){
            cfg.payboard.apihost =nvcfg.getString("apihost");
        }

        if(nvcfg.isKey("apikey")){
            cfg.payboard.apikey = nvcfg.getString("apikey");
        }

        if(nvcfg.isKey("mqtthost")){
            cfg.payboard.mqtthost = nvcfg.getString("mqtthost");
        }

        if(nvcfg.isKey("mqttport")){
            cfg.payboard.mqttport = nvcfg.getInt("mqttport");
        }

        if(nvcfg.isKey("sku1")){
            cfg.product[0].sku = nvcfg.getString("sku1");
            cfg.product[0].price = nvcfg.getFloat("price1");
            cfg.product[0].stime = nvcfg.getInt("stime1");
        }else{

        }

        if(nvcfg.isKey("sku2")){
            cfg.product[1].sku = nvcfg.getString("sku2");
            cfg.product[1].price = nvcfg.getFloat("price2");
            cfg.product[1].stime = nvcfg.getInt("stime2");
        }

        if(nvcfg.isKey("sku3")){
            cfg.product[2].sku = nvcfg.getString("sku3");
            cfg.product[2].price = nvcfg.getFloat("price3");
            cfg.product[2].stime = nvcfg.getInt("stime3");
        }
    nvcfg.end();
}

void showCFG(Config &cfg){
    Serial.println("   Excute---showCFG Function.");
    Serial.println("--------------------------------");
    Serial.printf("Header: %s\n",cfg.header.c_str());
    Serial.printf("DeviceID: %s\n",cfg.deviceid.c_str());

    Serial.printf("\nAsset Configuration\n");
    Serial.printf("  AssetID: %s\n",cfg.asset.assetid.c_str());
    Serial.printf("  MerchantID: %s\n",cfg.asset.merchantid.c_str());
    Serial.printf("  Orderid: %s\n",cfg.asset.orderid.c_str());
    Serial.printf("  CoinType: %d\n",cfg.asset.coinModule);
    Serial.printf("  AssetType: %d\n",cfg.asset.assettype);
    Serial.printf("  Firmware: %s\n",cfg.asset.firmware.c_str());
    Serial.printf("  MacAddress: %s\n",cfg.asset.mac.c_str());
    Serial.printf("  Model: %s\n",cfg.asset.model.c_str());
    Serial.printf("  User: %s\n",cfg.asset.user.c_str());
    Serial.printf("  Pass: %s\n",cfg.asset.pass.c_str());

    Serial.printf("\nBackend Configuration\n");
    Serial.printf("  Apihost: %s\n",cfg.backend.apihost.c_str());
    Serial.printf("  Apikey: %s\n",cfg.backend.apikey.c_str());
    Serial.printf("  Mqtthost: %s\n",cfg.backend.mqtthost.c_str());
    Serial.printf("  Mqttport: %d\n",cfg.backend.mqttport);
    Serial.printf("  Mqttuser: %s\n",cfg.backend.mqttuser.c_str());
    Serial.printf("  Mqttpass: %s\n",cfg.backend.mqttpass.c_str());

    Serial.printf("\nPayboard Configuration\n");
    Serial.printf("  MerchantId: %s\n",cfg.payboard.merchantid.c_str());
    Serial.printf("  MerchantKey: %s\n",cfg.payboard.merchantkey.c_str());
    Serial.printf("  ApiHost: %s\n", cfg.payboard.apihost.c_str());
    Serial.printf("  Apikey: %s\n",cfg.payboard.apikey.c_str());
    Serial.printf("  Mqtthost: %s\n",cfg.payboard.mqtthost.c_str());
    Serial.printf("  Mqttport: %d\n",cfg.payboard.mqttport);
    Serial.printf("  Mqttuser: %s\n",cfg.payboard.mqttuser.c_str());
    Serial.printf("  Mqttpass: %s\n",cfg.payboard.mqttpass.c_str());
    
    //int sz = sizeof(cfg.product);
    //Serial.println(sz);
    Serial.printf("\nProduct Information\n");
    for(int i=0;i<3;i++){
        Serial.printf("  SKU[%d]: %s\n",i,cfg.product[i].sku.c_str());
        Serial.printf("  Price[%d]: %.2f\n",i,cfg.product[i].price);
        Serial.printf("  Stime[%d]: %d\n",i,cfg.product[i].stime);
    }

    // sz = sizeof(cfg.wifissid);
    // Serial.println(sz);
    Serial.printf("\nWiFi Information\n");
    for(int i=0;i<2;i++){
        Serial.printf("  SSID[%d]: %s\n",i,cfg.wifissid[i].ssid.c_str());
        Serial.printf("  Key[%d]: %s\n",i,cfg.wifissid[i].key.c_str());
    }
    Serial.println("--------------------------------");
    Serial.println();
}


void initGPIO(unsigned long long INP, unsigned long long OUTP){
  gpio_config_t io_config;

  Serial.printf("  Execute---Initial GPIO Functio\n");
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



void initCFG(Config &cfg){

    cfg.header = "EITC";
    cfg.deviceid = "";

    cfg.payboard.merchantid = "1000000104";
    cfg.payboard.merchantkey = "mki9lvhjpp1xt4jxgjdjqxuhx2ihucgkgz9ledsylsu7terwtsnibhhjzrnsiiig";
    cfg.payboard.apihost = "https://apis-dv-partner.payboard.cc";
    cfg.payboard.apikey = "558iim6kjkre38dxk2uj8i6yuew6gwa9";
    cfg.payboard.mqtthost ="mq3.payboard.cc";
    cfg.payboard.mqttport = 1883;
    cfg.payboard.mqttuser = cfg.payboard.merchantid;
    cfg.payboard.mqttpass= cfg.payboard.merchantkey;

    Serial.printf("  MerchantID: %s\n",cfg.payboard.merchantid.c_str());


    cfg.asset.assetid="";
    cfg.asset.merchantid ="07202100001";
    cfg.asset.orderid="";
    cfg.asset.assettype=DRYER; // 0 = WASHER, 1 = DRYER
    cfg.asset.coinModule=SINGLE; //  SINGLE=0, MULTI=1
    cfg.asset.user="admin";
    cfg.asset.pass="ad1@#min";
    cfg.asset.mac = "";
    cfg.asset.model ="HaierDryer_V2.0.2";
    cfg.asset.firmware = "1.0.0";

    cfg.backend.apihost="https://cointracker100.herokuapp.com/cointracker/v1.0.0/devices";
    cfg.backend.apikey="87cdf9229caf9a7fa3fd1403bcc5dd97";
    cfg.backend.mqtthost="flipup.net";
    cfg.backend.mqttuser="sammy";
    cfg.backend.mqttpass="password";
    cfg.backend.mqttport = 1883;    

    // if(cfg.asset.assettype == 0){//WASHER
    //   cfg.product[0].sku = "P1";
    //   cfg.product[0].price = 30;
    //   cfg.product[0].stime = 25;

    //   cfg.product[1].sku = "P2";
    //   cfg.product[1].price = 40;
    //   cfg.product[1].stime = 35;

    //   cfg.product[2].sku = "P3";
    //   cfg.product[2].price = 50;
    //   cfg.product[2].stime = 40;   
    // }else{
      cfg.product[0].sku = "P1";
      cfg.product[0].price = 40;
      cfg.product[0].stime = 60;

      cfg.product[1].sku = "P2";
      cfg.product[1].price = 50;
      cfg.product[1].stime = 75;

      cfg.product[2].sku = "P3";
      cfg.product[2].price = 60;
      cfg.product[2].stime = 90;
    // }
    

    // for(int i=0;i<3;i++){
    //     cfg.product[i].sku="";
    //     cfg.product[i].price=0;
    //     cfg.product[i].stime=0;
    // }
}


void WiFiinfo(void){
      Serial.printf("\nWiFi Connect to\n");
      Serial.print("   SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("   IP: ");
      Serial.println(WiFi.localIP());
      Serial.println();

      WiFi.softAPConfig(SoftAP_IP, SoftAP_GW, SoftAP_SUBNET);
      WiFi.softAP( SoftAP_NAME.c_str(),"1100110011",8,false,2 );
      Serial.print("   Soft-AP Name : ");
      Serial.println(WiFi.softAPSSID());
    
      Serial.print("   Soft-AP IP address : ");
      Serial.println(WiFi.softAPIP());
}


int loadWIFICFG(Preferences nvcfg,Config &cfg){  
  int inx=0;

  Serial.printf("  Execute---loadWIFICFG Function\n");
  nvcfg.begin("wificfg",false);
    if(nvcfg.isKey("ssid1")){
      cfg.wifissid[0].ssid = nvcfg.getString("ssid1");
      cfg.wifissid[0].key = nvcfg.getString("key1");
      Serial.printf("     Found ssid1: %s\n",cfg.wifissid[0].ssid.c_str());
      inx++;
    }
    if(nvcfg.isKey("ssid2")){
      cfg.wifissid[1].ssid = nvcfg.getString("ssid2");
      cfg.wifissid[1].key = nvcfg.getString("key2");
      Serial.printf("     Found ssid2: %s\n",cfg.wifissid[1].ssid.c_str());
      inx++;
    }
  nvcfg.end();
    return inx;
}


void saveRemainTime(SevenSegmentTM1637 &disp, Preferences &nvdata){


}

void blinkGPIO(int pin, int btime){
    if(digitalRead(pin)){         
        digitalWrite(pin,LOW);
        delay(btime);
    }else{
        digitalWrite(pin,HIGH);
        delay(btime);
    }
}

void printLocalTime(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}