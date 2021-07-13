#include "hdv70e1.h"

HDV70E1::HDV70E1(){
   
}

void HDV70E1::ctrlRotary(rotaryMODE prog){
  //Default ctrlbytes  0x55,0xaa,0x00,0x00,0x00,0x50,0x50
  Serial.println();
  Serial.println("select is modeRotary");
  int valuein = 0;

  for(int i = 0 ; i <= (prog*5) ; i++){
    if(i == 5){
      if(valuein != prog){
         ctrlbytes[4] += 0x01 ;
         valuein += 1;
         i = 0;
         Serial.print("Round: ");Serial.println(valuein);
         Serial.print("Counin: ");Serial.println(i); 
         Serial.print(" ctrlbytes[4]: ");Serial.println(ctrlbytes[4],HEX); 
         Serial.print(" ctrlbytes[6]: ");Serial.println(ctrlbytes[6],HEX);
         Serial.println();
      }
    }
    ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
    Serial2.write(ctrlbytes, sizeof(ctrlbytes));
    delay(20);
  }
}


void HDV70E1::ctrlButton(buttonMODE btn){
    if(btn == 1){
        Serial.printf("Pressing button: Start/Pause\n");
    }else if(btn == 2){
        Serial.printf("Pressing button: Option\n");
    }

    for(int i = 0 ; i <40 ; i++){
      if(i > 0 && i < 20){
        ctrlbytes[2] = btn;
      }
      if(i > 20 && i < 40){
        ctrlbytes[2] = 0 ;  
      }

      ctrlbytes[6] = 0x50 +  ctrlbytes[4] +  ctrlbytes[2];
      Serial2.write( ctrlbytes, sizeof( ctrlbytes));

      Serial.print("ctrlbytes: ");Serial.println(byte( ctrlbytes[6]));
      Serial.println(i);  Serial.println();
      delay(20);
    }
}


void HDV70E1::runProgram(int pwrPin,int machinePwr,int doorState,rotaryMODE prog){
    int maxtry = 3;
    int retry = 0;
    this->powerCtrl(pwrPin,machinePwr,1); // Turn on machine
    this->ctrlRotary(prog);
    this->ctrlButton(OPTION);
    
    while( (!isDoorClose(doorState)) && (retry != maxtry)){
        Serial.printf("Please close Door");
        retry++;
        delay(1000);
    }
}

void HDV70E1::powerCtrl(int pwrPin, int machinePwr,bool mode){
    if(mode == 1){ // Pwoer ON Machine
        if(!isMachineON(machinePwr)){
            singlePulse(pwrPin,400,HIGH);            
        }
    }else{ // POWER OFF Machine
        if(isMachineON(machinePwr)){
            singlePulse(pwrPin,400,HIGH);
        }
    }
}

void HDV70E1::singlePulse(int pin, int period, bool startlogic){
    digitalWrite(pin,startlogic);
    delay(period);
    digitalWrite(pin,!startlogic);
    delay(period);
}

bool HDV70E1::isMachineON(int pin){
    if(digitalRead(pin)){
        Serial.printf("Machine ON\n");
        return true;
    }else{
        Serial.printf("Machine OFF\n");
        return false;
    }
}

bool HDV70E1::isDoorClose(int pin){
    if(digitalRead(pin)){
        Serial.printf("Door CLOSE\n");
        return true; // Door Close
    }else{
        Serial.printf("Door OPEN\n");
        return false; //Door Open
    }
}


void boardTest(void){
    int outputPIN[] = {POWER_RLY,ENPANEL,ENCOIN,UNLOCK,BUZZ,BOOK_LED,WIFI_LED};

    Serial.printf("Start Selftest board\n");
    for(int i=0;i<7;i++){
        for(int x=0;x<3;x++){
            Serial.printf("Turn io %d ON\n",outputPIN[i]);
            digitalWrite(outputPIN[i],HIGH);
            delay(500);
            Serial.printf("Turn io %d OFF\n",outputPIN[i]);
            digitalWrite(outputPIN[i],LOW);
            delay(500);
        }
    }
}