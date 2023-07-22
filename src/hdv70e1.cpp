#include "hdv70e1.h"
#include "animation.h"

HDV70E1::HDV70E1(){
   
}


void HDV70E1::showPanel(){
    Serial.printf("[showPanel]->activate panel\n");
    for(int i=0; i<5;i++){
        ctrlbytes[2] = 0;
        ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
        Serial2.write(ctrlbytes, sizeof(ctrlbytes));
        delay(50);
    }
}

bool HDV70E1::controlByPanel(int pwrPin,int machinePwr, int panelPIN){
    if(digitalRead(machinePwr)== 0){ // Check machine ON. If not ON , turn ON it.
        digitalWrite(pwrPin,HIGH);
        delay(100);
        digitalWrite(pwrPin,LOW);
    }
    delay(100);
    if(digitalRead(machinePwr)== 1){
        digitalWrite(panelPIN,HIGH); // Enabble panel control
        return true;
    }else{
        return false;
    }
}


void HDV70E1::ctrlRotary(rotaryMODE prog){
  //Default ctrlbytes  0x55,0xaa,0x00,0x00,0x00,0x50,0x50
  Serial.println();
  Serial.println("Selecting Rotary");
  int valuein = 0;

  for(int i = 0 ; i <= (prog*5) ; i++){
    if(i == 5){
      if(valuein != prog){
         ctrlbytes[4] += 0x01;
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
    delay(50);
  }
}


void HDV70E1::ctrlRotary(rotaryMODE prog,int dtime){
  Serial.printf("Rotate CCW for %d times\n",prog);
  //Send ready command 
  ctrlbytes[2] = 0;
  ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
  Serial2.write(ctrlbytes, sizeof(ctrlbytes));

  //Send Rotation command
  for(int i=0;i<prog;i++){
      ctrlbytes[4] += 1;
      ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
      Serial2.write(ctrlbytes, sizeof(ctrlbytes));
      delay(dtime);
  }
}


void HDV70E1::ctrlButton(buttonMODE btn,int nturn,int dtime){
    Serial.printf("Press button (1=Start, 2=Option): %d for %d times\n",btn,nturn);
    for(int i=0;i<nturn;i++){
        ctrlbytes[2] = 0x00;
        ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
        Serial2.write(ctrlbytes, sizeof(ctrlbytes));
        delay(50);

        ctrlbytes[2] = btn;
        ctrlbytes[6] = 0x50 + ctrlbytes[4] + ctrlbytes[2];
        Serial2.write(ctrlbytes, sizeof(ctrlbytes));
        delay(50);
    }
}


void HDV70E1::ctrlButton(buttonMODE btn){
    //byte ctrlmsg;
    
    if(btn == 1){
        Serial.printf("Pressing button: Start/Pause\n");
    }else if(btn == 2){
        Serial.printf("Pressing button: Option\n");
    }

    for(int i = 0 ; i <40 ; i++){
      if(i > 0 && i < 20){
        ctrlbytes[2] = (byte)btn;
      }
      if(i > 20 && i < 40){
        ctrlbytes[2] = (byte)0 ;  
      }

      ctrlbytes[6] = 0x50 +  ctrlbytes[4] +  ctrlbytes[2];
      Serial.printf("Inx: %d\n",i);  
      Serial.print("Byte 2: ");Serial.println(byte(ctrlbytes[2]),HEX); 
      Serial.print("Byte 4: ");Serial.println(byte(ctrlbytes[4]),HEX); 
      Serial.print("Byte 6: ");Serial.println(byte(ctrlbytes[6]),HEX);
      Serial.println();
      Serial2.write( ctrlbytes, sizeof( ctrlbytes));

      delay(50);
    }
    // ctrlbytes[2] += 0x00;
    // ctrlbytes[6] += 0x50;
}




int HDV70E1::runProgram(int pwrPin,int machinePwr,int doorState,rotaryMODE prog,digitdisplay &disp,int &err){
    //int maxtry = 10;
    int retry = 0;

    while(retry <3){   
        if(!powerCtrl(pwrPin,machinePwr,TURNON)){
            Serial.printf("[runProgram]-> Power On machine, But machine not ON. Retry:%d\n",retry+1);
            retry++;
        }else{
            Serial.printf("[runProgram]->Machine is On.\n");
            while( (!isDoorClose(doorState))){
                Serial.printf("[runProgram]->Door is open. Please close Door\n");
                disp.scrollingText("-doo-",5);
                disp.print("do");
                sleep(15); // wait 30secs.
            }
            delay(300);
            ctrlRotary(prog);
            //ctrlButton(TEMP);
            ctrlButton(OPTION); 
            delay(300);
            ctrlButton(START);
            return 1;
        }
    }

    if(retry >= 3){
        disp.scrollingText("-PoE-",5);
        disp.print("PE");
        delay(3000);
        return 0;
    }

    
   /* 
    if(powerCtrl(pwrPin,machinePwr,TURNON)){ // Turn on machine
        delay(500);
        while( (!isDoorClose(doorState))){
            Serial.printf("[runProgram]->Door is open. Please close Door\n");
            disp.scrollingText("-dO-",5);
            sleep(5); // wait 30secs.
        }
        ctrlRotary(prog);
        //ctrlButton(TEMP);
        ctrlButton(OPTION); 
        delay(300);
        ctrlButton(START);
        return 1;
    }else{
        Serial.printf("[runProgram]-> Power On machine, But machine not ON\n");
        disp.scrollingText("-PE-",5);
        delay(5000);
        return 0;
    }
    */

}

bool HDV70E1::powerCtrl(int pwrPin, int machinePwr,powerMODE mode){
    
    if(mode == 1){ // Pwoer ON Machine
        Serial.printf("[powerCtrl]->Turn on machine\n");
        if(!isMachineON(machinePwr)){
            singlePulse(pwrPin,400,HIGH);   
        }
    }else{ // POWER OFF Machine
        Serial.printf("[powerCtrl]->Turn off machine\n");
        if(isMachineON(machinePwr)){
            singlePulse(pwrPin,400,HIGH);
        }
    }
    delay(500);
    //return isMachineON(machinePwr); 
    return digitalRead(machinePwr);
}

void HDV70E1::singlePulse(int pin, int period, bool startlogic){
    digitalWrite(pin,startlogic);
    delay(period);
    digitalWrite(pin,!startlogic);
    delay(period);
}


bool HDV70E1::isMachineON(int pin){
    int err;
    return isMachineON(pin,err);
}

bool HDV70E1::isMachineON(int pin,int &err){
    if(digitalRead(pin)){
        Serial.printf("[isMachineOn]->Machine ON\n");
        return true;
    }else{
        Serial.printf("[isMachineOn]->Machine OFF\n");
        return false;
    }
}

bool HDV70E1::isDoorClose(int pin){
    if(digitalRead(pin)){
        Serial.printf("[isDoorClose]->Door CLOSE\n");
        return true; // Door Close
    }else{
        Serial.printf("[isDoorClose]->Door OPEN\n");
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