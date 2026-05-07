/**
 * Simple DM320T test without I2C
 * Arduino Nano R4
 *
 * Pins:
 *   ENA : D6
 *   DIR : D7
 *   PUL : D9
 *
 * Wiring to DM320T P1:
 *   Nano 5V  -> OPTO
 *   Nano GND -> GND
 *   D9       -> PUL
 *   D7       -> DIR
 *   D6       -> ENA
 */

const uint8_t PIN_EN  = 6;
const uint8_t PIN_DIR = 8;
const uint8_t PIN_PUL = 9;

const long TEST_STEPS = 2000;

const uint32_t PULSE_US    = 20;   // µs HIGH / LOW per halve puls (min. 7.5 µs voor DM320T)
const uint32_t STEP_GAP_US = 20; // µs tussen stappen (= 5 ms)

bool testDone = false;

inline void waitMicros(uint32_t us) {
  uint32_t start = micros();
  while ((micros() - start) < us) { }
}

void stepOnce() {
  digitalWrite(PIN_PUL, HIGH);
  waitMicros(PULSE_US);
  digitalWrite(PIN_PUL, LOW);
  waitMicros(PULSE_US);
}

void setup() {
  pinMode(PIN_EN,  OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_PUL, OUTPUT);

  Serial.begin(115200);
  while (!Serial) { }

  digitalWrite(PIN_PUL, LOW);
  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_EN,  HIGH);

  Serial.println("DM320T test start");
}

void loop() {
  digitalWrite(PIN_PUL, HIGH);
  delay(1.25);
  digitalWrite(PIN_PUL, LOW);
  delay(1.25);
}