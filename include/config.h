#include <Arduino.h>
#include <Preferences.h>
#ifndef config_h
#define config_h


String wifiSSID1 = "Home173-AIS";
String wifiPASS1 = "1100110011";
String wifiSSID2 = "";
String wifiPASS2 = "";
String ntpServer1 = "0.th.pool.ntp.org";
String ntpServer2 = "1.th.pool.ntp.org";

struct WIFICFG{

};

void loadWIFICFG(Preferences cfg);
void initGPIO(unsigned long long INP, unsigned long long OUTP);
void saveRemainTime(SevenSegmentTM1637 &disp, Preferences &nvdata);

#endif