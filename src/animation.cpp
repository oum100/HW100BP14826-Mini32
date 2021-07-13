#include "animation.h"
#include <Arduino.h>
#include <SevenSegmentTM1637.h>
#include <SevenSegmentAsciiMap.h>
#include "SevenSegmentFun.h"



byte anima1[]={8,64,1};

//Display pattern
uint8_t RUNBOX1[] = { 0x00, 0b01100011, 0b01011100, 0x00 };
uint8_t RUNBOX2[] = { 0x00, 0b01011100, 0b01100011, 0x00 };

// digitdisplay::digitdisplay(void){

// }
digitdisplay::digitdisplay(uint8_t pinClk, uint8_t pinDIO):SevenSegmentFun(pinClk, pinDIO){};

void digitdisplay :: animation1(SevenSegmentTM1637 &disp,int dtime,int repeat){
    uint8_t buffer[4]={B01011100,B01100011,0,0};
    for(int i=0;i<repeat;i++){
        if(buffer[0]==B01100011){
            buffer[0]=B01011100;
            buffer[1]=B01100011;  
        }else{
            buffer[0]=B01100011;
            buffer[1]=B01011100;     
        }
        disp.printRaw(buffer);
        delay(dtime);
    }
}


void digitdisplay :: animation2(SevenSegmentTM1637 &disp,int dtime,int repeat){
   uint8_t buffer[4][4] = {{1,1,0,0},{0,6,0,0},{8,8,0,0},{48,0,0,0}};
   //16: B00010000
   //2:B00000010
   //4: B00000100
   //32: B00100000

  for(int i=0;i<repeat;i++){
    disp.printRaw(buffer[0]);
    delay(dtime);

    disp.printRaw(buffer[1]);
    delay(dtime);

    disp.printRaw(buffer[2]);
    delay(dtime);

    disp.printRaw(buffer[3]);
    delay(dtime);
  }  
}



void digitdisplay :: animation3(SevenSegmentTM1637 &disp,int dtime,int repeat){
   uint8_t buffer[4][4] = {{16,2,0,0},{4,32,0,0},{2,16,0,0},{32,4,0,0}};
   //16: B00010000
   //2:B00000010
   //4: B00000100
   //32: B00100000

  for(int i=0;i<repeat;i++){
    disp.printRaw(buffer[0]);
    delay(dtime);

    disp.printRaw(buffer[1]);
    delay(dtime);

    disp.printRaw(buffer[2]);
    delay(dtime);

    disp.printRaw(buffer[3]);
    delay(dtime);
  }  
}


void digitdisplay :: animation4(SevenSegmentTM1637 &disp,int dtime,int repeat){
   uint8_t buffer[4][4] = {{1,1,0,0},{6,6,0,0},{8,8,0,0},{48,48,0,0}};
   //16: B00010000
   //2:B00000010
   //4: B00000100
   //32: B00100000

  for(int i=0;i<repeat;i++){
    disp.printRaw(buffer[0]);
    delay(dtime);

    disp.printRaw(buffer[1]);
    delay(dtime);

    disp.printRaw(buffer[2]);
    delay(dtime);

    disp.printRaw(buffer[3]);
    delay(dtime);
  }  
}
