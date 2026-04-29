"""
stepper_master_example.py
=========================
Example SMBus / I2C master script for use with a Raspberry Pi (or any
Linux board that exposes /dev/i2c-*).

Communicates with the StepperMotorController sketch running on an
Arduino Nano R4.

Requires:  pip install smbus2
"""

import time
import smbus2

# ─── Configuration ────────────────────────────────────────────────────────────
I2C_BUS     = 1       # /dev/i2c-1 on most Raspberry Pi models
ARDUINO_ADDR = 0x12   # must match I2C_ADDRESS in the Arduino sketch

# ─── Register addresses (must match the sketch) ──────────────────────────────
REG_ENABLE        = 0x00
REG_DIRECTION     = 0x01
REG_SET_RPM       = 0x02
REG_MOVE_STEPS    = 0x03
REG_ROTATE_DEG    = 0x04
REG_STOP          = 0x05
REG_SET_MICROSTEP = 0x06

REG_STATUS        = 0x10
REG_CURRENT_RPM   = 0x11
REG_STEPS_REMAIN  = 0x12


class StepperController:
    """High-level interface for the Arduino Nano R4 stepper controller."""

    def __init__(self, bus: int = I2C_BUS, address: int = ARDUINO_ADDR):
        self._bus     = smbus2.SMBus(bus)
        self._address = address

    # ── Write helpers ────────────────────────────────────────────────────────

    def enable(self) -> None:
        """Enable the stepper driver output."""
        self._bus.write_i2c_block_data(self._address, REG_ENABLE, [0x01])

    def disable(self) -> None:
        """Disable (de-energise) the stepper driver output."""
        self._bus.write_i2c_block_data(self._address, REG_ENABLE, [0x00])

    def set_direction(self, reverse: bool = False) -> None:
        """Set motor direction.  reverse=False → forward, reverse=True → reverse."""
        self._bus.write_i2c_block_data(self._address, REG_DIRECTION, [0x01 if reverse else 0x00])

    def set_rpm(self, rpm: int) -> None:
        """Set motor speed in RPM (1–255)."""
        rpm = max(1, min(255, int(rpm)))
        self._bus.write_i2c_block_data(self._address, REG_SET_RPM, [rpm])

    def set_microstep(self, microstep: int) -> None:
        """Set micro-stepping resolution.  Valid values: 1, 2, 4, 8, 16."""
        if microstep not in (1, 2, 4, 8, 16):
            raise ValueError(f"Invalid microstep value: {microstep}")
        self._bus.write_i2c_block_data(self._address, REG_SET_MICROSTEP, [microstep])

    def move_steps(self, steps: int) -> None:
        """Command the motor to move a given number of steps."""
        steps = max(0, min(0xFFFF, int(steps)))
        high  = (steps >> 8) & 0xFF
        low   = steps & 0xFF
        self._bus.write_i2c_block_data(self._address, REG_MOVE_STEPS, [high, low])

    def rotate_degrees(self, degrees: int) -> None:
        """Command the motor to rotate by a given number of degrees."""
        degrees = max(0, min(0xFFFF, int(degrees)))
        high    = (degrees >> 8) & 0xFF
        low     = degrees & 0xFF
        self._bus.write_i2c_block_data(self._address, REG_ROTATE_DEG, [high, low])

    def stop(self) -> None:
        """Stop any ongoing motor movement immediately."""
        self._bus.write_i2c_block_data(self._address, REG_STOP, [0x01])

    # ── Read helpers ─────────────────────────────────────────────────────────

    def get_status(self) -> dict:
        """
        Read the STATUS register and return a dict with:
          busy      – motor is currently moving
          enabled   – driver output is enabled
          direction – True = reverse, False = forward
        """
        # Set register pointer, then read
        self._bus.write_byte(self._address, REG_STATUS)
        time.sleep(0.001)
        byte = self._bus.read_byte(self._address)
        return {
            "busy":      bool(byte & 0x01),
            "enabled":   bool(byte & 0x02),
            "direction": bool(byte & 0x04),
        }

    def get_rpm(self) -> int:
        """Read the current RPM setting from the controller."""
        self._bus.write_byte(self._address, REG_CURRENT_RPM)
        time.sleep(0.001)
        return self._bus.read_byte(self._address)

    def get_steps_remaining(self) -> int:
        """Read the number of steps remaining in the current move."""
        self._bus.write_byte(self._address, REG_STEPS_REMAIN)
        time.sleep(0.001)
        data = self._bus.read_i2c_block_data(self._address, REG_STEPS_REMAIN, 2)
        return (data[0] << 8) | data[1]

    def close(self) -> None:
        """Release the I2C bus."""
        self._bus.close()


# ─── Demo ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    ctrl = StepperController()

    print("Enabling motor …")
    ctrl.enable()

    print("Setting speed to 80 RPM …")
    ctrl.set_rpm(80)

    print("Setting 1/8 micro-stepping …")
    ctrl.set_microstep(8)

    print("Moving 400 steps forward …")
    ctrl.set_direction(reverse=False)
    ctrl.move_steps(400)
    time.sleep(2)

    print("Rotating 180° in reverse …")
    ctrl.set_direction(reverse=True)
    ctrl.rotate_degrees(180)
    time.sleep(2)

    status = ctrl.get_status()
    print(f"Status: {status}")
    print(f"Current RPM reported: {ctrl.get_rpm()}")

    print("Disabling motor …")
    ctrl.disable()
    ctrl.close()
    print("Done.")
