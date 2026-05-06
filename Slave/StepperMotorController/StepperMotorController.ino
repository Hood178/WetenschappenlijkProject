/**
 * StepperMotorController.ino
 *
 * Arduino Nano R4 I2C slave for a DM320T stepper driver.
 *
 * Pin mapping (as requested)
 * --------------------------
 *  SDA  : 18
 *  SCL  : 19
 *  EN   : 6
 *  DIR  : 7
 *  OPTO : 8
 *  PUL  : 9
 *
 * DIP switches (pins 2..5)
 * ------------------------
 *  The 7-bit slave address is built as: 000 + dip[3:0]
 *  bit0 -> pin 2, bit1 -> pin 3, bit2 -> pin 4, bit3 -> pin 5
 *
 * Registers
 * ---------
 *  0x00 (R/W): ENABLE
 *      write 0x01 -> enable driver
 *      write 0x00 -> disable driver
 *      read        -> 0x01 enabled, 0x00 disabled
 */

#include <Wire.h>

// DM320T signal pins
const uint8_t PIN_EN = 6;
const uint8_t PIN_DIR = 7;
const uint8_t PIN_OPTO = 8;
const uint8_t PIN_PUL = 9;

// DIP switches for low 4 address bits
const uint8_t DIP_PIN_0 = 2;
const uint8_t DIP_PIN_1 = 3;
const uint8_t DIP_PIN_2 = 4;
const uint8_t DIP_PIN_3 = 5;

// Register map
const uint8_t REG_ENABLE = 0x00;

volatile uint8_t currentRegister = REG_ENABLE;
volatile bool motorEnabled = false;
uint8_t slaveAddress = 0x00;

uint8_t readDipAddressLowNibble() {
  const uint8_t b0 = (digitalRead(DIP_PIN_0) == HIGH) ? 1 : 0;
  const uint8_t b1 = (digitalRead(DIP_PIN_1) == HIGH) ? 1 : 0;
  const uint8_t b2 = (digitalRead(DIP_PIN_2) == HIGH) ? 1 : 0;
  const uint8_t b3 = (digitalRead(DIP_PIN_3) == HIGH) ? 1 : 0;
  return (uint8_t)(b0 | (b1 << 1) | (b2 << 2) | (b3 << 3));
}

void setMotorEnabled(bool enable) {
  motorEnabled = enable;
  digitalWrite(PIN_EN, enable ? HIGH : LOW);
}

void onReceive(int numBytes) {
  if (numBytes < 1) {
    return;
  }

  currentRegister = Wire.read();
  numBytes--;

  if (currentRegister == REG_ENABLE && numBytes >= 1 && Wire.available()) {
    const uint8_t value = Wire.read();
    setMotorEnabled(value != 0);
  }
}

void onRequest() {
  if (currentRegister == REG_ENABLE) {
    Wire.write(motorEnabled ? 0x01 : 0x00);
    return;
  }

  Wire.write(0xFF);
}

void setup() {
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_OPTO, OUTPUT);
  pinMode(PIN_PUL, OUTPUT);

  pinMode(DIP_PIN_0, INPUT_PULLUP);
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_3, INPUT_PULLUP);

  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_PUL, LOW);
  digitalWrite(PIN_OPTO, HIGH);
  setMotorEnabled(false);

  const uint8_t lowNibble = readDipAddressLowNibble();
  slaveAddress = (uint8_t)(lowNibble & 0x0F);

  Wire.begin(slaveAddress);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  Serial.begin(115200);
  Serial.println(F("DM320T slave ready"));
  Serial.print(F("I2C address: 0x"));
  if (slaveAddress < 0x10) {
    Serial.print('0');
  }
  Serial.println(slaveAddress, HEX);
}

void loop() {
  // No background work yet.
}
