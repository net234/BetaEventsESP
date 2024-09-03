
/*************************************************
 *************************************************
    handler evHandlers.cpp   validation of lib betaEvents to deal nicely with events programing with Arduino
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

   works with beteEvents 2.0

    V2.0  20/04/2021
    - Mise en liste chainée de modules 'events'
      evHandlerSerial   Gestion des caracteres et des chaines provenant de Serial
      evHandlerLed      Gestion d'une led avec ou sans clignotement sur un GPIO (Multiple instance possible)
      evHandlerButton   Gestion d'un pousoir sur un GPIO (Multiple instance possible)
      evHandlerDebug    Affichage de l'occupation CPU, de la memoire libre et des evenements 100Hz 10Hz et 1Hz
    V2.0.1  26/10/2021
      corections evHandlerLed sur le true/false
    V2.2  27/10/2021
       more arduino like lib with self built in instance
    V2.2a  11/11/2021
       add begin in evHandles

    V2.3    09/03/2022   isolation of evHandler for compatibility with dual core ESP32

    *************************************************/
#include "evHelpers.h"
#include "evHandlers.h"
#include "EventsManagerESP.h"


evHandler::evHandler() {
  //   next = nullptr;
  Events.handlerList.addHandler(this);
};




/**********************************************************

   gestion d'un output generique

 ***********************************************************/

evHandlerOutput::evHandlerOutput(const uint8_t aEventCode, const uint8_t aPinNumber, const bool aStateON)
  : pinNumber(aPinNumber), stateON(aStateON), evCode(aEventCode){};

void evHandlerOutput::begin() {
  pinMode(pinNumber, OUTPUT);
}


void evHandlerOutput::handle() {
  if (Events.code == evCode) {
    switch (Events.ext) {
      case evxOff:
        setOn(false);
        break;

      case evxOn:
        setOn(true);
        //digitalWrite(pinNumber, (percent == 0) xor levelON);
        break;
    }
  }
}

bool evHandlerOutput::isOn() {
  return state;
};

void evHandlerOutput::setOn(const bool status) {
  state = status;
  digitalWrite(pinNumber, (not status) xor stateON);
}

void evHandlerOutput::pulse(const uint32_t aDelay) {  // pulse d'allumage simple
  if (aDelay == 0) {
    Events.delayedPushMillis(0, evCode, evxOff);
    return;
  }
  Events.delayedPushMillis(0, evCode, evxOn);
  Events.delayedPushMillis(aDelay, evCode, evxOff);
}


/**********************************************************

   gestion d'une Led sur un port   clignotement en frequence ou en millisecondes

 ***********************************************************/

evHandlerLed::evHandlerLed(const uint8_t aEventCode, const uint8_t aPinNumber, const bool revert, const uint8_t frequence)
  : evHandlerOutput(aEventCode, aPinNumber, revert) {
  setFrequence(frequence);
};

void evHandlerLed::handle() {
  if (Events.code == evCode) {
    evHandlerOutput::handle();
    switch (Events.ext) {

      case evxBlink:
        Events.push(evCode, (percent > 0) ? evxOn : evxOff);  // si percent d'allumage = 0 on allume pas
        if (percent > 0 && percent < 100) {                   // si percent = 0% ou 100% on ne clignote pas
          Events.delayedPushMillis(millisecondes * percent / 100, evCode, evxOff);
          Events.forceDelayedPushMillis(millisecondes, evCode, evxBlink);
        }
        break;
    }
  }
}

void evHandlerLed::setOn(const bool status) {
  Events.removeDelayEvent(evCode);
  evHandlerOutput::setOn(status);  // make result instant needed  outside event loop
}


void evHandlerLed::setMillisec(const uint16_t aMillisecondes, const uint8_t aPercent) {
  millisecondes = max(aMillisecondes, (uint16_t)2);
  percent = aPercent;
  Events.delayedPushMillis(0, evCode, evxBlink);
}

void evHandlerLed::setFrequence(const uint8_t frequence, const uint8_t percent) {
  if (frequence == 0) {
    setMillisec(0, 0);
    return;
  }
  setMillisec(1000U / frequence, percent);
}


/**********************************************************

   gestion d'un poussoir sur un port   genere evBPDown, evBPUp, evBPLongDown, evBPLongUp

 ***********************************************************/



evHandlerButton::evHandlerButton(const uint8_t aEventCode, const uint8_t aPinNumber, const uint16_t aLongDelay)
  : evCode(aEventCode), pinNumber(aPinNumber), longDelay(aLongDelay){};

void evHandlerButton::begin() {
  pinMode(pinNumber, INPUT_PULLUP);
  multi = 0;
};

void evHandlerButton::handle() {
  if (Events.code == ev10Hz) {

    if (state == (digitalRead(pinNumber) == LOW)) return;  // rien n'a changé

    state = !state;
    // si enfoncé -> evXon
    if (state) {
      if (multi < 100) multi++;
      Events.push(evCode, evxOn);
      Events.delayedPushMillis(longDelay, evCode, evxLongOn);  // arme un event BP long On
      return;
    }
    // si relaché -> evxOff
    Events.push(evCode, evxOff);
    Events.delayedPushMillis(longDelay, evCode, evxLongOff);  // arme un event BP long Off

    return;
  }

  //raz multi sur un relaché long
  if (Events.code == evCode and Events.ext == evxLongOff) multi = 0;  
  }

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

//evHandlerKeypad::evHandlerKeypad(const uint8_t aEventCode, const uint8_t aPinNumber, const uint16_t aRepeatDelay)
//  : evCode(aEventCode), pinNumber(aPinNumber), repeatDelay(aRepeatDelay){};

/*
  evHandlerKeypad::begin() {
   //TODO: aTable to setup spec ific value for the pad
  }
*/
void evHandlerKeypad::handle() {
  if (Events.code == evCode and Events.ext == evxRepeat) {
    if (currentDelay >= repeatDelay / 4) {
      currentDelay -= repeatDelay / 10;
    }
    Events.delayedPushMillis(currentDelay, evCode, evxRepeat, key);  // arme un event BP long On
    return;
  }
  if (Events.code != ev10Hz) return;

  int aValue = analogRead(pinNumber);
  if (abs(value - aValue) > 10) {

    pendingValue = true;
    value = aValue;
    //DTV_println("Pending keypad", value);
    return;
  }
  if (!pendingValue) return;

  pendingValue = false;
  //DV_print(value);
  //key = 0;
  if (value > 1000) {  //key
    //ZZ   Events.delayedPushMillis(0, evCode, evxOff, key);  //clear repeat
    return;
  } else if (value > 450) {
    key = 0b00000010;  //droite
  } else if (value > 380) {
    key = 0b00000100;  //gauche
  } else if (value > 340) {
    key = 0b00001000;  //haut
  } else if (value > 260) {
    key = 0b00000110;  //gauche + droite
  } else if (value > 180) {
    key = 0b00010000;  //bas
  } else if (value > 140) {
    key = 0b00011000;  //haut + bas
  } else {
    key = 0b00000001;  //millieux
  }
  //DV_print(key);
  Events.push(evCode, evxOn, key);
  if (key != 1) {
    currentDelay = repeatDelay;
    Events.delayedPushMillis(currentDelay, evCode, evxRepeat, key);  // arme un event BP long On
  }
  return;
}



/**********************************************************

   gestion de Serial pour generer les   evInChar et  evInString

 ***********************************************************/

evHandlerSerial::evHandlerSerial(const uint32_t aSerialSpeed, const uint8_t inputStringSize)
  : serialSpeed(aSerialSpeed),
    inputStringSizeMax(inputStringSize) {
  inputString.reserve(inputStringSize);
}

void evHandlerSerial::begin() {
  Serial.begin(serialSpeed);
}

byte evHandlerSerial::get() {
  if (stringComplete) {
    stringComplete = false;
    stringErase = true;  // la chaine sera effacee au prochain caractere recu
    Events.strPtrParam = &inputString;
    return (Events.code = evInString);
  }
  if (Serial.available()) {
    inputChar = Serial.read();
    if (stringErase) {
      inputString = "";
      stringErase = false;
    }
    if (isPrintable(inputChar) && (inputString.length() <= inputStringSizeMax)) {
      inputString += inputChar;
    };
    if (inputChar == '\n' || inputChar == '\r') {
      stringComplete = (inputString.length() > 0);
    }
    Events.cParam = inputChar;
    return (Events.code = evInChar);
  }
  return (evNill);
}


/**********************

   forcage d'une inputstring
*/

void evHandlerSerial::setInputString(const String aStr) {
  inputString = aStr;
  stringErase = false;
  stringComplete = (inputString.length() > 0);
}



/**********************************************************

   gestion d'un traceur de debugage touche 'T' pour visualiser la charge CPU

 ***********************************************************/


void evHandlerDebug::handle() {
  switch (Events.code) {
    case ev1Hz:
      if (trackTime) {
        Serial.print(Digit2_str(hour()));
        Serial.print(':');
        Serial.print(Digit2_str(minute()));
        Serial.print(':');
        Serial.print(Digit2_str(second()));
        Serial.print(F(",CPU="));
        Serial.print(Events._percentCPU);
        Serial.print('%');
        if (trackTime < 2) {
          Serial.print(F(",Loop="));
          Serial.print(Events._loopParsec);
          Serial.print(F(",Nill="));
          Serial.print(Events._evNillParsec);
          Serial.print(F(",Ram="));
          Serial.print(helperFreeRam());
#ifdef ESP8266
          Serial.print(F(",Frag="));
          Serial.print(ESP.getHeapFragmentation());
          Serial.print(F("%,MaxMem="));
          Serial.print(ESP.getMaxFreeBlockSize());
#endif
        }
        if (ev100HzMissed + ev10HzMissed) {
          Serial.print(F(" Miss:"));
          Serial.print(ev100HzMissed);
          Serial.print('/');
          Serial.print(ev10HzMissed);
          ev100HzMissed = 0;
          ev10HzMissed = 0;
        }
        Serial.println();
      }

      break;

    case ev10Hz:

      ev10HzMissed += Events.iParam - 1;
      if (trackTime > 1) {

        if (Events.iParam > 1) {
          //        for (int N = 2; N<currentEvent.param; N++) Serial.print(' ');
          Serial.print('X');
          Serial.print(Events.iParam - 1);
        } else {
          Serial.print('|');
        }
      }
      break;

    case ev100Hz:
      ev100HzMissed += Events.iParam - 1;

      if (trackTime > 2) {

        if (Events.iParam > 1) {
          //      for (int N = 3; N<currentEvent.param; N++) Serial.print(' ');
          Serial.print('x');
          Serial.print(Events.iParam - 1);
        } else {
          Serial.print('_');
        }
      }
      break;
    case evInString:
      if (Events.strPtrParam->equals("T")) {
        if (++(trackTime) > 3) trackTime = 0;
        DV_println(trackTime);
      }

      break;
  }
};

#endif
