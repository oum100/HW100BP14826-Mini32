/*
   esp32 firmware OTA
   Date: December 2018
   Author: Chris Joyce <https://github.com/chrisjoyce911/esp32FOTA/esp32FOTA>
   Purpose: Perform an OTA update from a bin located on a webserver (HTTP Only)
*/

#ifndef esp32fota_h
#define esp32fota_h

#include "Arduino.h"
#include <WiFiClientSecure.h>

class esp32FOTA
{
public:
  esp32FOTA(String firwmareType, String firwmareVersion);
  void forceUpdate(String firwmareHost, int firwmarePort, String firwmarePath);
  void execOTA();
  bool execHTTPcheck();
  bool useDeviceID;
  String checkURL;
  String _firwmareVersion;

private:
  String getDeviceID();
  String _firwmareType;
  //String _firwmareVersion;
  String _host;
  String _bin;
  int _port;
};

class secureEsp32FOTA
{
public:
  secureEsp32FOTA(String firwmareType, String firwmareVersion);
  bool execHTTPSCheck();
  int executeOTA();
  void executeOTA_REBOOT();
  String _descriptionOfFirmwareURL;
  char *_certificate;
  WiFiClientSecure clientForOta;
  String _host;
  String _firwmareVersion;

private:
  bool prepareConnection(String locationOfServer);
  String secureGetContent();
  bool isValidContentType(String line);
  String _firwmareType;
  //String _firwmareVersion;
  String locationOfFirmware;
  String _bin;
  int _port;
};

#endif
