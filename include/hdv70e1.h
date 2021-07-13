

#ifndef hdv70e1_h
#define hdv70e1_h

#include<Arduino.h>
    #define POWER_RLY    26
    #define BOOK_LED    18
    #define DSTATE      19
    #define COINDOOR    23
    #define ENPANEL     5

    #define MODESW      39
    #define COININ      35
    #define ENCOIN      33
    #define MACHINEDC   34
    #define UNLOCK      32

    #define RXDU1       27 // DIO
    #define TXDU1       25 // CLK

    #define DIO         25
    #define CLK         27

    #define BUZZ        4
    #define WIFI_LED    2

    #define INTERRUPT_SET ( (1ULL<<MODESW) |(1ULL<<COININ) |(1ULL<<COINDOOR) )
    #define INPUT_SET ( (1ULL<<DSTATE) |(1ULL<<MACHINEDC) )
    #define OUTPUT_SET ( (1ULL<<POWER_RLY) |(1ULL<<ENPANEL) | (1ULL<<ENCOIN) |(1ULL<<UNLOCK) |(1ULL<<BUZZ) |(1ULL<<BOOK_LED) |(1ULL<<WIFI_LED))

#endif

      

/*
    Step to control HDV70E1

    Wait for coin   10Baht = 15 mins,  Max 

    1. Power On Machine
    2. Set program 
    3. Check Door close.  Cannot start if door not close
    4. Press start
        4.1 Tracking timer remain to NV-RAM
        4.2 if pause or door open. pause timer too. and continue when door close again.
        4.3 if power failure. After power come back.  If timer remain and door close then resume machine.
    5. When timeout
    
*/
// byte ctrlBYTES[] = {0X55,0xAA,0x00,0x00,0x00,0x50,0x50}; 

class HDV70E1 {
    public:
        byte ctrlbytes[7] = {0X55,0xAA,0x00,0x00,0x00,0x50,0x50};
        enum rotaryMODE {NORMAL,EXTRA,MIN30,MIN60,MIN90,MIN120,MIN150,MIN180,IRON};
        enum buttonMODE {NOFUNC,START,OPTION};

        HDV70E1(void);
        void ctrlRotary(rotaryMODE prog);
        void ctrlButton(buttonMODE btn);
        void runProgram(int pwrPin,int machinePwr,int doorState,rotaryMODE prog);
        void powerCtrl(int pwrPin, int machinePwr,bool mode); // ON = 1, 0 = OFF
        void singlePulse(int pin, int period, bool startlogic);
        bool isDoorClose(int pin);
        bool isMachineON(int pin);
};

void boardTest(void);



