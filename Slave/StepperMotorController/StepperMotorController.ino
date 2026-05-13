/**
 * StepperMotorController.ino
 *
 * Arduino Nano R4 I2C slave for a DM320T stepper driver.
 *
 * Pin mapping
 * -----------
 * SDA : 18
 * SCL : 19
 * EN  : 6
 * DIR : 7
 * PUL : 9
 *
 * DIP switches (pins 2..5)
 * ------------------------
 * The 7-bit slave address is built from the 4 DIP bits:
 * bit0 -> pin 2, bit1 -> pin 3, bit2 -> pin 4, bit3 -> pin 5
 *
 * Registers
 * ---------
 * 0x00 (R/W): ENABLE
 *      write 0x01 -> enable + start motion using current registers
 *      write 0x00 -> disable driver + stop motion
 *      read       -> 0x01 enabled, 0x00 disabled
 *
 * 0x01 (R/W): DIR
 *      write 0x00 -> DIR low
 *      write 0x01 -> DIR high
 *      read       -> current DIR value
 *
 * 0x02 (R/W): PERIOD_US_H
 * 0x03 (R/W): PERIOD_US_L
 *      Full period_us = (H << 8) | L
 *
 * 0x04 (R/W): PULSE_COUNT_H
 * 0x05 (R/W): PULSE_COUNT_L
 *      pulse_count == 0 -> continue mode
 *      pulse_count > 0  -> finite move
 *
 * 0x06 (R/O): MOTION_COMPLETE_FLAG
 *      0 -> motion active
 *      1 -> motion complete / idle
 */

#include <Wire.h>

// ─── Pin definitions ───────────────────────────────────────────────────────
const uint8_t PIN_EN  = 6;
const uint8_t PIN_DIR = 7;
const uint8_t PIN_PUL = 9;

// DIP switches
const uint8_t DIP_PIN_0 = 2;
const uint8_t DIP_PIN_1 = 3;
const uint8_t DIP_PIN_2 = 4;
const uint8_t DIP_PIN_3 = 5;

// ─── Register map ──────────────────────────────────────────────────────────
const uint8_t REG_ENABLE                = 0x00;
const uint8_t REG_DIR                   = 0x01;
const uint8_t REG_PERIOD_US_H           = 0x02;
const uint8_t REG_PERIOD_US_L           = 0x03;
const uint8_t REG_PCOUNT_H              = 0x04;
const uint8_t REG_PCOUNT_L              = 0x05;
const uint8_t REG_MOTION_COMPLETE_FLAG  = 0x06;

// ─── Register state ────────────────────────────────────────────────────────
volatile uint8_t  currentRegister      = REG_ENABLE;
volatile bool     regEnable            = false;
volatile uint8_t  regDir               = 0;
volatile uint16_t regPeriodUs          = 200;
volatile uint16_t regPulseCount        = 0;
volatile bool     regMotionComplete    = true;
volatile bool     motionStartPending   = false;

// trigger om nieuwe motion te starten vanuit loop()
volatile bool startMotionRequest = false;
volatile bool stopMotionRequest  = false;

// ─── Motion state (alleen loop) ────────────────────────────────────────────
bool     motionActive       = false;
bool     continuousMode     = false;
uint16_t pulsesTarget       = 0;
uint16_t pulsesDone         = 0;
bool     pulseHighState     = false;
uint32_t lastToggleMicros   = 0;

// ─── Debug flags ───────────────────────────────────────────────────────────
const bool ENABLE_DEBUG_LOGS = false;
volatile bool rxEventPending = false;
volatile bool txEventPending = false;

volatile int     lastNumBytes         = 0;
volatile uint8_t lastReceivedRegister = 0xFF;
volatile uint8_t lastReceivedValue    = 0xFF;
volatile bool    lastReceivedHasValue = false;
volatile bool    rxOverflow           = false;

// ─── Helpers ───────────────────────────────────────────────────────────────
uint8_t readDipAddressLowNibble() {
  const uint8_t b0 = (digitalRead(DIP_PIN_0) == HIGH) ? 1 : 0;
  const uint8_t b1 = (digitalRead(DIP_PIN_1) == HIGH) ? 1 : 0;
  const uint8_t b2 = (digitalRead(DIP_PIN_2) == HIGH) ? 1 : 0;
  const uint8_t b3 = (digitalRead(DIP_PIN_3) == HIGH) ? 1 : 0;
  return (uint8_t)(b0 | (b1 << 1) | (b2 << 2) | (b3 << 3));
}

void applyEnablePin(bool enable) {
  regEnable = enable;
  digitalWrite(PIN_EN, enable ? HIGH : LOW);
}

void applyDirPin(uint8_t dir) {
  regDir = (dir ? 1 : 0);
  digitalWrite(PIN_DIR, regDir ? HIGH : LOW);
}

void stopMotionInternal(bool setCompleteFlag) {
  motionActive     = false;
  continuousMode   = false;
  pulsesTarget     = 0;
  pulsesDone       = 0;
  pulseHighState   = false;
  motionStartPending = false;
  lastToggleMicros = micros();
  digitalWrite(PIN_PUL, LOW);

  if (setCompleteFlag) {
    regMotionComplete = true;
  }
}

void beginMotionInternal(uint16_t pulseCount) {
  motionActive      = true;
  motionStartPending = false;
  continuousMode    = (pulseCount == 0);
  pulsesTarget      = pulseCount;
  pulsesDone        = 0;
  pulseHighState    = false;
  lastToggleMicros  = micros();
  regMotionComplete = false;
  digitalWrite(PIN_PUL, LOW);
}

// ─── I2C callbacks ─────────────────────────────────────────────────────────
void onReceive(int numBytes) {
  if (numBytes < 1) return;

  lastNumBytes = numBytes;
  rxOverflow = false;
  lastReceivedHasValue = false;
  lastReceivedValue = 0xFF;

  currentRegister = Wire.read();
  lastReceivedRegister = currentRegister;
  numBytes--;

  if (numBytes == 0) {
    // Alleen register pointer zetten voor volgende read
    rxEventPending = true;
    return;
  }

  uint8_t receivedValue = 0xFF;
  if (Wire.available()) {
    receivedValue = Wire.read();
    lastReceivedValue = receivedValue;
    lastReceivedHasValue = true;
  }

  while (Wire.available()) {
    Wire.read();
    rxOverflow = true;
  }

  switch (currentRegister) {
    case REG_ENABLE:
      if (receivedValue != 0) {
        regEnable = true;
        motionStartPending = true;
        regMotionComplete = false;
        stopMotionRequest = false;
        startMotionRequest = true;   // start nieuwe motion in loop()
      } else {
        regEnable = false;
        motionStartPending = false;
        stopMotionRequest = true;
        regMotionComplete = true;
      }
      break;

    case REG_DIR:
      regDir = (receivedValue & 0x01);
      break;

    case REG_PERIOD_US_H: {
      uint16_t tmp = regPeriodUs;
      tmp &= 0x00FF;
      tmp |= ((uint16_t)receivedValue << 8);
      regPeriodUs = tmp;
      break;
    }

    case REG_PERIOD_US_L: {
      uint16_t tmp = regPeriodUs;
      tmp &= 0xFF00;
      tmp |= receivedValue;
      regPeriodUs = tmp;
      break;
    }

    case REG_PCOUNT_H: {
      uint16_t tmp = regPulseCount;
      tmp &= 0x00FF;
      tmp |= ((uint16_t)receivedValue << 8);
      regPulseCount = tmp;
      break;
    }

    case REG_PCOUNT_L: {
      uint16_t tmp = regPulseCount;
      tmp &= 0xFF00;
      tmp |= receivedValue;
      regPulseCount = tmp;
      break;
    }

    case REG_MOTION_COMPLETE_FLAG:
      // read-only
      break;

    default:
      break;
  }

  rxEventPending = true;
}

void onRequest() {
  uint8_t outVal = 0xFF;

  switch (currentRegister) {
    case REG_ENABLE:
      outVal = regEnable ? 0x01 : 0x00;
      break;
    case REG_DIR:
      outVal = regDir ? 0x01 : 0x00;
      break;
    case REG_PERIOD_US_H:
      outVal = (uint8_t)(regPeriodUs >> 8);
      break;
    case REG_PERIOD_US_L:
      outVal = (uint8_t)(regPeriodUs & 0xFF);
      break;
    case REG_PCOUNT_H:
      outVal = (uint8_t)(regPulseCount >> 8);
      break;
    case REG_PCOUNT_L:
      outVal = (uint8_t)(regPulseCount & 0xFF);
      break;
    case REG_MOTION_COMPLETE_FLAG:
      outVal = regMotionComplete ? 0x01 : 0x00;
      break;
    default:
      outVal = 0xFF;
      break;
  }

  Wire.write(outVal);
  txEventPending = true;
}

// ─── setup() ───────────────────────────────────────────────────────────────
void setup() {
  pinMode(PIN_EN,  OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PUL, OUTPUT);

  pinMode(DIP_PIN_0, INPUT_PULLUP);
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_3, INPUT_PULLUP);

  digitalWrite(PIN_PUL, LOW);
  applyDirPin(0);
  applyEnablePin(false);
  stopMotionInternal(true);

  const uint8_t lowNibble = readDipAddressLowNibble();
  uint8_t slaveAddress = (lowNibble & 0x0F);
  if (slaveAddress == 0) slaveAddress = 1;

  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F("DM320T stepper slave ready"));
  Serial.print(F("I2C address: 0x"));
  if (slaveAddress < 0x10) Serial.print('0');
  Serial.println(slaveAddress, HEX);

  Wire.begin(slaveAddress);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
}

// ─── loop() ────────────────────────────────────────────────────────────────
void loop() {
  bool     enCopy;
  uint8_t  dirCopy;
  uint16_t periodCopy;
  uint16_t pcountCopy;
  bool     startReqCopy;
  bool     stopReqCopy;

  noInterrupts();
  enCopy       = regEnable;
  dirCopy      = regDir;
  periodCopy   = regPeriodUs;
  pcountCopy   = regPulseCount;
  startReqCopy = startMotionRequest;
  stopReqCopy  = stopMotionRequest;
  if (startMotionRequest) {
    startMotionRequest = false;
  }
  if (stopMotionRequest) {
    stopMotionRequest = false;
  }
  interrupts();

  digitalWrite(PIN_EN,  enCopy ? HIGH : LOW);
  digitalWrite(PIN_DIR, dirCopy ? HIGH : LOW);

  if (periodCopy < 20) {
    periodCopy = 20;
  }
  uint16_t halfPeriod = (periodCopy / 2);
  if (halfPeriod < 10) {
    halfPeriod = 10;
  }

  if (stopReqCopy) {
    stopMotionInternal(true);
  }

  // Start enkel op expliciete enable=1 write
  if (!stopReqCopy && startReqCopy && enCopy) {
    beginMotionInternal(pcountCopy);
  }

  // Als disabled -> zeker stoppen
  if (!enCopy && motionActive) {
    stopMotionInternal(true);
  }

  // Puls-generator
  if (motionActive && enCopy) {
    uint32_t now = micros();

    if (!pulseHighState) {
      if ((uint32_t)(now - lastToggleMicros) >= halfPeriod) {
        digitalWrite(PIN_PUL, HIGH);
        pulseHighState   = true;
        lastToggleMicros = now;
      }
    } else {
      if ((uint32_t)(now - lastToggleMicros) >= halfPeriod) {
        digitalWrite(PIN_PUL, LOW);
        pulseHighState   = false;
        lastToggleMicros = now;

        if (!continuousMode) {
          pulsesDone++;
          if (pulsesDone >= pulsesTarget) {
            stopMotionInternal(true);
          }
        }
      }
    }
  }

  // Update motioncomplete flag:
  // - false zolang motion actief is of nog wacht op opstarten na enable
  // - true in alle andere gevallen
  noInterrupts();
  regMotionComplete = !(motionStartPending || (motionActive && enCopy));
  interrupts();

  // Debug RX
  if (ENABLE_DEBUG_LOGS && rxEventPending) {
    int     numBytesCopy;
    uint8_t regCopy;
    uint8_t valueCopy;
    bool    hasValueCopy;
    bool    overflowCopy;
    bool    enableDbg;
    uint8_t dirDbg;
    uint16_t perDbg;
    uint16_t cntDbg;
    bool    motionDbg;

    noInterrupts();
    numBytesCopy = lastNumBytes;
    regCopy      = lastReceivedRegister;
    valueCopy    = lastReceivedValue;
    hasValueCopy = lastReceivedHasValue;
    overflowCopy = rxOverflow;
    enableDbg    = regEnable;
    dirDbg       = regDir;
    perDbg       = regPeriodUs;
    cntDbg       = regPulseCount;
    motionDbg    = regMotionComplete;
    rxEventPending = false;
    rxOverflow = false;
    interrupts();

    Serial.print(F("[RX] bytes="));
    Serial.print(numBytesCopy);
    Serial.print(F(" reg=0x"));
    if (regCopy < 0x10) Serial.print('0');
    Serial.print(regCopy, HEX);

    if (hasValueCopy) {
      Serial.print(F(" value=0x"));
      if (valueCopy < 0x10) Serial.print('0');
      Serial.print(valueCopy, HEX);
    } else {
      Serial.print(F(" value=--"));
    }

    Serial.print(F(" ENABLE="));
    Serial.print(enableDbg ? F("1") : F("0"));
    Serial.print(F(" DIR="));
    Serial.print(dirDbg ? F("1") : F("0"));
    Serial.print(F(" periodUs="));
    Serial.print(perDbg);
    Serial.print(F(" pulseCount="));
    Serial.print(cntDbg);
    Serial.print(F(" motionComplete="));
    Serial.println(motionDbg ? F("1") : F("0"));

    if (overflowCopy) {
      Serial.println(F("[WARN] Extra bytes ontvangen en weggegooid."));
    }
  }

  // Debug TX
  if (ENABLE_DEBUG_LOGS && txEventPending) {
    uint8_t  regCopy;
    bool     enDbg;
    uint8_t  dirDbg;
    uint16_t perDbg;
    uint16_t cntDbg;
    bool     motionDbg;

    noInterrupts();
    regCopy = currentRegister;
    enDbg   = regEnable;
    dirDbg  = regDir;
    perDbg  = regPeriodUs;
    cntDbg  = regPulseCount;
    motionDbg = regMotionComplete;
    txEventPending = false;
    interrupts();

    Serial.print(F("[TX] read reg=0x"));
    if (regCopy < 0x10) Serial.print('0');
    Serial.print(regCopy, HEX);
    Serial.print(F(" ENABLE="));
    Serial.print(enDbg ? F("1") : F("0"));
    Serial.print(F(" DIR="));
    Serial.print(dirDbg ? F("1") : F("0"));
    Serial.print(F(" periodUs="));
    Serial.print(perDbg);
    Serial.print(F(" pulseCount="));
    Serial.print(cntDbg);
    Serial.print(F(" motionComplete="));
    Serial.println(motionDbg ? F("1") : F("0"));
  }
}