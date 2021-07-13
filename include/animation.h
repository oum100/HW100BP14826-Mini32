#ifndef animation_h
#define animation_h

#include <Arduino.h>
#include <SevenSegmentTM1637.h>
#include <SevenSegmentAsciiMap.h>
#include "SevenSegmentFun.h"



class digitdisplay : public SevenSegmentFun{ 
    public:
        // digitdisplay(void);
        digitdisplay(uint8_t pinClk, uint8_t pinDIO);
        void animation1(SevenSegmentTM1637 &disp,int dtime,int repeat);
        void animation2(SevenSegmentTM1637 &disp,int dtime,int repeat);
        void animation3(SevenSegmentTM1637 &disp,int dtime,int repeat);
        void animation4(SevenSegmentTM1637 &disp,int dtime,int repeat);
};



#endif