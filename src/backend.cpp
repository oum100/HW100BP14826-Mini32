#include "backend.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define DBprintf Serial.printf


void regCallback(char* topic, byte* payload, unsigned int length){
    DynamicJsonDocument doc(1024);
    
    Serial.println("This is regCallback.");
}

void Pboard::setUUID(const char* uid){
    _uuid = uid;
}

String Pboard::getUUID(){
    return _uuid;
}

void Pboard::setCallback(PubSubClient &client,MQTT_CALLBACK_SIGNATURE){
    client.setCallback(callback);
}


void Pboard::pbRegister(PubSubClient &client){
    String pubRegister = "payboard/register";
    String subRegister = "payboard/"+_merchantID+"/"+_mac;
    String mqttMSG;


    Serial.print("sub: ");
    Serial.println(subRegister);

    Serial.print("pub");
    Serial.println(pubRegister);
    
    //Preparing payload
    StaticJsonDocument <1024> jdoc;
    jdoc["merchantid"]=_merchantID;
    jdoc["mac"]=_mac;
    jdoc["model"]=_model;
    jdoc["firmware"]=_firmware;
    serializeJson(jdoc,mqttMSG);
    Serial.print("This json will sent to payboard:");
    Serial.println(mqttMSG);

    //Preparing MQTT Host connection
    client.setServer(_mqhost,_mqport);
    client.setCallback(regCallback);

    while(!client.connected()){
        client.connect("payboard",_mquser,_mqpass);
    }
    
    client.subscribe(subRegister.c_str());
    //client.publish(pubRegister.c_str(),mqttMSG.c_str());
}


String getdeviceid(void){
    char chipname[13];
    uint64_t chipid = ESP.getEfuseMac();

    snprintf(chipname, 13, "%04X%08X", (uint16_t)(chipid >> 32),(uint32_t)chipid);
    
    return chipname;
}

/* ----------------Control Machine SW ----------------- */
//PowerSW:25, ProgSW:26, StartSW:27
void pulseGEN2(bool logic, uint qty, int width,int object) {
  int i = 0;
  unsigned long time1 = 0;
  unsigned long time2 = 0;
  unsigned long timediff = 0;

  //Serial.println("PulseGen");

  digitalWrite(object, !logic);

  for (i = 1; i <= qty; i++) {
    DBprintf("(Pulse: %d) ", i);
    time1 = millis();
    time2 = millis();
    timediff = time2 - time1;
    digitalWrite(object, logic);
    DBprintf("(Logic: %d ", digitalRead(object));
    while (timediff != width) {
      time2 = millis();
      timediff = time2 - time1;
    }
    DBprintf("Time: %lu )", timediff);
    digitalWrite(object, !logic);
    DBprintf("(Logic: %d ", digitalRead(object));
    time1 = millis();
    time2 = millis();
    timediff = time2 - time1;
    while (timediff != width) {
      time2 = millis();
      timediff = time2 - time1;
    }
    DBprintf("Time: %lu )\n", timediff);
  }
}


int requestPOST(String server, String req, String &res){
  
  HTTPClient client;
  //String responsebody =""; 

  //String ss = "https://cointracker100.herokuapp.com/cointracker/v1.0.0/asset/deviceName";

  //{"\"deviceName\":\"xxxx\""}

  DBprintf(" --- Req to server: %s\n",server.c_str());
  DBprintf(" --- Req body: %s\n",req.c_str());

  client.begin(server);
  client.addHeader("Content-Type", "application/json");
  int rescode = client.POST(req.c_str());

  //responsebody = client.getString();
  res = client.getString();

  DBprintf(" --- Response Code: %d\n",rescode);
  DBprintf(" --- Response Body: %s\n",res.c_str());

  //res = &responsebody;

  client.end();
  return rescode;
}

int requestGET(String server, String &res){
  
  HTTPClient client;
  String responsebody =""; 

  //String ss = "https://cointracker100.herokuapp.com/cointracker/v1.0.0/asset/deviceName";

  //{"\"deviceName\":\"xxxx\""}

  DBprintf(" --- Req to server: %s\n",server.c_str());

  client.begin(server);
  client.addHeader("Content-Type", "application/json");
  int responsecode = client.GET();

  //responsebody = client.getString();
  res = client.getString();

  DBprintf(" --- Response Code: %d\n",responsecode);
  DBprintf(" --- Response Body: %s\n",responsebody.c_str());

  //res = &responsebody;

  client.end();
  return responsecode;
}


String CoinTracker::devRegis(DEVinfo devinfo){
    HTTPClient hclient;
    String apihost="https://cointracker100.herokuapp.com/cointracker/v1.0.0/devices/dev_name";
    String response;


    DynamicJsonDocument doc(512);

    doc["dev_name"]=devinfo.deviceid;
    doc["dev_model"]=devinfo.model;
    doc["dev_firmware"]=devinfo.firmware;
    doc["dev_boardname"]=devinfo.board;
    doc["dev_status"] = "Registered";

    String jsonstr;
    serializeJson(doc,jsonstr);


    requestPOST(apihost,jsonstr,response);
}


void mqconnect(PubSubClient &client,const char* mqname, const char* user, const char* pass){
    Serial.printf("Mqtt connecting ...\n"); 
    while(!client.connected()){
        if(client.connect(mqname,user,pass)){
            Serial.printf("Mqtt connected\n");
            //digitalWrite(GREEN_LED,LOW);
        }else{
            //digitalWrite(GREEN_LED,HIGH);
            Serial.printf(".");
            delay(500);
        }
    }
}