//#include <sys/_stdint.h>
//#include <algorithm>
//#include <sys/_stdint.h>
/*************************************************
    EventsManager.h   validation of lib betaEvents to deal nicely with events programing with Arduino
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
    - ajout setLedOn(true/false)
    V1.3.1 23/01/2021
  	- correction setLedOn pour un resultat immediat
    V1.4   6/3/2021
    - Inclusion TimeLib.h
    - Gestion des event en liste chainée
     V2.0  20/04/2021
    - Mise en liste chainée de modules 'events'
      evHandlerSerial   Gestion des caracteres et des chaines provenant de Serial
      evHandlerLed      Gestion d'une led avec ou sans clignotement sur un GPIO (Multiple instance possible)
      evHandlerButton   Gestion d'un pousoir sur un GPIO (Multiple instance possible)
      evHandlerDebug    Affichage de l'occupation CPU, de la memoire libre et des evenements 100Hz 10Hz et 1Hz

     V2.1  05/06/2021
       split EventManger and BetaEvents
     V2.2  27/10/2021
       more arduino like lib with self built in instance
       TODO : faire une liste de get event separés

    V2.3    09/03/2022   isolation of evHandler for compatibility with dual core ESP32
     V3.0
 *************************************************/
#pragma once
#include <Arduino.h>


// betaEvent handle a minimal time system to get for seconds() minutes() or hours()

//#include <TimeLib.h>  // needed with ESP
#include "evHelpers.h"
#include "evHandlers.h"






// base pour un eventHandler (gestionaire avec un handleEvent);
// get retourne dans l'ordre :
// 1 les delayed-event de precision millisecondes  (inferieurs a 1 seconde)
// 2 les events 100Hz  en non cumulatif
// 3 les events  10Hz  en non cumulatif   (les ev10Hz scan les delayed events de moins 10 minutes)
// 4 les event    1Hz  en cumulatif       (les ev1Hz scan les delayed events de plus de 10 minutes)
// 5 les events des handlers (appel du get de chaque handler)
// 6 les events presents dans la liste d'attente (push)
// 7 si l'event precedent n'etait pas un nilEvent un nilEvent est envoyé
// sinon un delay de 1 millisec effectué
/*
class eventHandler_t {
public:
  eventHandler_t* next;  // handle suivant
  eventHandler_t();
  virtual void begin(){};   // called with evManager::begin
  virtual void handle(){};  // calledwith evManager::handle
  virtual byte get() {
    return evNill;
  };
};
*/
//this is the only instance of evManager
//futur version will create Events0 and Events1 for ESP32 dual core
//#define evManager Events


// Base structure for event
struct event {
  event()
    : code(evNill), ext(0), iParam(0){};
  event(const uint8_t code, const uint8_t ext = 0, const int16_t iParam = 0)
    : code(code), ext(ext), iParam(iParam){};
  //event(const uint8_t code, const int16_t int1, const int16_t int2)
  //  : code(code), intExt2(int2), intExt(int1){};
  //stdEvent_t(const uint8_t code = evNill, const float aFloat = 0) : code(code), aFloat(aFloat) {};
  //  stdEvent_t(const uint8_t code = evNill, const uint8_t ext ) : code(code), ext(ext) {};
  //  stdEvent_t(const uint8_t code = evNill, const char aChar) : code(code), aChar(aChar) {};

  // stdEvent_t(const stdEvent_t& stdevent)
  // : code(stdevent.code), ext3(stdevent.ext3), intExt2(stdevent.intExt2), data(stdevent.data){};
  //: stdEvent_t(stdEvent);
  uint8_t code;  // code of the event
  uint8_t ext;   // only in Manager32
  union {
    char cParam;
    int iParam;
    float fParam;
    String* strPtrParam;
    size_t ptrParam;
  };
};

//base structure pour une Event avec Delay  (16bit)
// delay millis   1min  100Hz 10 min  10Hz  1H30  1Hz  18H
struct eventDelay : public event {
  eventDelay(const uint8_t code, const uint8_t ext, const uint32_t iParam1, const uint16_t aDelay, const bool repeat = false)
    : event(code, ext, iParam1), curentDelay(aDelay), repeatDelay(repeat ? aDelay : 0) {}

  uint16_t curentDelay;  // delay millis   1min  100Hz 10 min  10Hz  1H30  1Hz  17H
  uint16_t repeatDelay;
};

//base structure pour une Event avec Delay long  (32bit)
// delay  milis  49Jours  100Hz >1 ans  10Hz  >10ans  1Hz  >100ans
struct eventLongDelay : public event {

  eventLongDelay(const uint8_t code, const uint8_t ext, const uint32_t iParam1, const uint32_t aDelay, const bool repeat = false)
    : event(code, ext, iParam1), curentDelay(aDelay), repeatDelay(repeat ? aDelay : 0) {}


  uint32_t curentDelay;  // delay  millis  49Jours  100Hz >1 ans  10Hz  >10ans  1Hz  100ans
  uint32_t repeatDelay;
};






//   event list




// une liste d'event
template<typename T>
class evList {
private:
  //    element chainé d'event
  struct evItem : public T {
    evItem* next;  // Pointeur vers le prochain nœud
    evItem(const T& aEvent)
      : T(aEvent), next(nullptr){};
    //evItem(T* evHandler, evHandlerItem* next = nullptr) : evHandler(evHandler), next(next) {}
  };
  evItem* head;  // Pointeur vers le premier nœud de la liste
  evItem* tail;  // Pointeur vers le dernier nœud de la liste

public:
  evList()
    : head(nullptr), tail(nullptr) {}

  void add(const T& aEvent) {
    evItem* aItem = new evItem(aEvent);
    if (tail) {
      tail->next = aItem;
    } else {
      head = aItem;
    }
    tail = aItem;
  }




  bool remove(const evItem* toRemove) {
    if (!head) return (false);
    if (head == toRemove) {

      head = head->next;
      if (head == nullptr) tail = nullptr;
      delete toRemove;
      return (true);
    }
    for (evItem* prev = head; prev->next; prev = prev->next) {
      if (prev->next == toRemove) {
        prev->next = toRemove->next;
        if (tail == toRemove) tail = prev;
        delete toRemove;
        return (true);
      }
    }
    return (false);
  }

  bool remove(const uint8_t aCode) {
    if (!head) return (false);
    if (head->code == aCode) {
      evItem* toRemove = head;
      head = head->next;
      if (head == nullptr) tail = nullptr;
      delete toRemove;
      return (true);
    }
    for (evItem* prev = head; prev->next; prev = prev->next) {
      if (prev->next->code == aCode) {
        evItem* toRemove = prev->next;
        prev->next = toRemove->next;
        if (tail == toRemove) tail = prev;
        delete toRemove;
        return (true);
      }
    }
    return (false);
  }

  // push tout les events perimés de la liste sinon decremente leur duree de vie
  void parseDelay(const uint16_t delay);



  //~evList() {
  //clear();
  //}


  friend class EvManager;
};






//  event list END

class EvManager : public event {
public:

  EvManager()
    : event(evNill) {  // constructeur
  }
  void begin();
  bool get(const bool sleep = true);
  void handle();

  uint8_t getPercentCPU() {
    return _percentCPU;
  };

  bool push(event& aEvent);
  bool push(const uint8_t code, const int8_t ext = 0, const int iParam = 0);
  //   //   bool   pushFloat(const uint8_t code, const float   afloat);
  bool removeDelayEvent(const byte codeevent);
  bool delayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext = 0, const int iParam = 0);
  bool delayedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext = 0, const int iParam = 0);
  bool forceDelayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext = 0, const int iParam = 0, bool repeat = false);
  //  bool forceDelayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext = 0, const float fParam = 0, bool repeat = false);
  bool forceDelayedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext = 0, const int iParam = 0, bool repeat = false);
  bool repeatedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext = 0, const int iParam = 0);
  bool repeatedPushSeconds(const uint32_t delaySeconds, const uint8_t code, const uint8_t ext = 0, const int iParam = 0);
  //   bool repeatedPushMillis(const uint32_t delayMillisec, const uint8_t code);
  //   bool delayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const int16_t param1, const int16_t param2 = 0);
  //   bool repeatedPushMillis(const uint32_t delayMillisec, const uint8_t code, const int16_t param1, const int16_t param2, const bool repeat = false);
  //   //bool forceDelayedPushMillis(const uint32_t delayMillisec, const uint8_t code);
  //bool forceDelayedPushMillis(const uint32_t delayMillisec, const uint8_t code, const uint8_t ext = 0, const int16_t int16 = 0);
  //   //    int    syncroSeconde(const int millisec = 0);


  //   void addHandleEvent(eventHandler_t* eventHandlerPtr);
  //   //    void   addGetEvent(eventHandler_t* eventHandlerPtr);
private:
  bool nextEvent();  // Recherche du prochain event disponible
  byte _percentCPU = 0;
  unsigned long _loopCounter = 0;
  unsigned long _loopParsec = 0;
  unsigned long _evNillParsec = 0;
  unsigned long _evNillCounter = 0;
  uint16_t _idleMillisec = 0;  // CPU millisecondes en pause

  evHandlerList<evHandler> handlerList;
  evList<event> eventList;                  //event pending (no delay)
  evList<eventDelay> eventMilliList;        //pending Event with delay in millis sec  (less tha 1 sec)   abs max 62 sec
  evList<eventDelay> eventCentsList;        //pending Event with delay in cents of sec  (less than 1 minute)   (abs max 10 minutes)
  evList<eventDelay> eventTenthList;        //pending Event with delay in tenth of sec  (less than 1 Hour)   (abs max 1:49 )
  evList<eventLongDelay> eventSecondsList;  //pending Event with delay in seconds     (abs max more than 100 years )

  // private:
  //
  //
  //   eventItem_t* eventList = nullptr;
  //   delayEventItem_t* eventMillisList = nullptr;       // event < 1 seconde
  //   delayEventItem_t* eventTenthList = nullptr;        // event < 1 Minute
  //   longDelayEventItem_t* eventSecondsList = nullptr;  // autres events up
  //   eventHandler_t* handleEventList = nullptr;
  //   //eventHandler_t*   getEventList = nullptr;
  friend class evHandlerDebug;
  friend class evHandler;
};



extern EvManager Events;

template<typename T>
// push tout les events perimés de la liste sinon decremente leur duree de vie
void evList<T>::parseDelay(const uint16_t delay) {
  evItem* aItem = head;
  while (aItem) {
    //DV_print(aItem->code);
    //DV_println(aItem->curentDelay);
    if (aItem->curentDelay > delay) {
      aItem->curentDelay -= delay;
      aItem = aItem->next;
    } else {
      //Serial.print("done waitingdelay : ");
      //Serial.println(aItem->code);
      //WW delayEventItem_t* aDelayItemPtr = *ItemPtr;
      //event aEvent = *aItem;
      Events.push(*aItem);
      //DV_println((*ItemPtr)->refDelay);

      if (aItem->repeatDelay == 0) {  //true or
        evItem* toRemove = aItem;
        aItem = aItem->next;
        remove(toRemove);
      } else {
        aItem->curentDelay += aItem->repeatDelay;
        if (delay > aItem->curentDelay) {
          aItem->curentDelay = 0;
        } else {
          aItem->curentDelay -= delay;
        }
        aItem = aItem->next;
      }
    }
  }
}
