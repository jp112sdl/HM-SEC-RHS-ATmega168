//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__

#include "AlarmClock.h"
#include "Radio.h"
#include <LowPower.h>

namespace as {

#if defined(ARDUINO_ARCH_AVR) && ! (defined(ARDUINO_AVR_ATmega32) || defined(__AVR_ATmega644__))

/*class Sleep {
public:

  static uint32_t doSleep (uint32_t ticks) {
    uint32_t offset = 0;
    period_t sleeptime = SLEEP_FOREVER;
    if( ticks > millis2ticks(500) ) { offset = millis2ticks(500); sleeptime = SLEEP_500MS; }
    else if( ticks > millis2ticks(250) ) { offset = millis2ticks(250); sleeptime = SLEEP_250MS; }
    else if( ticks > millis2ticks(120) ) { offset = millis2ticks(120); sleeptime = SLEEP_120MS; }
    else if( ticks > millis2ticks(60)  ) { offset = millis2ticks(60);  sleeptime = SLEEP_60MS; }
    else if( ticks > millis2ticks(30)  ) { offset = millis2ticks(30);  sleeptime = SLEEP_30MS; }
    else if( ticks > millis2ticks(15)  ) { offset = millis2ticks(15);  sleeptime = SLEEP_15MS; }

    LowPower.powerDown(sleeptime,ADC_OFF,BOD_OFF);
    return offset;
  }

  template <class Hal>
  static void powerSave (Hal& hal) {
    sysclock.disable();
    uint32_t ticks = sysclock.next();
    if( sysclock.isready() == false ) {
      if( ticks == 0 || ticks > millis2ticks(15) ) {
        hal.setIdle();
        uint32_t offset = doSleep(ticks);
        hal.unsetIdle();
        sysclock.correct(offset);
        sysclock.enable();
      }
      else{
        hal.radio.disable();
        digitalWrite(5, HIGH); //disable cc1101
        sysclock.enable();
        LowPower.idle(SLEEP_FOREVER,ADC_OFF,TIMER2_OFF,TIMER1_ON,TIMER0_OFF,SPI_ON,USART0_ON,TWI_OFF);
      }
    }
    else {
      sysclock.enable();
    }
  }
};
*/
#endif

class Activity : public Alarm {

  volatile bool  awake;

public:

  Activity () : Alarm(0), awake(false) {
    async(true);
  }

  virtual ~Activity () {}

  virtual void trigger (__attribute__((unused)) AlarmClock& clock) {
    awake = false;
  }

  // do not sleep for time in ticks
  void stayAwake (uint32_t time) {
    uint32_t old = sysclock.get(*this);
    if( old < time ) {
      awake = true;
      sysclock.cancel(*this);
      tick = time;
      sysclock.add(*this);
    }
  }

  bool stayAwake () const {
    return awake;
  }

  /*template <class Hal>
  void savePower (Hal& hal) {
    if( awake == false ) {
      Sleep::powerSave(hal);
    }
    else {
      // ensure radio is up and running
      hal.wakeup();
    }
  }*/

};

}

#endif
