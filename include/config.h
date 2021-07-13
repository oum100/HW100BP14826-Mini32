
#ifndef config_h
#define config_h

#include <Arduino.h>
#include <Preferences.h>


struct Product{
    String sku;
    float  price;
    int stime;
};

struct SSID{
    String ssid;
    String key;
};

struct Payboard{
    String uuid;
    String merchantid;
    String merchantkey;
    String apihost;
    String apikey;
    String mqtthost;
    int mqttport;
    String mqttuser;
    String mqttpass;
};

struct Asset{
    String assetid;
    String orderid;
    String mac;
    String model;
    String firmware;
    String user;
    String pass;
    int coinModule;
    int assettype; //***  0=Washer , 1 = Dryer
    String ntpServer1;
    String ntpServer2;
};

struct Backend{
    String apikey;
    String apihost;
    String mqtthost;
    int  mqttport;
    String mqttuser;
    String mqttpass;
};

struct Config{
    String header;
    String deviceid;
    Asset asset;
    Backend backend;
    SSID wifissid[2];
    Payboard payboard;
    Product product[3];
};

enum cointype {SINGLE,MULTI};
enum machinetype {WASHER, DRYER};



void initGPIO(unsigned long long INP, unsigned long long OUTP);
void saveRemainTime(SevenSegmentTM1637 &disp, Preferences &nvdata);

void blinkGPIO(int pin, int btime);

void initCFG(Config &cfg);
void showCFG(Config &cfg);
void getNVCFG(Preferences nvcfg, Config &cfg);

int loadWIFICFG(Preferences nvcfg,Config &cfg);
void printLocalTime();
void WiFiinfo(void);

#endif