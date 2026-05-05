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
I2C_BUS = 1          # /dev/i2c-1 on most Raspberry Pi models
ARDUINO_ADDR = 0x08  # set this to match the DIP-based address

# ─── Register addresses (must match the sketch) ──────────────────────────────
REG_ENABLE = 0x00


class StepperController:
    """High-level interface for the Arduino Nano R4 DM320T controller."""

    def __init__(self, bus: int = I2C_BUS, address: int = ARDUINO_ADDR):
        self._bus     = smbus2.SMBus(bus)
        self._address = address

    def enable(self) -> None:
        """Enable the stepper driver output."""
        self._bus.write_i2c_block_data(self._address, REG_ENABLE, [0x01])

    def disable(self) -> None:
        """Disable (de-energise) the stepper driver output."""
        self._bus.write_i2c_block_data(self._address, REG_ENABLE, [0x00])

    def is_enabled(self) -> bool:
        """Read back REG_ENABLE: True if enabled."""
        self._bus.write_byte(self._address, REG_ENABLE)
        time.sleep(0.001)
        byte = self._bus.read_byte(self._address)
        return byte != 0

    def close(self) -> None:
        """Release the I2C bus."""
        self._bus.close()


# ─── Demo ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    ctrl = StepperController()

    print("Enable driver")
    ctrl.enable()
    time.sleep(0.2)
    print(f"Enabled? {ctrl.is_enabled()}")

    print("Disable driver")
    ctrl.disable()
    time.sleep(0.2)
    print(f"Enabled? {ctrl.is_enabled()}")

    ctrl.close()
