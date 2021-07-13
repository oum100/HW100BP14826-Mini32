#ifndef payboardAPI_h
#define payboardAPI_h

#include <Arduino.h>
#include <Preferences.h>


class payboard{
    public:
        String merchantID;
        String merchantKEY;
        String apphost;
        String appkey;

        String uri_register="https://apis-dv-partner.payboard.cc/v1.0/device/register";
        String uri_requestQR="https://apis-dv-partner.payboard.cc/v1.0/qr/request";
        String uri_deviceStart="https://apis-dv-partner.payboard.cc/v1.0/device/start";
        String uri_deviceStop="https://apis-dv-partner.payboard.cc/v1.0/device/stop";
        String uri_countCoin="https://apis-dv-partner.payboard.cc/v1.0/device/countcoin";

        struct qrOutput{
            String transaction;
            String qrTxt;
            int amount;
        };
        qrOutput qrResult;

        struct countOutput{
            String msg;
            String transaction;
        };
        countOutput countResult;

        void setMerchant(const char* mid, const char* mkey);
        void setHost(const char* host, const char* akey);
        int registerDEV(const char*  macaddr, String &uuid);
        int registerDEV(const char*  macaddr, String &uuid, String &resJson);
        int requestQR(const char *uuid, const char* sku, qrOutput &response);
        int requestQR(const char *uuid, const char* sku, qrOutput &response, String &resJson);
        int deviceStart(const char *trans);
        int deviceStart(const char *trans, String &response);
        int deviceStop(const char *trans);
        int deviceStop(const char *trans, String &response);
        int coinCounter(const char *uuid, int amount, String &trans);
        int coinCounter(const char *uuid, int amount, String &trans, String &resJson);

};


#endif