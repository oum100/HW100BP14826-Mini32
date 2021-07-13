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

#include "Event.h"

int count_x = 0;

Event::Event(void)
{
	eventType = EVENT_NONE;
}

void Event::update(void)
{
    unsigned long now = millis();
    update(now);
}

void Event::update(unsigned long now)
{
  //Record operateTime
  operateTime = now - lastEventTime;
  
	if (operateTime >= period )		
	{
		switch (eventType)
		{
			case EVENT_EVERY:
				(*callback)();
				break;

			case EVENT_OSCILLATE:
				pinState = ! pinState;
				digitalWrite(pin, pinState);
				break;
		}
   
		lastEventTime = now;
		count++;
	}

  //Check repeat
	if (repeatCount > -1 && count >= repeatCount)
	{
		eventType = EVENT_NONE;
    Serial.printf("\n[EVENT]->Total Service time: %li\n",now - lastEventTime) ;
    Serial.printf("[EVENT]->Total Pause Time: %li or %li Secs \n",pauseTime, pauseTime/1000);
    Serial.printf("[EVENT]->Total Time (hh:mm:ss) = %02d:%02d:%02d\n\n",count_hour,count_min,count_sec);
	}
}
