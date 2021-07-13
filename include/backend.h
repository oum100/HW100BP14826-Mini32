#ifndef backend_h
#define backend_h
#include <Arduino.h>
#include <PubSubClient.h>


void regCallback(char* topic, byte* payload, unsigned int length);


class CoinTracker{
    public:
        struct DEVinfo{
            String deviceid;
            String model;
            String board;
            String firmware;
            String status;
        };

        String devRegis(DEVinfo devinfo);
        String createJson(DEVinfo devinfo);
        String devGetinfo();
};

class Pboard {
    public:
        String _merchantID;
        String _uuid;
        const char* _mac;
        const char* _model;
        const char* _firmware;
        const char* _mquser;
        const char* _mqpass;
        const char* _mqhost;
        int _mqport = 1883;
                
        void setUUID(const char* uid);
        String  getUUID();
        void setMac(String mac);
        String  getMac();
        void setMerchantId(String mid);
        String getMerchantId();
        void setMqtt(String host, String user, String pass);


        String requestQR();

        void setCallback(PubSubClient &client,MQTT_CALLBACK_SIGNATURE);
        
        void pbRegister(PubSubClient &client);
        //void callback(char* topic, byte* payload, unsigned int length);
        void reconnect();

    private:
        
};

String getdeviceid(void);
int requestPOST(String server, String req, String &res);
int requestGET(String server, String &res);
void pulseGEN2(int object,bool logic, uint qty, int width);

void mqconnect(PubSubClient &client,const char* mqname, const char* user, const char* pass);

#endif