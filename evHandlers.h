/*************************************************
 *************************************************
    handler evHandlers.h   validation of lib betaEvents to deal nicely with events programing with Arduino
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

    V2.0  20/04/2021
    - Mise en liste chainée de modules 'events'
      evHandlerSerial   Gestion des caracteres et des chaines provenant de Serial
      evHandlerLed      Gestion d'une led avec ou sans clignotement sur un GPIO (Multiple instance possible)
      evHandlerButton   Gestion d'un pousoir sur un GPIO (Multiple instance possible)
      evHandlerDebug    Affichage de l'occupation CPU, de la memoire libre et des evenements 100Hz 10Hz et 1Hz

   History

   works with beteEvents 2.0

    V2.0  20/04/2021
    - Mise en liste chainée de modules 'events' test avec un evButton
    V2.0.1  26/10/2021
      corections evHandlerLed sur le true/false
    V2.2  27/10/2021
       more arduino like lib with self built in instance
    V2.2a  11/11/2021
       add begin in evHandles

    V2.3    09/03/2022   isolation of evHandler for compatibility with dual core ESP32

    V2.4    30/09/2022   Isolation des IO (evhandlerOutput)
    01/09/24   V4.1  add handler pour serial2

    *************************************************/
#pragma once
#include <Arduino.h>


//Basic system events
enum tevCore : uint8_t {
  evNill = 0,  // No event  about 1 every milisecond but do not use them for delay Use delayedPushEvent(delay,event)
  ev100Hz,     // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,      // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,       // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,       // 24H when timestamp pass over 24H
  evInit,      // event pushed par MyEvent.begin()
  evPostInit,
  evInChar,
  evInString,
  evHARD = 20,  //??
  //evDth20,
  //evDs18b20,
  evBNODES = 50,
  // evOta = 50,   // a migrer dans bnOdes tools
  // evWifi,
  // evTagHisto,
  evUser = 100,
};



/**********************************************************

   gestion d'un output simple

 ***********************************************************/
typedef enum {
  // evenement etendus pour les Input Output
  evxOff,      // IO Off (les ou Relais)
  evxOn,       // IO on
  evxRepeat,   // auto repeat (keypad)
  evxBlink,    // clignotement actif (LED)
  evxLongOff,  // poussoir relaché longtemp
  evxLongOn,   // pousoir enfoncé longtemps
} tIOEventExt;


class evHandler {
public:
  //evHandler *next;  // handle suivant
  evHandler();
  virtual ~evHandler(){};
  virtual void begin(){};   // called with eventManager::begin
  virtual void handle(){};  // called
  virtual byte get() {
    return evNill;
  };
  //    EventManager evManager;
};

// template pour une liste d'event Handler
template<typename T>
class evHandlerList {
private:
  // Définition de la structure du nœud
  struct evHandlerItem {
    T* evHandler;
    evHandlerItem* next;  // Pointeur vers le prochain nœud

    evHandlerItem(T* evHandler, evHandlerItem* next = nullptr)
      : evHandler(evHandler), next(next) {}
  };

  evHandlerItem* head;  // Pointeur vers le premier nœud de la liste
  evHandlerItem* tail;  // Pointeur vers le dernier nœud de la liste

public:
  evHandlerList()
    : head(nullptr), tail(nullptr) {}

  // ~evHandlerList() {
  //   //clear();
  // }

  void addHandler(T* aHandler) {
    evHandlerItem* aItem = new evHandlerItem(aHandler);
    if (tail) {
      tail->next = aItem;
    } else {
      head = aItem;
    }
    tail = aItem;
  };
  void parseBegin() {
    evHandlerItem* current = head;
    while (current) {
      current->evHandler->begin();
      current = current->next;
    }
  }
  void parseHandle() {
    evHandlerItem* current = head;
    while (current) {
      current->evHandler->handle();
      current = current->next;
    }
  }
  uint8_t parseGet() {
    evHandlerItem* current = head;
    while (current) {
      if (current->evHandler->get()) return (true);
      current = current->next;
    }
    return (false);
  }




  /*
      void clear() {
          Node* current = head;
          while (current) {
              Node* toDelete = current;
              current = current->next;
              delete toDelete->event;
              delete toDelete;
          }
          head = nullptr;
          tail = nullptr;
      }
    */

  //friend class evHandlerList<evHandler>;
};


class evHandlerOutput : public evHandler {

public:
  evHandlerOutput(const uint8_t aEventCode, const uint8_t aPinNumber, const bool stateON = HIGH);
  virtual void begin() override;
  virtual void handle() override;
  bool isOn();
  void setOn(const bool status = true);
  void pulse(const uint32_t millisecondes);  // pulse d'allumage simple



private:
  uint8_t pinNumber;
  bool stateON;  // = HIGH;
  bool state;    // = false;

protected:
  uint8_t evCode;
};

/**********************************************************

   gestion d'une Led sur un port   clignotement en frequence ou en millisecondes

 ***********************************************************/






class evHandlerLed : public evHandlerOutput {
public:
  evHandlerLed(const uint8_t aEventCode, const uint8_t aPinNumber, const bool stateON = HIGH, const uint8_t frequence = 0);
  //virtual void begin()  override;
  virtual void handle() override;
  void setOn(const bool status = true);
  void setFrequence(const uint8_t frequence, const uint8_t percent = 10);      // frequence de la led
  void setMillisec(const uint16_t millisecondes, const uint8_t percent = 10);  // frequence de la led

private:
  uint16_t millisecondes;
  uint8_t percent;
};


/**********************************************************

   gestion d'un poussoir sur un port   genere evxOn, evxOff, evxLongOn, evxLongOff
   geston du click rapide

 ***********************************************************/




class evHandlerButton : public evHandler {
public:
  evHandlerButton(const uint8_t aEventCode, const uint8_t aPinNumber, const uint16_t aLongDelay = 1500);
  virtual void begin() override;
  virtual void handle() override;
  bool isOn() {
    return state;
  };

  uint8_t multi = 0;
protected:
  uint8_t evCode;


private:
  uint8_t pinNumber;
  uint16_t longDelay;
  bool state = false;
};

#ifndef __AVR_ATtiny85__

/***************************************************************


  Gestion de pousoirs avec un port analogique (A0)
  genere un evxOn  evxOff
  si repeatDelay est fournis un evxRepeat est generé

   +------ A0
    |
    +------R10K-----V3.3
    |
    +-[SEL]------------GND
    |
    +-[NEXT]----R2K----GND
    |
    +-[BEFORE]--R4,7K--GND
    |
    +-[INC]---- R5,6K--GND
    |
    +-[DEC]-----R7,5K--GND


****************************************************************/

class evHandlerKeypad : public evHandler {
public:
  evHandlerKeypad(const uint8_t aEventCode, const uint8_t aPinNumber = A0, const uint16_t aRepeatDelay = 1000)
    : evCode(aEventCode), pinNumber(aPinNumber), repeatDelay(aRepeatDelay){};
  //virtual void begin() override;
  virtual void handle() override;
  uint16_t getKey() {
    return key;
  };

protected:
  uint8_t evCode;

private:
  uint8_t pinNumber;  //On esp only A0   (for ESP32 version)
  uint16_t repeatDelay;
  uint16_t currentDelay;
  uint8_t key = 0;
  uint16_t value = 0;
  bool pendingValue = false;
};



/**********************************************************

   gestion de Serial pour generer les   evInChar et  evInString

 ***********************************************************/

class evHandlerSerial : public evHandler {
public:
  evHandlerSerial(const uint32_t aSerialSpeed = 115200, const uint8_t inputStringSize = 100);
  virtual void begin() override;
  //virtual void handle()  override;
  virtual byte get() override;
  void setInputString(const String aStr);
  String inputString = "";
  char inputChar = '\0';
private:

  uint32_t serialSpeed;
  uint8_t inputStringSizeMax;
  bool stringComplete = false;
  bool stringErase = false;
    friend class evHandlerSerial1;
};





/**********************************************************

   gestion d'un traceur de debugage touche 'T' pour visualiser la charge CPU

 ***********************************************************/



class evHandlerDebug : public evHandler {
public:
  //evHandlerDebug();
  virtual void handle() override;
  uint8_t trackTime = 0;
private:
  uint16_t ev100HzMissed = 0;
  uint16_t ev10HzMissed = 0;
};

#endif
