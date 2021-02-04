//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __BATTERYSENSOR_H__
#define __BATTERYSENSOR_H__

#include "AlarmClock.h"
#include <avr/power.h>
#define ADMUX_VCCWRT1V1 (_BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1))

namespace as {

#define ADMUX_ADCMASK  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(1 << MUX0))
#define ADMUX_REFMASK  ((1 << REFS1)|(1 << REFS0))

#define ADMUX_REF_AREF ((0 << REFS1)|(0 << REFS0))
#define ADMUX_REF_AVCC ((0 << REFS1)|(1 << REFS0))
#define ADMUX_REF_VBG  ((1 << REFS1)|(1 << REFS0))

#define ADMUX_ADC_VBG  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(0 << MUX0))


class IrqBaseBatt {
protected:
  /// value for low battery
  uint8_t m_Low;
  /// value for critical battery
  uint8_t m_Critical;

  static volatile uint16_t __gb_BatCurrent;
  static volatile uint8_t __gb_BatCount;
  static uint16_t (*__gb_BatIrq)();
  uint8_t m_BatSkip;

  IrqBaseBatt () : m_Low(0), m_Critical(0), m_BatSkip(0) {}
  ~IrqBaseBatt () {}

public:
  /** get current battery voltage value
   * \return the current battery value multiplied by 10
   */
  uint8_t current () const { return (__gb_BatCurrent + 50) / 100; }
  /** check if the battery is below critical value
   *  \return true if battery voltage below critical value
   */
  bool critical () const { return current() < m_Critical; }
  /** set critical battery value
   * \param value critical battery value
   */
  void critical (uint8_t value ) { m_Critical = value; }
  /** check if the battery is below low value
   *  \return true if battery voltage below low value
   */
  bool low () const { return current() < m_Low; }
  /** set low battery value
   * \param value low battery value
   */
  void low (uint8_t value ) { m_Low = value; }

  /// for backward compatibility
  uint16_t voltageHighRes() { return __gb_BatCurrent; }
  /// for backward compatibility
  uint8_t voltage() { return current(); }

protected:
  /**
   * Disable the continues battery measurement
   * Called by HAL before enter idle/sleep state
   * Call this before your application code uses the ADC.
   */
  void setIdle () {
    if( __gb_BatCount < 10 ) {
      // if we skip to often - force reading
      if( ++m_BatSkip > 10 ) {
        // wait for valid bat value
        while( __gb_BatCount++ < 10 ) {
          while (ADCSRA & (1 << ADSC)) ; // wait ADC finish
          ADCSRA |= (1 << ADSC);         // start conversion again
        }
        m_BatSkip = 0;
      }
    }
    ATOMIC_BLOCK( ATOMIC_RESTORESTATE ) {
      __gb_BatIrq = 0;
    }

    ADCSRA &= ~((1 << ADIE) | (1 << ADIF));  // disable interrupt
    while (ADCSRA & (1 << ADSC)) ;  // wait finish
    __vectorfunc(); // ensure value is read and stored
  }

  void unsetIdle (uint16_t (*irqfunc)()) {
    //DDECLN(__gb_BatCurrent);
    ATOMIC_BLOCK( ATOMIC_RESTORESTATE ) {
      __gb_BatCount = 0; // reset irq counter
      __gb_BatIrq = irqfunc; // set irq method
    }
    ADMUX &= ~(ADMUX_REFMASK | ADMUX_ADCMASK);
    ADMUX |= ADMUX_REF_AVCC;      // select AVCC as reference
    ADMUX |= ADMUX_ADC_VBG;       // measure bandgap reference voltage
    ADCSRA |= (1 << ADIE) | (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2); // enable interrupt & 128 prescaler
    ADCSRA |= (1 << ADSC);        // start conversion
  }

#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284P__)
  static void __vectorfunc() __asm__("__vector_24")  __attribute__((__signal__, __used__, __externally_visible__));
#else
  static void __vectorfunc() __asm__("__vector_21")  __attribute__((__signal__, __used__, __externally_visible__));
#endif
};

/**
 * IrqInternalBatt class uses continue measurement in background.
 * It uses the ADC and IRQ to get battery voltage during normal operation. If a device needs to sample
 * analog values, it has to call setIdle() before and unsetIdle() after analogRead().
 */
class IrqInternalBatt : public IrqBaseBatt {

public:
  /** Constructor
   */
  IrqInternalBatt () {}
  /** Destructor
   */
  ~IrqInternalBatt() {}
  /** init measurement with period and used clock
   * \param period ticks until next measurement
   * \param clock clock to use for waiting
   */
  void init() {
    unsetIdle();
  }

  /**
   * Disable the continues battery measurement
   * Called by HAL before enter idle/sleep state
   * Call this before your application code uses the ADC.
   */
  void setIdle () {
    IrqBaseBatt::setIdle();
  }
  /**
   * Enable the continues measurement of the battery voltage
   * Called by HAL after return from idle/sleep state
   * Call this after the application doesn't need ADC longer
   */
  void unsetIdle () {
    IrqBaseBatt::unsetIdle(irq);
    // wait for stable values
    /*
    int maxnum = 50;  // we will wait max 50
    uint16_t last=0 ,current=0;
    do {
      last = current;
      while (ADCSRA & (1 << ADSC)) ; // wait ADC finish
      current = ADC >> 2; // remove some bits ???
    } while( current != last && --maxnum > 0);
    */
  }
  /** ISR function to get current measured value
   */
  static uint16_t irq () {
      return 1100UL * 1024 / ADC;
  }

};


}

#endif
