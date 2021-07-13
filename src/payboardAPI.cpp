
#include <Arduino.h>
#include <HTTPClient.h>
#include "payboardAPI.h"
#include <ArduinoJson.h>


void payboard::setMerchant(const char* mid, const char* mkey){
    this->merchantID = mid;
    this->merchantKEY = mkey;
}

void payboard::setHost(const char* host, const char* akey){
    this->appkey = akey;
    this->apphost = host;
}


int payboard::registerDEV(const char*  macaddr, String &uuid){
    String rjson;
    return registerDEV(macaddr,uuid,rjson);
}

int payboard::registerDEV(const char*  macaddr, String &uuid, String &resJson){
    //This function will return only uuid from backend service.
    HTTPClient http;
    String reqbody;
    String resbody;
    StaticJsonDocument<150> doc;

    
    http.begin(this->uri_register);
    http.addHeader("Content-Type","application/json");
    http.addHeader("merchantid",this->merchantID);
    http.addHeader("merchantkey",this->merchantKEY);
    http.addHeader("Authorization",this->appkey);

    doc["macaddress"]= macaddr;
    serializeJson(doc,reqbody);
    //msgbody = "{\"deviceName\":\"" + String(macaddr) + "\"}";

    // Serial.print("URI: "); 
    // Serial.println(this->uri_register);

    // Serial.print("merchantid: ");
    // Serial.println(this->merchantID);

    // Serial.print("merchantKey: ");
    // Serial.println(this->merchantKEY);  

    // Serial.print("appkey: ");
    // Serial.println(this->appkey);

    // Serial.print("reqbody: ");
    // Serial.println(reqbody);


    int rescode = http.POST(reqbody);
    
    if(rescode == 200){
        resbody = http.getString();
        // Serial.print("resbody: ");
        // Serial.println(resbody);
        http.end();

        doc.clear();
        DeserializationError error = deserializeJson(doc,resbody);
       
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return 500;
        }else{
            if(doc["StatusCode"] == "0000"){
                uuid = doc["uuid"].as<String>();
                resJson = resbody;
                // Serial.print("UUID: ");
                return rescode;
            }
        }
    }else{
        Serial.print("[Api Rescode]: ");
        Serial.println(rescode);
        Serial.print("Description: ");
        Serial.println(http.getString());
        http.end();
        return rescode;
    }
}


int payboard::requestQR(const char* uuid, const char* sku, qrOutput &response){
    String rjson;
    return requestQR(uuid,  sku, response,rjson);
}

int payboard::requestQR(const char* uuid, const char* sku, qrOutput &response,String &resJson){
    HTTPClient http;
    String reqbody;
    String resbody;
    StaticJsonDocument<1024> doc;

    http.begin(this->uri_requestQR);
    http.addHeader("Content-Type","application/json");
    http.addHeader("merchantid",this->merchantID);
    http.addHeader("merchantkey",this->merchantKEY);
    http.addHeader("Authorization",this->appkey);

    doc["uuid"]= uuid;
    doc["sku"]=sku;
    serializeJson(doc,reqbody);
    //msgbody = "{\"deviceName\":\"" + String(macaddr) + "\"}";

    int rescode = http.POST(reqbody);

    if(rescode == 200){
        resbody = http.getString();
        // Serial.print("resbody: ");
        // Serial.println(resbody);
        http.end();

        doc.clear();
        DeserializationError error = deserializeJson(doc,resbody);
       
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return 500;
        }else{
            if(doc["StatusCode"] == "0000"){
                response.transaction = doc["ResultValues"]["transactionId"].as<String>();
                response.qrTxt = doc["ResultValues"]["qrTxt"].as<String>();
                response.amount = doc["ResultValues"]["amount"].as<int>();
                resJson = resbody;
                //Serial.println(resbody);
                return rescode;
            }
        }
    }else{
        Serial.print("requestQR API: ");
        Serial.print(rescode + "Desc: ");
        Serial.println(http.getString());
        http.end();
        return rescode;
    }
}



int payboard::deviceStart(const char *trans){
    String rjson;

    return deviceStart(trans,rjson);
}

int payboard::deviceStart(const char *trans,String &response){
    HTTPClient http;
    String reqbody;
    String resbody;
    StaticJsonDocument<100> doc;

    http.begin(this->uri_deviceStart);
    http.addHeader("Content-Type","application/json");
    http.addHeader("merchantid",this->merchantID);
    http.addHeader("merchantkey",this->merchantKEY);
    http.addHeader("Authorization",this->appkey);

    doc["transactionid"]= trans;
    serializeJson(doc,reqbody);
    //msgbody = "{\"deviceName\":\"" + String(macaddr) + "\"}";

    int rescode = http.POST(reqbody);

    if(rescode == 200){
        resbody = http.getString();
        // Serial.print("resbody: ");
        // Serial.println(resbody);
        http.end();

        doc.clear();
        DeserializationError error = deserializeJson(doc,resbody);
       
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return 500;
        }else{
            if(doc["StatusCode"] == "0000"){
                response = doc["Message"].as<String>();
                //Serial.println(resbody);
                return rescode;
            }
        }
    }else{
        Serial.print("deviceStart API: ");
        Serial.print(rescode + "Desc: ");
        Serial.println(http.getString());
        http.end();
        return rescode;
    }
}


int payboard::deviceStop(const char *trans){
    String rjson;

    return deviceStop(trans,rjson);
}


int payboard::deviceStop(const char *trans,String &response){
    HTTPClient http;
    String reqbody;
    String resbody;
    StaticJsonDocument<100> doc;

    http.begin(this->uri_deviceStop);
    http.addHeader("Content-Type","application/json");
    http.addHeader("merchantid",this->merchantID);
    http.addHeader("merchantkey",this->merchantKEY);
    http.addHeader("Authorization",this->appkey);

    doc["transactionid"]= trans;
    serializeJson(doc,reqbody);
    //msgbody = "{\"deviceName\":\"" + String(macaddr) + "\"}";

    int rescode = http.POST(reqbody);

    if(rescode == 200){
        resbody = http.getString();
        // Serial.print("resbody: ");
        // Serial.println(resbody);
        http.end();

        doc.clear();
        DeserializationError error = deserializeJson(doc,resbody);
       
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return 500;
        }else{
            if(doc["StatusCode"] == "0000"){
                response = doc["Message"].as<String>();
                //Serial.println(resbody);
                return rescode;
            }
        }
    }else{
        Serial.print("deviceStop API: ");
        Serial.print(rescode + "Desc: ");
        Serial.println(http.getString());
        http.end();
        return rescode;
    }
}


int payboard::coinCounter(const char *uuid, int amount,String &trans){
    String rjson;
    return coinCounter(uuid,amount,trans,rjson);
}

int payboard::coinCounter(const char *uuid, int amount,String &trans, String &resJson){
    HTTPClient http;
    String reqbody;
    String resbody;
    StaticJsonDocument<200> doc;

    Serial.println("************* payboard coinCounter function **************");
    Serial.print("uuid now: "); Serial.println(uuid);
    Serial.printf("amount: %d\n",amount);
    Serial.print("merchantid: ");Serial.println(this->merchantID);
    Serial.print("merchantKey: ");Serial.println(this->merchantKEY);
    Serial.print("appKey: ");Serial.println(this->appkey);

    http.begin(this->uri_countCoin);
    http.addHeader("Content-Type","application/json");
    http.addHeader("merchantid",this->merchantID);
    http.addHeader("merchantkey",this->merchantKEY);
    http.addHeader("Authorization",this->appkey);

    doc["uuid"]= uuid;
    doc["amount"]=amount;
    serializeJson(doc,reqbody);
    //msgbody = "{\"deviceName\":\"" + String(macaddr) + "\"}";

    int rescode = http.POST(reqbody);

    if(rescode == 200){
        resbody = http.getString();
        Serial.print("resbody: ");
        Serial.println(resbody);
        http.end();

        doc.clear();
        DeserializationError error = deserializeJson(doc,resbody);
       
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return 500;
        }else{
            if(doc["StatusCode"] == "0000"){
                //response.msg = doc["Message"].as<String>();
                trans = doc["ResultValues"]["transactionId"].as<String>();
                resJson = resbody;
                Serial.println(trans);
                return rescode;
            }
        }
    }else{
        Serial.print("countCoin API: ");
        Serial.print(rescode + "Desc: ");
        Serial.println(http.getString());
        http.end();
        return rescode;
    }
}