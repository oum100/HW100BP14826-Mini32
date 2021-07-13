/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*  * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Code by Simon Monk
 http://www.simonmonk.org
* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// For Arduino 1.0 and earlier
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "Timer.h"

unsigned long startPause=0;
unsigned long endPause=0;
int xloop=0;

Timer::Timer(void)
{
}

int8_t Timer::every(unsigned long period, void (*callback)(), int repeatCount)
{
	int8_t i = findFreeEventIndex();
	if (i == -1) return -1;

	_events[i].eventType = EVENT_EVERY;
	_events[i].period = period;
	_events[i].repeatCount = repeatCount;
	_events[i].callback = callback;
	_events[i].lastEventTime = millis();
	_events[i].pauseTime = 0;
	_events[i].operateTime = 0;
  _events[i].count_sec=0;
  _events[i].count_min=0;
  _events[i].count_hour=0;
	_events[i].count = 0;

	Serial.printf("[TIMER]->Period Found: %li msecs\n",_events[i].period);
 Serial.printf("[TIMER]->Period Set: %li Secs\n",_events[i].period/1000);
  Serial.printf("[TIMER]->eventType Set: %d\n\n",_events[i].eventType);
	return i;
}

int8_t Timer::every(unsigned long period, void (*callback)())
{
	return every(period, callback, -1); // - means forever
}

int8_t Timer::after(unsigned long period, void (*callback)())
{
	return every(period, callback, 1);
}

int8_t Timer::oscillate(uint8_t pin, unsigned long period, uint8_t startingValue, int repeatCount)
{
	int8_t i = findFreeEventIndex();
	if (i == NO_TIMER_AVAILABLE) return NO_TIMER_AVAILABLE;

	_events[i].eventType = EVENT_OSCILLATE;
	_events[i].pin = pin;
	_events[i].period = period;
	_events[i].pinState = startingValue;
	digitalWrite(pin, startingValue);
	_events[i].repeatCount = repeatCount * 2; // full cycles not transitions
	_events[i].lastEventTime = millis();
	_events[i].pauseTime = 0;
	_events[i].operateTime = 0;	
  _events[i].count_sec=0;
  _events[i].count_min=0;
  _events[i].count_hour=0;  
	_events[i].count = 0;
	return i;
}

int8_t Timer::oscillate(uint8_t pin, unsigned long period, uint8_t startingValue)
{
	return oscillate(pin, period, startingValue, -1); // forever
}

/**
 * This method will generate a pulse of !startingValue, occuring period after the
 * call of this method and lasting for period. The Pin will be left in !startingValue.
 */
int8_t Timer::pulse(uint8_t pin, unsigned long period, uint8_t startingValue)
{
	return oscillate(pin, period, startingValue, 1); // once
}

/**
 * This method will generate a pulse of startingValue, starting immediately and of
 * length period. The pin will be left in the !startingValue state
 */
int8_t Timer::pulseImmediate(uint8_t pin, unsigned long period, uint8_t pulseValue)
{
	int8_t id(oscillate(pin, period, pulseValue, 1));
	// now fix the repeat count
	if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {
		_events[id].repeatCount = 1;
	}
	return id;
}


void Timer::stop(int8_t id)
{
	if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {
		_events[id].eventType = EVENT_NONE;
	}
}

void Timer::update(void)
{
	unsigned long now = millis();
	update(now);
}

void Timer::update(unsigned long now)
{
  int xHour=0,xMin=0,xSec=0;
  
	for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
	{
		if ((_events[i].eventType != EVENT_NONE) && (_events[i].eventType != EVENT_PAUSE))
		{
			_events[i].update(now);

      getOperTime(i, &xHour, &xMin, &xSec);
      if(_events[i].operateTime % 1000 == 0){
          if( xloop == 0){
            //Serial.printf("[Timer][%d]->HH:MM:SS = %02d:%02d:%02d \n",i,xHour,xMin,xSec);
          }else{
            if( xloop >= 25){                
              //Serial.printf("[Timer][%d]->HH:MM:SS = %02d:%02d:%02d \n",i,xHour,xMin,xSec);
              xloop=1;
            } 
         }
      }   
      xloop++; 
		}   	
	}
}

void Timer::pause(int8_t id)
{
	if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {
		_events[id].eventType = EVENT_PAUSE;
    startPause = millis();
		//_events[id].pauseTime = millis();
		Serial.printf("[Timer]->Start pause: %li\n",startPause);
	}

}

void Timer::resume(int8_t id)
{
  endPause = millis();
  
	if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {
		_events[id].eventType = EVENT_EVERY;

    	Serial.printf("[Timer]->This PauseTime: %li-%li=%li\n",startPause,endPause,endPause-startPause);
    
		_events[id].pauseTime += endPause - startPause;
		Serial.printf("[Timer]->Total PauseTime: %li\n",_events[id].pauseTime);

    //Set new period
		_events[id].period += endPause - startPause;
		Serial.printf("[Timer]->Set New Period: %li\n\n",_events[id].period);

    startPause = 0;
    endPause = 0;
	}
	
}


unsigned long Timer::getOperTime(int8_t id)
{
  if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {
     return _events[id].operateTime;
  }
}

unsigned long Timer::getOperTime(int8_t id, int *Hour, int *Min, int *Sec)
{
  if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {

    //Serial.printf("[getOperTime]->OperateTime[%d]: %u\n",id,_events[id].operateTime);

    _events[id].count_sec = _events[id].operateTime / 1000;
    _events[id].count_min = _events[id].count_sec / 60;
    _events[id].count_hour = _events[id].count_min / 60;

    if(_events[id].count_min > 0){  
      _events[id].count_sec %= 60;
  
      if(_events[id].count_hour > 0){
        _events[id].count_min %= 60;
      }
    }
    
    *Hour = _events[id].count_hour;
    *Min = _events[id].count_min; 
    *Sec = _events[id].count_sec;

    //Serial.printf("[Timer]->HH:MM:SS:[%d]= %02d:%02d:%02d \n",id,xHour,xMin,xSec);
     
    return _events[id].operateTime;
  }
  
}

int8_t Timer::getCounter(int8_t id)
{
  if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {    
     return _events[id].count;
  }   
}

int8_t Timer::getEventType(int8_t id)
{
  if (id >= 0 && id < MAX_NUMBER_OF_EVENTS) {    
     return _events[id].eventType;
  }   
}

int8_t Timer::findFreeEventIndex(void)
{
	for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
	{
    // Serial.printf("\n[TIMER FindFree]-> ID: %d\n",i);
    // Serial.printf("[TIMER FindFree]-> evenType: %d\n",_events[i].eventType);
		if (_events[i].eventType == EVENT_NONE)
		{
			return i;
		}
	}
	return NO_TIMER_AVAILABLE;
}
