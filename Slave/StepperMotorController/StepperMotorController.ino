/**
 * StepperMotorController.ino
 *
 * Arduino Nano R4 I2C slave for a DM320T stepper driver.
 *
 * Pin mapping (as requested)
 * --------------------------
 * SDA  : 18
 * SCL  : 19
 * EN   : 6
 * DIR  : 7
 * OPTO : 8
 * PUL  : 9
 *
 * DIP switches (pins 2..5)
 * ------------------------
 * The 7-bit slave address is built as: 000 + dip[3:0]
 * bit0 -> pin 2, bit1 -> pin 3, bit2 -> pin 4, bit3 -> pin 5
 *
 * Registers
 * ---------
 * 0x00 (R/W): ENABLE
 *      write 0x01 -> enable driver
 *      write 0x00 -> disable driver
 *      read       -> 0x01 enabled, 0x00 disabled
 */

#include <Wire.h>

// ─── Pin definitions ───────────────────────────────────────────────────────
const uint8_t PIN_EN   = 6;
const uint8_t PIN_DIR  = 7;
const uint8_t PIN_OPTO = 8;
const uint8_t PIN_PUL  = 9;

// DIP switches voor low 4 adres bits
const uint8_t DIP_PIN_0 = 2;
const uint8_t DIP_PIN_1 = 3;
const uint8_t DIP_PIN_2 = 4;
const uint8_t DIP_PIN_3 = 5;

// ─── Register map ──────────────────────────────────────────────────────────
const uint8_t REG_ENABLE = 0x00;

// Huidige driver state
volatile uint8_t currentRegister = REG_ENABLE;
volatile bool motorEnabled = false;

// ─── Debug flags ───────────────────────────────────────────────────────────
volatile bool rxEventPending = false;
volatile bool txEventPending = false;

volatile int    lastNumBytes = 0;
volatile uint8_t lastReceivedRegister = 0xFF;
volatile uint8_t lastReceivedValue = 0xFF;
volatile bool    lastEnableState = false;
volatile bool    rxOverflow = false;

// ─── DIP-Adres lezen───────────────────────────────────────────────────────
uint8_t readDipAddressLowNibble() {
  const uint8_t b0 = (digitalRead(DIP_PIN_0) == HIGH) ? 1 : 0;
  const uint8_t b1 = (digitalRead(DIP_PIN_1) == HIGH) ? 1 : 0;
  const uint8_t b2 = (digitalRead(DIP_PIN_2) == HIGH) ? 1 : 0;
  const uint8_t b3 = (digitalRead(DIP_PIN_3) == HIGH) ? 1 : 0;

  return (uint8_t)(b0 | (b1 << 1) | (b2 << 2) | (b3 << 3));
}

// ─── DM320T control ───────────────────────────────────────────────────────
void setMotorEnabled(bool enable) {
  motorEnabled = enable;

  // Pins voor DM320T (EN, DIR, PUL, OPTO)
  digitalWrite(PIN_EN, enable ? HIGH : LOW);
  // Opto (logica’side) blijft HIGH, zolang ENLOW/DIR/PUL goed gestuurd worden.
}

// ─── I2C callbacks ─────────────────────────────────────────────────────────
void onReceive(int numBytes) {
  if (numBytes < 1) return;

  lastNumBytes = numBytes;

  currentRegister = Wire.read();
  lastReceivedRegister = currentRegister;
  numBytes--;

  uint8_t receivedValue = 0xFF;
  bool hasValue = false;

  if (numBytes >= 1 && Wire.available()) {
    receivedValue = Wire.read();
    hasValue = true;
    lastReceivedValue = receivedValue;
  }

  // Lees eventuele extra bytes weg
  while (Wire.available()) {
    Wire.read();
    rxOverflow = true;
  }

  // Enkel REG_ENABLE verwerken
  if (currentRegister == REG_ENABLE && hasValue) {
    setMotorEnabled(receivedValue != 0);
    lastEnableState = motorEnabled;
  }

  rxEventPending = true;
}

void onRequest() {
  if (currentRegister == REG_ENABLE) {
    Wire.write(motorEnabled ? 0x01 : 0x00);
  } else {
    Wire.write(0xFF);
  }

  txEventPending = true;
}

// ─── setup()───────────────────────────────────────────────────────────────
void setup() {
  // Init DM320T pins
  pinMode(PIN_EN,   OUTPUT);
  pinMode(PIN_DIR,  OUTPUT);
  pinMode(PIN_OPTO, OUTPUT);
  pinMode(PIN_PUL,  OUTPUT);

  // Init DIP pins (HIGH = logische 0 op switch)
  pinMode(DIP_PIN_0, INPUT_PULLUP);
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_3, INPUT_PULLUP);

  // Default motor toestand
  digitalWrite(PIN_DIR,  LOW);
  digitalWrite(PIN_PUL,  LOW);
  digitalWrite(PIN_OPTO, HIGH);
  setMotorEnabled(false);

  // Lees 4 bits DIP en bereken slave adres
  const uint8_t lowNibble = readDipAddressLowNibble();

  // Vermijd adres 0x00 (niet toegewezen in I2C)
  uint8_t slaveAddress = (lowNibble & 0x0F);

  // Serial monitor
  Serial.begin(115200);
  while (!Serial); // Rustig starten na herstart USB (R4)

  Serial.println(F("DM320T slave ready"));
  Serial.print(F("I2C address: 0x"));
  if (slaveAddress < 0x10) Serial.print('0');
  Serial.println(slaveAddress, BIN);

  // Start I2C slave
  Wire.begin(slaveAddress);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
}

// ─── loop()────────────────────────────────────────────────────────────────
void loop() {
  // RX-events afhandelen
  if (rxEventPending) {
    noInterrupts();
    const int numBytesCopy = lastNumBytes;
    const uint8_t regCopy = lastReceivedRegister;
    const uint8_t valueCopy = lastReceivedValue;
    const bool enabledCopy = lastEnableState;
    const bool overflowCopy = rxOverflow;
    rxEventPending = false;
    rxOverflow = false;
    interrupts();

    Serial.print(F("[RX] bytes="));
    Serial.print(numBytesCopy);
    Serial.print(F(" reg=0x"));
    if (regCopy < 0x10) Serial.print('0');
    Serial.print(regCopy, HEX);
    Serial.print(F(" value=0x"));
    if (valueCopy < 0x10) Serial.print('0');
    Serial.print(valueCopy, HEX);
    Serial.print(F(" motorEnabled="));
    Serial.println(enabledCopy ? F("true") : F("false"));

    if (overflowCopy) {
      Serial.println(F("[WARN] Extra bytes ontvangen en weggegooid."));
    }
  }

  // TX-events afhandelen
  if (txEventPending) {
    noInterrupts();
    const uint8_t regCopy = currentRegister;
    const bool enabledCopy = motorEnabled;
    txEventPending = false;
    interrupts();

    Serial.print(F("[TX] read request for reg=0x"));
    if (regCopy < 0x10) Serial.print('0');
    Serial.print(regCopy, HEX);
    Serial.print(F(" -> returned "));
    Serial.println(enabledCopy ? F("0x01") : F("0x00"));
  }

  delay(10);
}