#define EI_NOTPORTB
#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <LowPower.h>

#define STROBE_DLY  1500
#define READ_COUNT  4

unsigned long ms = 0;
volatile bool isInterrupt = false;

void isr() {
  isInterrupt = true;
}

bool strobeA1readA3() {
  digitalWrite(A1, HIGH);
  _delay_us(STROBE_DLY);
  bool a3 = digitalRead(A3);
  digitalWrite(A1, LOW);
  _delay_us(STROBE_DLY);
  return a3;
}

uint8_t readMSP430() {
  uint8_t savedSREG = SREG;
  cli();

  uint8_t mspState = 0;

  for (uint8_t i = 0; i < READ_COUNT; i++) {
    while (digitalRead(A2) == LOW);
    if (strobeA1readA3()) bitSet(mspState, i);
  }

  delay(20);

  // for (uint8_t i = 0; i < READ_COUNT; i++)
  //   Serial.print(bitRead(mspState, READ_COUNT - i - 1), DEC);
  // Serial.println();

  if (bitRead(mspState, 0)) {
    Serial.print("BOOT+");
    if (bitRead(mspState, 3))
      Serial.println("SABOTAGE");
    else
      Serial.println("NO SAB");
  }

  if (mspState == 0b00001100) Serial.println("ZU");
  if (mspState == 0b00000100) Serial.println("KIPP");
  if (mspState == 0b00001000) Serial.println("AUF");
  if (mspState == 0b00001010) Serial.println("SABOTAGE");
  if (mspState == 0b00000010) Serial.println("NO SAB");

  SREG = savedSREG;
  return mspState;
}

void initMSP430() {
  while (!strobeA1readA3());
  _delay_us(STROBE_DLY >> 4);
  if (digitalRead(A2) == HIGH || digitalRead(A3) == HIGH) strobeA1readA3();
  _delay_us(STROBE_DLY >> 4);
  if (digitalRead(A2) == HIGH || digitalRead(A3) == HIGH) strobeA1readA3();

  enableInterrupt(A2, isr, RISING);
}

void setup() {
  Serial.begin(57600);
  Serial.println("START");
  pinMode(8, OUTPUT); //green led
  pinMode(9, OUTPUT); //red led

  digitalWrite(8, HIGH); //green led off
  digitalWrite(9, LOW);  //start init - red led on

  /* DEFINE MSP430 WIRES */
  pinMode(A1, OUTPUT); digitalWrite(A1, LOW);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);  //unused?
  /* END */

  delay(2000);  // eQ-3 waits 3 seconds on the HM-SCI-3-FM

  initMSP430();

  digitalWrite(9, HIGH); //init done - red led off
  digitalWrite(8, LOW);  //init done - green led on

  ms = millis();
}


void loop() {
  if (isInterrupt == true) {
    isInterrupt = false;
    readMSP430();
    ms = millis();
  }

  if (millis() - ms > 2000) {
    digitalWrite(8, HIGH);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    digitalWrite(8, LOW);
  }

}
