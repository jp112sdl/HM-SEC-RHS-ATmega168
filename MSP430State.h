//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2017-10-19 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2020-01-30 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __MSP430CONTACTSTATE_H__
#define __MSP430CONTACTSTATE_H__

#include <MultiChannelDevice.h>

enum State { NoPos = 0, PosA, PosB, PosC };

#define STROBE_DLY  1500
#define READ_COUNT  4

namespace as {

template <class HALTYPE, class List0Type, class List1Type, class List4Type, int PEERCOUNT>
class MSPStateChannel : public Channel<HALTYPE, List1Type, EmptyList, List4Type, PEERCOUNT, List0Type> {

    class EventSender : public Alarm {
      public:
        MSPStateChannel& channel;
        uint8_t count, state;

        EventSender (MSPStateChannel& c) : Alarm(0), channel(c), count(0), state(0) {}
        virtual ~EventSender () {}
        virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
          SensorEventMsg& msg = (SensorEventMsg&)channel.device().message();
          msg.init(channel.device().nextcount(), channel.number(), count++, state, channel.device().battery().low());
          channel.device().sendPeerEvent(msg, channel);
        }
    };

    EventSender sender;

    class CheckAlarm : public Alarm {
      public:
        MSPStateChannel& channel;
        CheckAlarm (MSPStateChannel& _channel) : Alarm(0), channel(_channel) {}
        virtual ~CheckAlarm () {}
        virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
          channel.check();
        }
    };

  private:
    volatile bool canInterrupt;
    bool sabotage, sabstate, cycleEnabled;
  private:
    bool strobeA1readA3() {
      digitalWrite(A1, HIGH);
      _delay_us(STROBE_DLY);
      bool a3 = digitalRead(A3);
      digitalWrite(A1, LOW);
      _delay_us(STROBE_DLY);
      return a3;
    }
  protected:
    CheckAlarm ca;
  public:
    typedef Channel<HALTYPE, List1Type, EmptyList, List4Type, PEERCOUNT, List0Type> BaseChannel;

    MSPStateChannel () : BaseChannel(), sender(*this), canInterrupt(true), sabotage(false), sabstate(false), cycleEnabled(true), ca(*this) {}
    virtual ~MSPStateChannel () {}

    void setup(Device<HALTYPE, List0Type>* dev, uint8_t number, uint16_t addr) {
      BaseChannel::setup(dev, number, addr);
    }

    void irq() {
      sysclock.cancel(ca);
      if (canInterrupt) {
        sysclock.add(ca);
      }
    }

    void init () {
      pinMode(A1, OUTPUT); digitalWrite(A1, LOW);
      pinMode(A2, INPUT);
      pinMode(A3, INPUT);
      pinMode(A4, INPUT);

      delay(2000);  // eQ-3 waits 3 seconds on the HM-SCI-3-FM

      while (!strobeA1readA3());
      _delay_us(STROBE_DLY >> 4);
      if (digitalRead(A2) == HIGH || digitalRead(A3) == HIGH) strobeA1readA3();
      _delay_us(STROBE_DLY >> 4);
      if (digitalRead(A2) == HIGH || digitalRead(A3) == HIGH) strobeA1readA3();
    }

    uint8_t status () const {
      return sender.state;
    }

    uint8_t flags () const {
      uint8_t flags = sabotage ? 0x07 << 1 : 0x00;
      flags |= this->device().battery().low() ? 0x80 : 0x00;
      return flags;
    }

    void setCycle(bool b) {
      cycleEnabled = b;
    }

    void check() {
      canInterrupt = false;
      uint8_t newstate = sender.state;

      uint8_t posi = State::NoPos;

      uint8_t mspState = 0;

      for (uint8_t i = 0; i < READ_COUNT; i++) {
        while (digitalRead(A2) == LOW);
        if (strobeA1readA3()) bitSet(mspState, i);
      }

      delay(20);

      bool boot = bitRead(mspState, 0);
      if (boot == true) {
        sabstate = bitRead(mspState, 3);
      } else {
        if (mspState == 0b00000100) posi = State::PosA;
        if (mspState == 0b00001100) posi = State::PosB;
        if (mspState == 0b00001000) posi = State::PosC;

        if (mspState == 0b00001010) sabstate = true;
        if (mspState == 0b00000010) sabstate = false;

        uint8_t msg = 0;
        switch ( posi ) {
          case State::PosA:
            msg = this->getList1().msgForPosA();
            break;
          case State::PosB:
            msg = this->getList1().msgForPosB();
            break;
          case State::PosC:
            msg = this->getList1().msgForPosC();
            break;
          default:
            break;
        }

        if ( msg == 1) newstate = 0;
        else if ( msg == 2) newstate = 200;
        else if ( msg == 3) newstate = 100;

        uint8_t delay = this->getList1().eventDelaytime();
        sender.state = newstate;
        sysclock.cancel(sender);
        if ( delay == 0 ) {
          sender.trigger(sysclock);
        }
        else {
          sender.set(AskSinBase::byteTimeCvtSeconds(delay));
          sysclock.add(sender);
        }
        uint16_t ledtime = (uint16_t)this->getList1().ledOntime() * 5;
        if ( ledtime > 0 ) {
          this->device().led().ledOn(millis2ticks(ledtime), 0);
        }
      }

      if ( (sabotage != sabstate && this->device().getList0().sabotageMsg() == true) || (cycleEnabled && boot) ) {
        sabotage = sabstate;
        this->changed(true);
      }

      canInterrupt = true;
    }
};

template<class HalType, class ChannelType, int ChannelCount, class List0Type>
class StateDevice : public MultiChannelDevice<HalType, ChannelType, ChannelCount, List0Type> {
  public:
    typedef MultiChannelDevice<HalType, ChannelType, ChannelCount, List0Type> DevType;
    StateDevice(const DeviceInfo& info, uint16_t addr) : DevType(info, addr) {}
    virtual ~StateDevice () {}

    virtual void configChanged () {
      this->channel(1).setCycle(this->getList0().cycleInfoMsg());
    }
};


#define contactISR(chan,pin) class __##pin##ISRHandler { \
    public: \
      static void isr () { chan.irq(); } \
  }; \
  chan.init(); \
    enableInterrupt(pin,__##pin##ISRHandler::isr,RISING);

}

#endif
