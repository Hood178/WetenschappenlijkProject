/**
 * StepperMotorController.ino
 *
 * Arduino Nano R4 I2C slave for a DM320T stepper driver.
 *
 * Pin mapping
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
 * The 7-bit slave address is built from the 4 DIP bits:
 * bit0 -> pin 2, bit1 -> pin 3, bit2 -> pin 4, bit3 -> pin 5
 *
 * Registers
 * ---------
 * 0x00 (R/W): ENABLE
 *      write 0x01 -> enable driver
 *      write 0x00 -> disable driver
 *      read       -> 0x01 enabled, 0x00 disabled
 *
 * 0x01 (R/W): DIR
 *      write 0x00 -> DIR low
 *      write 0x01 -> DIR high
 *      read       -> current DIR value (0x00 or 0x01)
 *
 * 0x02 (R/W): PERIOD_US_H  (high byte of step period in microseconds)
 * 0x03 (R/W): PERIOD_US_L  (low  byte of step period in microseconds)
 *      Full period_us = (PERIOD_US_H << 8) | PERIOD_US_L
 *      Intern wordt een 50/50 duty gebruikt, dus high = low = period_us / 2.
 *
 * 0x04 (R/W): PULSE_COUNT_H
 * 0x05 (R/W): PULSE_COUNT_L
 *      pulse_count = (PULSE_COUNT_H << 8) | PULSE_COUNT_L
 *      pulse_count == 0 -> continue mode (blijft pulsen zolang ENABLE == 1)
 *      pulse_count > 0  -> eindige beweging met precies pulse_count pulsen
 *
 * 0x06 (R/O): MOTION_COMPLETE_FLAG
 *      0 -> motion actief
 *      1 -> motion klaar / idle
 *
 * 0x07..0x0A (R/W): POSITION_PULSES
 *      32-bit signed positie in pulsen (big-endian)
 */

#include <Wire.h>

// ─── Pin definitions ───────────────────────────────────────────────────────
const uint8_t PIN_EN   = 6;
const uint8_t PIN_DIR  = 8;
const uint8_t PIN_PUL  = 9;

// DIP switches voor low 4 adres bits
const uint8_t DIP_PIN_0 = 2;
const uint8_t DIP_PIN_1 = 3;
const uint8_t DIP_PIN_2 = 4;
const uint8_t DIP_PIN_3 = 5;

// ─── Register map ──────────────────────────────────────────────────────────
const uint8_t REG_ENABLE      = 0x00;
const uint8_t REG_DIR         = 0x01;
const uint8_t REG_PERIOD_US_H = 0x02;
const uint8_t REG_PERIOD_US_L = 0x03;
const uint8_t REG_PCOUNT_H    = 0x04;
const uint8_t REG_PCOUNT_L    = 0x05;
const uint8_t REG_MOTION_COMPLETE_FLAG = 0x06;
const uint8_t REG_POS_HH      = 0x07;
const uint8_t REG_POS_HL      = 0x08;
const uint8_t REG_POS_LH      = 0x09;
const uint8_t REG_POS_LL      = 0x0A;

// ─── Huidige register state (ISR-geschreven) ──────────────────────────────
volatile uint8_t  currentRegister = REG_ENABLE;

volatile bool     regEnable       = false;   // ENABLE
volatile uint8_t  regDir          = 0;       // DIR (0/1)
volatile uint16_t regPeriodUs     = 200;     // PERIOD_US (default 200 µs)
volatile uint16_t regPulseCount   = 0;       // PULSE_COUNT (0 = continue)
volatile bool     regMotionComplete = true;   // 1 = idle/complete, 0 = active
volatile int32_t  regPositionPulses = 0;      // 32-bit signed pulse counter

// ─── Motion state (alleen in loop() gebruikt) ─────────────────────────────
bool     motionActive     = false;   // true = we zijn bezig met pulsen
bool     continuousMode   = false;   // true = pulseCount == 0
uint16_t pulsesDone       = 0;       // aantal voltooide pulsen
bool     pulseHighState   = false;   // huidige staat van PUL
uint32_t lastToggleMicros = 0;       // tijdstip laatste toggling

inline uint32_t positionAsU32() {
  return (uint32_t)regPositionPulses;
}

inline void setPositionFromU32(uint32_t value) {
  regPositionPulses = (int32_t)value;
}

// ─── Debug flags ───────────────────────────────────────────────────────────
volatile bool rxEventPending = false;
volatile bool txEventPending = false;

volatile int     lastNumBytes         = 0;
volatile uint8_t lastReceivedRegister = 0xFF;
volatile uint8_t lastReceivedValue    = 0xFF;
volatile bool    rxOverflow           = false;

// ─── DIP-Adres lezen───────────────────────────────────────────────────────
uint8_t readDipAddressLowNibble() {
  const uint8_t b0 = (digitalRead(DIP_PIN_0) == HIGH) ? 1 : 0;
  const uint8_t b1 = (digitalRead(DIP_PIN_1) == HIGH) ? 1 : 0;
  const uint8_t b2 = (digitalRead(DIP_PIN_2) == HIGH) ? 1 : 0;
  const uint8_t b3 = (digitalRead(DIP_PIN_3) == HIGH) ? 1 : 0;

  return (uint8_t)(b0 | (b1 << 1) | (b2 << 2) | (b3 << 3));
}

// ─── DM320T control ───────────────────────────────────────────────────────
void applyEnablePin(bool enable) {
  regEnable = enable;
  digitalWrite(PIN_EN, enable ? HIGH : LOW);
}

void applyDirPin(uint8_t dir) {
  regDir = dir ? 1 : 0;
  digitalWrite(PIN_DIR, regDir ? HIGH : LOW);
}

void applyPositionByte(uint8_t reg, uint8_t value) {
  uint32_t current = positionAsU32();

  switch (reg) {
    case REG_POS_HH:
      current &= 0x00FFFFFFUL;
      current |= ((uint32_t)value << 24);
      break;
    case REG_POS_HL:
      current &= 0xFF00FFFFUL;
      current |= ((uint32_t)value << 16);
      break;
    case REG_POS_LH:
      current &= 0xFFFF00FFUL;
      current |= ((uint32_t)value << 8);
      break;
    case REG_POS_LL:
      current &= 0xFFFFFF00UL;
      current |= value;
      break;
    default:
      return;
  }

  setPositionFromU32(current);
}

uint8_t readPositionByte(uint8_t reg) {
  uint32_t value = positionAsU32();

  switch (reg) {
    case REG_POS_HH:
      return (uint8_t)(value >> 24);
    case REG_POS_HL:
      return (uint8_t)(value >> 16);
    case REG_POS_LH:
      return (uint8_t)(value >> 8);
    case REG_POS_LL:
      return (uint8_t)(value);
    default:
      return 0xFF;
  }
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

  // Register schrijven
  if (hasValue) {
    switch (currentRegister) {
      case REG_ENABLE:
        applyEnablePin(receivedValue != 0);
        break;

      case REG_DIR:
        applyDirPin(receivedValue & 0x01);
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
        // Read-only flag; writes are ignored.
        break;

      case REG_POS_HH:
      case REG_POS_HL:
      case REG_POS_LH:
      case REG_POS_LL:
        applyPositionByte(currentRegister, receivedValue);
        break;

      default:
        // Onbekend register -> negeren
        break;
    }
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
    case REG_POS_HH:
    case REG_POS_HL:
    case REG_POS_LH:
    case REG_POS_LL:
      outVal = readPositionByte(currentRegister);
      break;
    default:
      outVal = 0xFF;
      break;
  }

  Wire.write(outVal);
  txEventPending = true;
}

// ─── Motion helpers ───────────────────────────────────────────────────────
void resetMotionState() {
  motionActive     = false;
  continuousMode   = false;
  pulsesDone       = 0;
  pulseHighState   = false;
  lastToggleMicros = micros();
  regMotionComplete = true;
  digitalWrite(PIN_PUL, LOW);
}

// ─── setup()───────────────────────────────────────────────────────────────
void setup() {
  // Init DM320T pins
  pinMode(PIN_EN,   OUTPUT);
  pinMode(PIN_DIR,  OUTPUT);
  pinMode(PIN_PUL,  OUTPUT);

  // Init DIP pins (HIGH = logische 1 via pullup)
  pinMode(DIP_PIN_0, INPUT_PULLUP);
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_3, INPUT_PULLUP);

  // Default motor toestand
  digitalWrite(PIN_DIR,  LOW);
  digitalWrite(PIN_PUL,  LOW);
  applyEnablePin(false);

  resetMotionState();

  // Lees 4 bits DIP en bereken slave adres (0..15)
  const uint8_t lowNibble = readDipAddressLowNibble();
  uint8_t slaveAddress = (lowNibble & 0x0F);
  if (slaveAddress == 0) {
    // Vermijd 0 als adres; schuif desnoods naar 1
    slaveAddress = 1;
  }

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

// ─── loop()────────────────────────────────────────────────────────────────
void loop() {
  // Kopieën van registers (zonder lang interrupts te blokkeren)
  bool     enCopy;
  uint8_t  dirCopy;
  uint16_t periodCopy;
  uint16_t pcountCopy;

  noInterrupts();
  enCopy      = regEnable;
  dirCopy     = regDir;
  periodCopy  = regPeriodUs;
  pcountCopy  = regPulseCount;
  interrupts();

  // EN en DIR doorgeven aan driver
  digitalWrite(PIN_EN,  enCopy ? HIGH : LOW);
  digitalWrite(PIN_DIR, dirCopy ? HIGH : LOW);

  // Minimale veilige periode ivm DM320T (min pulse width 7.5 µs) [web:1][web:4]
  if (periodCopy < 20) {
    periodCopy = 20; // 20 µs -> 10/10 high/low
  }
  uint16_t halfPeriod = periodCopy / 2;

  // Motion-state bepalen op basis van ENABLE en PULSE_COUNT
  if (!enCopy || periodCopy == 0) {
    // Motor uit of periode 0 -> geen motion
    resetMotionState();
  } else {
    if (!motionActive) {
      // Start nieuwe motion
      motionActive   = true;
      pulsesDone     = 0;
      continuousMode = (pcountCopy == 0);
      pulseHighState = false;
      lastToggleMicros = micros();
      regMotionComplete = false;
      digitalWrite(PIN_PUL, LOW);
    }
  }

  // Puls-generator
  if (motionActive && enCopy) {
    uint32_t now = micros();

    if (!pulseHighState) {
      // PUL is low -> naar high na halfPeriod
      if ((uint32_t)(now - lastToggleMicros) >= halfPeriod) {
        digitalWrite(PIN_PUL, HIGH);
        pulseHighState   = true;
        lastToggleMicros = now;
      }
    } else {
      // PUL is high -> naar low na halfPeriod
      if ((uint32_t)(now - lastToggleMicros) >= halfPeriod) {
        digitalWrite(PIN_PUL, LOW);
        pulseHighState   = false;
        lastToggleMicros = now;

        // Eén volledige puls afgewerkt
        regPositionPulses += (regDir ? 1 : -1);
        if (!continuousMode) {
          pulsesDone++;
          if (pulsesDone >= pcountCopy) {
            // Eindige beweging af, maar ENABLE zelf niet forceren
            resetMotionState();
          }
        }
      }
    }
  }

  // Debug RX
  if (rxEventPending) {
    noInterrupts();
    const int     numBytesCopy = lastNumBytes;
    const uint8_t regCopy      = lastReceivedRegister;
    const uint8_t valueCopy    = lastReceivedValue;
    const bool    overflowCopy = rxOverflow;
    rxEventPending = false;
    rxOverflow     = false;
    interrupts();

    Serial.print(F("[RX] bytes="));
    Serial.print(numBytesCopy);
    Serial.print(F(" reg=0x"));
    if (regCopy < 0x10) Serial.print('0');
    Serial.print(regCopy, HEX);
    Serial.print(F(" value=0x"));
    if (valueCopy < 0x10) Serial.print('0');
    Serial.print(valueCopy, HEX);
    Serial.print(F(" ENABLE="));
    Serial.print(enCopy ? F("1") : F("0"));
    Serial.print(F(" DIR="));
    Serial.println(dirCopy ? F("1") : F("0"));

    if (overflowCopy) {
      Serial.println(F("[WARN] Extra bytes ontvangen en weggegooid."));
    }
  }

  // Debug TX
  if (txEventPending) {
    noInterrupts();
    const uint8_t regCopy   = currentRegister;
    const bool    enDbg     = regEnable;
    const uint8_t dirDbg    = regDir;
    const uint16_t perDbg   = regPeriodUs;
    const uint16_t cntDbg   = regPulseCount;
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
    Serial.println(cntDbg);
  }
}