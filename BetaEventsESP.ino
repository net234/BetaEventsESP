/*************************************************

  ***********************************************
    Sketch betaEventsESP.ino   validation of lib betaEvents  to deal nicely with events programing with Arduino
    Copyright 2020 Pierre HENRY net23@frdev.com All - right reserved.


  This file is part of betaEvents.

    betaEvents is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    betaEvents is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.

  betaEventsESP is a an exemple of stand alone code to understand Betaevents concept
  this version is specifty crafted for 32 bit processor as ESP8266

  actual Sources : https://github.com/net234/BetaEventsESP

  it use lib TimeLib.h  and no specific Hardware except ESP8266

  BetaEvents Object :

  event         structure minimaliste pour gerer un evenement
  evManager     class du noyau evenementiel permetant de gerer les evenements  une seule instance nomée "Events"
  evHandler     class permetant de gerer un interface specifique (pousoir led relai ...) de maniere evenementiel


  Internal Objets
  evHandlerList class permetant de gerer une liste d'evenement


  fichier :
  BetaEventESP.h           modele d'utilisation basic des events (a utiliser pour debuter)
  EventsManagerESP.h/cpp   code du noyau
  evHandlerESP.h/cpp       code de divers handler de base (input output poussoir led relais clavier debug)


History :
Orinal build PH_Event V1.0 P.HENRY  24/02/2020  https://github.com/net234/PH_Events
Various version for Arduino 8Bits  under BetaEvents   https://github.com/net234/BetaEvents
Various version for Arduino 32Bits  under BetaEvents32   https://github.com/net234/BetaEvents32
Full rebuild    for ESP8266    https://github.com/net234/BetaEventsESP   (betaEvensESP V4.0) 25/04/2024

07/06/24   V4.0C add de multi in evHandlerButton 
01/09/24   V4.1  add handler pour serial2

*/
// Name of this application
#ifndef APP_NAME
// version de prod
#define APP_NAME "betaEventsESP V4.1"
#endif



// Eventslist
/* Evenements du Manager (voir evHandlers.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use delayedPushMillis(delay,event)
  ev100Hz,         // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,          // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,           // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,           // 24H when timestamp pass over 24H
  evInit,
  evInChar,
  evInString,
*/


// List of Event specific for this APP


// Utilisation d'enum class pour une meilleure encapsulation et un typage fort
enum tevUser : uint8_t {
  evBP0 = 100,
  evLed0,
  evD1,
  evR1,

};
//}



#include "BetaEventsESP.h"  // this will create an evManager called Events with a push buton BP0 and a Led LED0


void setup() {
  //Serial.begin(115200);
  //Serial.println(F("\r\n\n" APP_NAME));
  // delay(5000);
  //Serial.println(F("\r\n\n PRE INIT"));
  Events.begin();
  //Serial.println(F(" PRE INIT END"));
  Serial.println(F("\r\n\n" APP_NAME));

  T_println("Bonjour...");
}

bool sleep = true;

void loop() {
  Events.get(sleep);  //get the next event
  //if (Events.code >= 100) { DV_println(Events.code); };
  Events.handle();        //pass the event to every evHandlers
  switch (Events.code) {  //deal whith th event
    case evInit:          //evInit append once affter every evHandles haves done with 'begin'
      {
        Serial.println("Init");  //just infor the user that an evInit append
      }
      break;

    case evD1:
      T_println("D1 done");
      break;

    case evR1:
      Serial.print('R');
      break;




    case ev1Hz:
      {
        //DV_print(second());
        //DV_println(helperFreeRam());
      }
      break;

      /*
    case evLed0:  //
      //DV_println(Events.ext);
      switch (Events.ext) {
        case evxOn:
          Serial.println("O");
          break;
        case evxOff:
          Serial.println(".");
          break;
      }
      break;
*/

    case evBP0:  //
      //DV_println(Events.ext);
      switch (Events.ext) {
        case evxOn:
          TV_println("BP0 ON, multi", BP0.multi)
          Led0.setMillisec(500);
          break;
        case evxLongOn:
          TV_println("BP0 LONG ON, multi", BP0.multi);
          break;
        case evxOff:
          TV_println("BP0 OFF, multi",BP0.multi);
          Led0.setMillisec(5000);
          break;
        case evxLongOff:
          T_println("BP0 LONG OFF");
          break;
      }
      break;

    case evInChar:
      TV_println("incharext", Events.ext);
      //DV_println(Events.cParam);
      switch (Events.cParam) {
        case '1': delay(10); break;
        case '2': delay(100); break;
        case '3': delay(1000); break;
        case '4': delay(10000); break;
      }
      break;



    case evInString:
      if (Events.strPtrParam->equals("S")) {
        sleep = not sleep;
        V_println(sleep);
      }
      if (Events.strPtrParam->equals("D1")) {
        Events.delayedPushMillis(500, evD1);
        T_println("D1 start 500ms");
      }
      if (Events.strPtrParam->equals("D2")) {
        Events.delayedPushMillis(2000, evD1);
        T_println("D1 start 2s");
      }
      if (Events.strPtrParam->equals("D10")) {
        Events.delayedPushSeconds(10, evD1);
        T_println("D1 start 10s");
      }
      if (Events.strPtrParam->equals("R1")) {
        Events.repeatedPushMillis(500, evR1);
        T_println("R1 start every 500ms");
      }
      if (Events.strPtrParam->equals("R2")) {
        Events.repeatedPushSeconds(2, evR1);
        T_println("R1 start every 2 second");
      }
      if (Events.strPtrParam->equals("LEDON")) {
        Led0.setOn(true);
        T_println("LED ON");
      }

      if (Events.strPtrParam->equals("LEDOFF")) {
        Led0.setOn(false);
        T_println("LED OFF");
      }
      break;
  }
}
