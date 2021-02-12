//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2020-09-15 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
// ci-test=yes board=168p aes=no

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER
#define SIMPLE_CC1101_INIT
#define NORTC
#define NOCRC
#define SENSOR_ONLY
#define NDEBUG

#define EI_ATTINY24
#define EI_NOTPORTA
#define EI_NOTPORTB
#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <LowPower.h>
#include <AskSinPP.h>
#include <Register.h>
#include <Button.h>
#include "MSP430State.h"

using namespace as;


#define LED_PIN           8
#define LED_PIN2          9
#define CONFIG_BUTTON_PIN A0

#define STROBE_DLY  1500
#define READ_COUNT  4

#define CC1101_PWR    5

#define PEERS_PER_CHANNEL 10

const struct DeviceInfo PROGMEM devinfo = {
  {0x00, 0xc3, 0x41},     // Device ID
  "JPHMRHS001",           // Device Serial
  {0x00, 0xC3},           // Device Model
  0x22,                   // Firmware Version
  as::DeviceType::ThreeStateSensor, // Device Type
  {0x01, 0x00}            // Info Bytes
};

typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 2, CC1101_PWR> RadioType;
typedef DualStatusLed<LED_PIN, LED_PIN2> LedType;
typedef AskSin<LedType, IrqInternalBatt, RadioType> Hal;

DEFREGISTER(Reg0, DREG_CYCLICINFOMSG, MASTERID_REGS, DREG_TRANSMITTRYMAX, DREG_SABOTAGEMSG)
class RHSList0 : public RegList0<Reg0> {
  public:
    RHSList0(uint16_t addr) : RegList0<Reg0>(addr) {}
    void defaults () {
      clear();
      cycleInfoMsg(true);
      transmitDevTryMax(6);
      sabotageMsg(true);
    }
};

DEFREGISTER(Reg1, CREG_AES_ACTIVE, CREG_MSGFORPOS, CREG_EVENTDELAYTIME, CREG_LEDONTIME, CREG_TRANSMITTRYMAX)
class RHSList1 : public RegList1<Reg1> {
  public:
    RHSList1 (uint16_t addr) : RegList1<Reg1>(addr) {}
    void defaults () {
      clear();
      msgForPosA(1); // CLOSED
      msgForPosB(2); // OPEN
      msgForPosC(3); // TILTED
      //aesActive(false);
      eventDelaytime(0);
      ledOntime(100);
      transmitTryMax(6);
    }
};

typedef MSPStateChannel<Hal, RHSList0, RHSList1, DefList4, PEERS_PER_CHANNEL> ChannelType;
typedef StateDevice<Hal, ChannelType, 1, RHSList0> RHSType;

Hal hal;
RHSType sdev(devinfo, 0x20);
ConfigButton<RHSType> cfgBtn(sdev);

void setup () {
  sdev.init(hal);

  hal.led.invert(true);
  hal.battery.init();
  hal.battery.low(22);
  hal.battery.critical(21);

  while (hal.battery.current() == 0);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();

  contactISR(sdev.channel(1), A2);
}

class PowerOffAlarm : public Alarm {
  private:
    bool    timerActive;
  public:
    PowerOffAlarm () : Alarm(0), timerActive(false) {}
    virtual ~PowerOffAlarm () {}

    void activateTimer(bool en) {
      if (en == true && timerActive == false) {
        sysclock.cancel(*this);
        set(millis2ticks(5000));
        sysclock.add(*this);
      } else if (en == false) {
        sysclock.cancel(*this);
      }
      timerActive = en;
    }

    virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
      powerOff();
    }

    void powerOff() {
      hal.led.ledOff();
      hal.radio.setIdle();
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    }

} pwrOffAlarm;

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  pwrOffAlarm.activateTimer( hal.activity.stayAwake() == false &&  worked == false && poll == false );
}

//saves ~646 bytes program size:
//extern "C" void *malloc(size_t size) {return 0;}
//extern "C" void free(void* p) {}
