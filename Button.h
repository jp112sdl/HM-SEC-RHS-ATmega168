//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
#ifndef __BUTTON_H__
#define __BUTTON_H__

namespace as {
  
class StateButton: public Alarm {

#define DEBOUNCETIME millis2ticks(50)

  public:
    enum States {
      invalid = 0,
      none = 1,
      released = 2,
      pressed = 3,
      debounce = 4,
      longpressed = 5,
      longreleased = 6,
    };

    class CheckAlarm : public Alarm {
      public:
        StateButton& sb;
        CheckAlarm (StateButton& _sb) : Alarm(0), sb(_sb) {}
        virtual ~CheckAlarm () {}
        virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
          sb.check();
        }
    };

  protected:
    uint8_t  stat     : 3;
    uint8_t  pinstate : 1;
    uint8_t  pin;
    uint16_t longpresstime;
    CheckAlarm ca;

  public:
    StateButton() :
      Alarm(0), stat(none), pinstate(HIGH), pin(0), longpresstime(seconds2ticks(3)), ca(*this)  {
    }
    virtual ~StateButton() {
    }


    virtual void trigger(AlarmClock& clock) {
      uint8_t  nextstate = invalid;
      uint16_t nexttick = 0;
      switch ( state() ) {
        case released:
        case longreleased:
          nextstate = none;
          break;

        case debounce:
          nextstate = pressed;
          if (pinstate == LOW) {
            // set timer for detect longpressed
            nexttick = longpresstime - DEBOUNCETIME;
          } else {
            nextstate = released;
            nexttick = DEBOUNCETIME;
          }
          break;

        case pressed:
        case longpressed:
          if ( pinstate == LOW) {
            nextstate = longpressed;
            nexttick = longpresstime;
          }
          break;
      }
      // reactivate alarm if needed
      if ( nexttick != 0 ) {
        tick = nexttick;
        clock.add(*this);
      }
      // trigger the state change
      if ( nextstate != invalid ) {
        state(nextstate);
      }
    }

    virtual void state(uint8_t s) {
      /*switch(s) {
        case released: DPRINTLN(F(" released")); break;
        case pressed: DPRINTLN(F(" pressed")); break;
        case debounce: DPRINTLN(F(" debounce")); break;
        case longpressed: DPRINTLN(F(" longpressed")); break;
        case longreleased: DPRINTLN(F(" longreleased")); break;
        default: DPRINTLN(F("")); break;
        }*/
      stat = s;
    }

    uint8_t state() const {
      return stat;
    }

    void irq () {
      sysclock.cancel(ca);
      // use alarm to run code outside of interrupt
      sysclock.add(ca);
    }

    void check() {
      uint8_t ps = digitalRead(pin);
      if ( pinstate != ps ) {
        pinstate = ps;
        uint16_t nexttick = 0;
        uint8_t  nextstate = state();
        switch ( state() ) {
          case none:
            nextstate = debounce;
            nexttick = DEBOUNCETIME;
            break;

          case pressed:
          case longpressed:
            if (pinstate == HIGH) {
              nextstate = state() == pressed ? released : longreleased;
              nexttick = DEBOUNCETIME;
            }
            break;
          default:
            break;
        }
        if ( nexttick != 0 ) {
          sysclock.cancel(*this);
          tick = nexttick;
          sysclock.add(*this);
        }
        if ( nextstate != state () ) {
          state(nextstate);
        }
      }
    }

    void init(uint8_t pin) {
      this->pin = pin;
      pinMode(pin, INPUT_PULLUP);
    }
};


template <class DEVTYPE>
class ConfigButton : public StateButton {
    DEVTYPE& device;
  public:
    typedef StateButton ButtonType;

    ConfigButton (DEVTYPE& dev) : device(dev) {    }
    virtual ~ConfigButton () {}
    virtual void state (uint8_t s) {
      uint8_t old = ButtonType::state();
      ButtonType::state(s);
      if ( s == ButtonType::released ) {
        device.startPairing();
      }
      else if ( s == ButtonType::longpressed ) {
        if ( old == ButtonType::longpressed ) {
          if ( device.getList0().localResetDisable() == false ) {
            device.reset(); // long pressed again - reset
          }
        }
        else {
          device.led().set(LedStates::key_long);
        }
      }
    }
};



#define buttonISR(btn,pin) class btn##ISRHandler { \
    public: \
      static void isr () { btn.irq(); } \
  }; \
  btn.init(pin); \
  enableInterrupt(pin,btn##ISRHandler::isr,CHANGE);

}
#endif
