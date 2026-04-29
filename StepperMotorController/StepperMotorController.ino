/**
 * StepperMotorController.ino
 *
 * Arduino Nano R4 – I2C (SMBus) controlled stepper motor driver
 *
 * Hardware assumptions
 * --------------------
 *  - Stepper driver : A4988 (or compatible DRV8825 / TB6600)
 *  - STEP pin       : D3
 *  - DIR  pin       : D4
 *  - ENABLE pin     : D5  (active-LOW on A4988)
 *  - MS1  pin       : D6
 *  - MS2  pin       : D7
 *  - MS3  pin       : D8
 *  - Motor steps    : 200 steps/rev  (1.8° stepper)
 *
 * I2C (SMBus) interface
 * ---------------------
 *  Default slave address : 0x12  (configurable via I2C_ADDRESS below)
 *
 *  Write protocol  (master → Arduino)
 *  ┌──────────┬──────────────────────────────────────────────────┐
 *  │ Register │ Description                                      │
 *  ├──────────┼──────────────────────────────────────────────────┤
 *  │  0x00    │ ENABLE  – 0x01 enable motor, 0x00 disable motor  │
 *  │  0x01    │ DIRECTION – 0x00 forward, 0x01 reverse           │
 *  │  0x02    │ SET_RPM – 1 byte RPM value (1–255)               │
 *  │  0x03    │ MOVE_STEPS – 2 bytes big-endian step count       │
 *  │  0x04    │ ROTATE_DEG – 2 bytes big-endian degrees          │
 *  │  0x05    │ STOP – any value stops the current move          │
 *  │  0x06    │ SET_MICROSTEP – 1 byte: 1/2/4/8/16              │
 *  └──────────┴──────────────────────────────────────────────────┘
 *
 *  Read protocol  (master reads from Arduino)
 *  ┌──────────┬──────────────────────────────────────────────────┐
 *  │ Register │ Description                                      │
 *  ├──────────┼──────────────────────────────────────────────────┤
 *  │  0x10    │ STATUS  – bit0=busy, bit1=enabled, bit2=direction│
 *  │  0x11    │ CURRENT_RPM – 1 byte                            │
 *  │  0x12    │ STEPS_REMAINING – 2 bytes big-endian            │
 *  └──────────┴──────────────────────────────────────────────────┘
 *
 * Dependencies
 * ------------
 *  StepperDriver library by Laurentiu Badea
 *  Install via Arduino Library Manager: "StepperDriver"
 *  https://github.com/laurb9/StepperDriver
 */

#include <Wire.h>
#include <A4988.h>   // Part of the StepperDriver library

// ─── Pin definitions ────────────────────────────────────────────────────────
#define STEP_PIN      3
#define DIR_PIN       4
#define ENABLE_PIN    5
#define MS1_PIN       6
#define MS2_PIN       7
#define MS3_PIN       8

// ─── Motor parameters ────────────────────────────────────────────────────────
#define MOTOR_STEPS   200        // steps per revolution (1.8° stepper)
#define DEFAULT_RPM   60
#define DEFAULT_MICROSTEP 1

// ─── I2C configuration ───────────────────────────────────────────────────────
#define I2C_ADDRESS   0x12

// ─── Register addresses ──────────────────────────────────────────────────────
// Write registers
#define REG_ENABLE        0x00
#define REG_DIRECTION     0x01
#define REG_SET_RPM       0x02
#define REG_MOVE_STEPS    0x03
#define REG_ROTATE_DEG    0x04
#define REG_STOP          0x05
#define REG_SET_MICROSTEP 0x06
// Read registers
#define REG_STATUS        0x10
#define REG_CURRENT_RPM   0x11
#define REG_STEPS_REMAIN  0x12

// ─── Global state ────────────────────────────────────────────────────────────
A4988 stepper(MOTOR_STEPS, DIR_PIN, STEP_PIN, ENABLE_PIN, MS1_PIN, MS2_PIN, MS3_PIN);

volatile uint8_t  currentRegister  = 0x00;
volatile bool     motorEnabled     = false;
volatile bool     motorDirection   = false;   // false = forward, true = reverse
volatile uint8_t  currentRPM       = DEFAULT_RPM;
volatile uint8_t  currentMicrostep = DEFAULT_MICROSTEP;
volatile bool     stopRequested    = false;
volatile int32_t  stepsRemaining   = 0;
volatile bool     isBusy           = false;

// Receive buffer for multi-byte writes
uint8_t rxBuffer[4];
uint8_t rxCount = 0;

// ─── Helper functions ────────────────────────────────────────────────────────

/** Enable or disable the stepper driver output. */
void setMotorEnabled(bool enable) {
    if (enable) {
        stepper.enable();
        motorEnabled = true;
    } else {
        stepper.disable();
        motorEnabled = false;
    }
}

/** Set motor direction; also updates the state flag. */
void setDirection(bool reverse) {
    motorDirection = reverse;
    // The StepperDriver library uses positive steps for forward,
    // negative for reverse.  We cache the flag and apply it when moving.
}

/** Set motor speed in RPM. */
void setRPM(uint8_t rpm) {
    if (rpm == 0) rpm = 1;   // guard against zero
    currentRPM = rpm;
    stepper.setRPM(rpm);
}

/** Set micro-stepping mode (1, 2, 4, 8 or 16). */
void setMicrostep(uint8_t ms) {
    switch (ms) {
        case 1: case 2: case 4: case 8: case 16:
            currentMicrostep = ms;
            stepper.setMicrostep(ms);
            break;
        default:
            break;   // ignore invalid values
    }
}

/** Move the motor a given number of steps (direction-aware). */
void moveSteps(int16_t steps) {
    stepsRemaining = steps;
    stopRequested  = false;
    isBusy         = true;
    int16_t signedSteps = motorDirection ? -steps : steps;
    stepper.move(signedSteps);
    // stepper.move() is blocking; once it returns the move is complete.
    stepsRemaining = 0;
    isBusy         = false;
}

/** Rotate the motor by a given number of degrees (direction-aware). */
void rotateDegrees(int16_t degrees) {
    stopRequested = false;
    isBusy        = true;
    int16_t signedDeg = motorDirection ? -degrees : degrees;
    stepper.rotate(signedDeg);
    isBusy = false;
}

/** Immediately stop any ongoing movement. */
void stopMotor() {
    stopRequested = true;
    // The StepperDriver move/rotate calls are blocking on AVR; on the R4
    // they are also blocking, so we flag stop and disable the driver to
    // cut the pulse train quickly.
    stepper.disable();
    motorEnabled   = false;
    stepsRemaining = 0;
    isBusy         = false;
}

/** Build the status byte for the read register. */
uint8_t buildStatusByte() {
    uint8_t status = 0;
    if (isBusy)         status |= 0x01;   // bit0 = busy (set during blocking move)
    if (motorEnabled)   status |= 0x02;   // bit1 = enabled
    if (motorDirection) status |= 0x04;   // bit2 = direction
    return status;
}

// ─── I2C callbacks ───────────────────────────────────────────────────────────

/**
 * Called when the I2C master sends data to this device.
 * First byte is the register address; subsequent bytes are the payload.
 */
void onReceive(int numBytes) {
    if (numBytes < 1) return;

    currentRegister = Wire.read();
    rxCount = 0;

    while (Wire.available()) {
        rxBuffer[rxCount++] = Wire.read();
        if (rxCount >= sizeof(rxBuffer)) break;
    }

    // Process write commands that arrive complete in this callback
    switch (currentRegister) {
        case REG_ENABLE:
            if (rxCount >= 1) setMotorEnabled(rxBuffer[0] != 0);
            break;

        case REG_DIRECTION:
            if (rxCount >= 1) setDirection(rxBuffer[0] != 0);
            break;

        case REG_SET_RPM:
            if (rxCount >= 1) setRPM(rxBuffer[0]);
            break;

        case REG_STOP:
            stopMotor();
            break;

        case REG_SET_MICROSTEP:
            if (rxCount >= 1) setMicrostep(rxBuffer[0]);
            break;

        case REG_MOVE_STEPS:
            // Queued in rxBuffer; executed in loop() to avoid blocking ISR
            break;

        case REG_ROTATE_DEG:
            // Queued in rxBuffer; executed in loop() to avoid blocking ISR
            break;

        default:
            break;
    }
}

/**
 * Called when the I2C master requests data from this device.
 * The master must have written the desired read-register address first.
 */
void onRequest() {
    switch (currentRegister) {
        case REG_STATUS:
            Wire.write(buildStatusByte());
            break;

        case REG_CURRENT_RPM:
            Wire.write(currentRPM);
            break;

        case REG_STEPS_REMAIN: {
            int32_t rem = stepsRemaining;
            Wire.write((uint8_t)((rem >> 8) & 0xFF));
            Wire.write((uint8_t)(rem & 0xFF));
            break;
        }

        default:
            Wire.write(0xFF);   // unknown register
            break;
    }
}

// ─── Arduino lifecycle ───────────────────────────────────────────────────────

void setup() {
    // Initialise stepper driver
    stepper.begin(DEFAULT_RPM, DEFAULT_MICROSTEP);
    stepper.disable();   // keep motor de-energised at startup

    // Initialise I2C as slave
    Wire.begin(I2C_ADDRESS);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);

    // Optional: serial debug output (remove for production)
    Serial.begin(115200);
    Serial.println(F("StepperMotorController ready"));
    Serial.print(F("I2C address: 0x"));
    Serial.println(I2C_ADDRESS, HEX);
}

void loop() {
    // Handle queued blocking motor commands outside the ISR context
    if (currentRegister == REG_MOVE_STEPS && rxCount >= 2) {
        uint16_t steps = ((uint16_t)rxBuffer[0] << 8) | rxBuffer[1];
        rxCount = 0;
        currentRegister = 0xFF;   // clear so we don't re-execute
        if (motorEnabled && steps > 0 && !stopRequested) {
            moveSteps((int16_t)steps);
        }
    }

    if (currentRegister == REG_ROTATE_DEG && rxCount >= 2) {
        uint16_t degrees = ((uint16_t)rxBuffer[0] << 8) | rxBuffer[1];
        rxCount = 0;
        currentRegister = 0xFF;   // clear so we don't re-execute
        if (motorEnabled && degrees > 0 && !stopRequested) {
            rotateDegrees((int16_t)degrees);
        }
    }
}
