#include "hdv70e1.h"
#include "driver/gpio.h"
#include "esp32fota.h"
#include <ArduinoJson.h>
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

#include <PubSubClient.h>
#include "backend.h"

#include <nvs_flash.h>

#include "payboardAPI.h"