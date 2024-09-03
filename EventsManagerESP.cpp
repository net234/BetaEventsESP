#include "evHelpers.h"


/*************************************************
     Sketch betaEvents.ino   validation of lib betaEvents to deal nicely with events programing with Arduino
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

  History
    V1.0 (21/11/2020)
    - Full rebuild from PH_Event V1.3.1 (15/03/2020)
    V1.1 (30/11/2020)
    - Ajout du percentCPU pour une meilleur visualisation de l'usage CPU
    V1.2 02/01/2021
    - Ajout d'une globale EventManagerPtr pour l'acces par d'autre lib et respecter l'implantation C++
    - Amelioration du iddle mode pour l'ESP8266 (WiFi sleep mode)
    V1.3 13/01/2021
    - correction pour mieux gerer les pulses dans le cas 0 ou 100 percent
   V1.4   6/3/2021
    - Inclusion TimeLib.h
    - Gestion des event en liste chainée
    V2.0  20/04/2021
    - Mise en liste chainée de modules 'events'
      evHandlerSerial   Gestion des caracteres et des chaines provenant de Serial
      evHandlerLed      Gestion d'une led avec ou sans clignotement sur un GPIO (Multiple instance possible)
      evHandlerButton   Gestion d'un pousoir sur un GPIO (Multiple instance possible)
      evHandlerDebug    Affichage de l'occupation CPU, de la memoire libre et des evenements 100Hz 10Hz et 1Hz
    V2.2  27/10/2021
       more arduino like lib with self built in instance

    V2.3    09/03/2022   isolation of evHandler for compatibility with dual core ESP32

 *************************************************/
//#define BETAEVENTS_CCP


#include "EventsManagerESP.h"

void EvManager::begin() {
  DT_println("EvManager::begin");
  // parse event list
  handlerList.parseBegin();
  push(evInit);
}

static uint32_t milliSeconds = 0;
static uint16_t delta1Hz = 0;
static uint16_t delta10Hz = 0;
static uint16_t delta100Hz = 0;


//return true il anEvent is here
bool EvManager::get(const bool sleepOk) {  //  sleep = true;
  bool eventWasNill = (code == evNill);    // previous event was a evNill
  _loopCounter++;

  // cumul du temps passé
  uint16_t delta = millis() - milliSeconds;  // max 64 secondes
  if (delta) {
    milliSeconds += delta;
    eventMilliList.parseDelay(delta);
    delta100Hz += delta;
    delta10Hz += delta;
    delta1Hz += delta;
  }

  // recuperation des events passés   hors sleep time
  if (nextEvent()) return (true);
  // no event in list

  // si SleepOk et que l'evenement precedent etait un nillEvent on frezze le CPU
  // mooving in sleep mode
  if (sleepOk && eventWasNill) {

    // pour l'ESP8266 pas de sleep simple
    // !! TODO :  faire un meilleur sleep ESP32 & ESP8266
    //while (milliSeconds == millis()) yield();
    delay(1);                         // to allow wifi sleep in modem mode
    delta = millis() - milliSeconds;  // should be 1 max
    if (delta) {
      _idleMillisec += delta;
      milliSeconds += delta;
      eventMilliList.parseDelay(delta);
      delta100Hz += delta;
      delta10Hz += delta;
      delta1Hz += delta;
    }
    // recuperation des events passés
    if (nextEvent()) return (true);  //during sleep mode
  }

  _evNillCounter++;
  return (code = evNill);
}


///////////////////////////////////////////////////////////
// get next event
bool EvManager::nextEvent() {

  // les ev100Hz ne sont pas tous restitués mais le nombre est dans iParam de l'event

  if (delta100Hz >= 10) {
    iParam = (delta100Hz / 10);  // nombre d'ev100Hz d'un coup
    delta100Hz -= (iParam)*10;
    return (code = ev100Hz);
  }

  // les ev10Hz ne sont pas tous restitués mais le nombre est dans iParam de l'event
  if (delta10Hz >= 100) {
    iParam = (delta10Hz / 100);  // nombre d'ev10Hz d'un coup
    delta10Hz -= (iParam)*100;
    return (code = ev10Hz);
  }

  // par contre les ev1Hz sont tous restirués meme avec du retard
  if (delta1Hz >= 1000) {
    //    __cnt1Hz--;
    delta1Hz -= 1000;
    return (code = ev1Hz);
  }

  // pase all the get methode of evry event handler
  // return the first event
  if (handlerList.parseGet()) return true;
  // still no evant
  // grab the oldest event in eventList (if any)
  if (eventList.head) {
    *static_cast<event*>(this) = *eventList.head;  // copie par l'operateur par defaut
    eventList.remove(eventList.head);
    return true;
  }
  code = evNill;
  ext = 0;
  iParam = 0;
  return (false);
}



void EvManager::handle() {
  // parse event list
  handlerList.parseHandle();

  switch (code) {
      // gestion des evenement avec delay au 100' de seconde
      // todo  gerer des event repetitifs

    case ev100Hz: eventCentsList.parseDelay(iParam); break;
    case ev10Hz: eventTenthList.parseDelay(iParam); break;
    case ev1Hz:
      {

        _percentCPU = 100 - (100UL * _idleMillisec / 1000);

        // Ev24h only when day change but not just after a boot
        static uint8_t oldDay = 0;
        uint16_t aDay = day();
        if (oldDay != aDay) {
          if (oldDay > 0) push(ev24H, aDay);  // User may take care of days
          oldDay = aDay;
        }

        eventSecondsList.parseDelay(1);
        //        Serial.print("iddle="); Serial.println(_idleMillisec);
        //        Serial.print("CPU% ="); Serial.println(_percentCPU);
        //        Serial.print("_evNillCounter="); Serial.println(_evNillCounter);
        //        Serial.print("_loopCounter="); Serial.println(_loopCounter);
        //        //Serial.print("elaps="); Serial.println(elaps);
        _idleMillisec = 0;
        _evNillParsec = _evNillCounter;
        _evNillCounter = 0;
        _loopParsec = _loopCounter;
        _loopCounter = 0;
      }
      break;
  }
}



bool EvManager::push(event& aEvent) {
  eventList.add(aEvent);
  return (true);
}
/*
bool EventManager::push(const uint8_t codeP, const int16_t paramP) {
  eventItem_t aEvent(codeP, paramP);
  return (push(aEvent));
}
*/
bool EvManager::push(const uint8_t code, const int8_t ext, const int iParam) {
  event aEvent(code, ext, iParam);
  return (push(aEvent));
}


bool EvManager::forceDelayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext, const int iParam, bool repeat) {
  // delay millis   1min  100Hz 10 min  10Hz  1H30  1Hz  17H
  if (delayMillisec == 0) {
    return (push(code, ext, iParam));
  }

  if (delayMillisec < 1000UL) {  // moins de 1 secondes
    eventDelay aEvent = eventDelay(code, ext, iParam, delayMillisec, repeat);
    eventMilliList.add(aEvent);
    return (true);
  }

  if (delayMillisec < 1000UL * 60) {  // moins de 1 Minute
    eventDelay aEvent = eventDelay(code, ext, iParam, delayMillisec / 10, repeat);
    eventCentsList.add(aEvent);
    return (true);
  }


  if (delayMillisec < 1000UL * 3600) {  // moins de 1 heure
    eventDelay aEvent = eventDelay(code, ext, iParam, delayMillisec / 100, repeat);
    eventTenthList.add(aEvent);
    return (true);
  }
  //long event (more than 100 years :)
  eventLongDelay aEvent = eventLongDelay(code, ext, iParam, delayMillisec / 1000, repeat);
  eventSecondsList.add(aEvent);


  return (true);
}

bool EvManager::forceDelayedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext, const int iParam, bool repeat) {
  if (delaySeconds == 0) {
    return (push(code, ext, iParam));
  }

  if (delaySeconds < 60UL) {  // moins de 1 Minute
    eventDelay aEvent = eventDelay(code, ext, iParam, delaySeconds * 100, repeat);
    eventCentsList.add(aEvent);
    return (true);
  }


  if (delaySeconds < 3600UL) {  // moins de 1 heure
    eventDelay aEvent = eventDelay(code, ext, iParam, delaySeconds * 10,repeat);
    eventTenthList.add(aEvent);
    return (true);
  }
  //long event (more than 100 years :)
  eventLongDelay aEvent = eventLongDelay(code, ext, iParam, delaySeconds,repeat);
  eventSecondsList.add(aEvent);


  return (true);
}



// remove all pending events with same code then add the deleayed event in milliseconds (max 49 days)
bool EvManager::delayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext, const int iParam) {
  while (removeDelayEvent(code)) {};
  return (forceDelayedPushMillis(delayMillisec, code, ext, iParam));
}
// remove all pending events with same code then add the deleayed event in milliseconds (max over 100 years)
bool EvManager::delayedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext, const int iParam) {
  while (removeDelayEvent(code)) {};
  return (forceDelayedPushSeconds(delaySeconds, code, ext, iParam));
}


/*
bool EventManager::delayedPushMillis(const uint32_t delayMillisec, const uint8_t code) {
  while (removeDelayEvent(code)) {};
  return (forceDelayedPushMillis(delayMillisec, code));
}
*/

// le repeated ne peut pas etre inferieur a 10Ms
// il efface tout les pendinfg event du meme code
// les 2 parametres sont forcement a zero
bool EvManager::repeatedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext, const int iParam) {
  while (removeDelayEvent(code)) {};
  if (delayMillisec > 0 and delayMillisec < 10) return (false);
  return (forceDelayedPushMillis(delayMillisec, code, ext, iParam, true));
}
bool EvManager::repeatedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext, const int iParam) {
  while (removeDelayEvent(code)) {};
  if (delaySeconds == 0 ) return (false);
  return (forceDelayedPushSeconds(delaySeconds, code, ext, iParam, true));
}

bool EvManager::removeDelayEvent(const byte codeevent) {
  return (eventMilliList.remove(codeevent) || eventCentsList.remove(codeevent) || eventTenthList.remove(codeevent) || eventSecondsList.remove(codeevent));
}



//only instance of EvManager
EvManager Events = EvManager();
