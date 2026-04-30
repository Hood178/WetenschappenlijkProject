from smbus2 import SMBus
from . import constants as const

class StepperController:
    """High-level I2C master interface for the stepper controller."""
    
    def __init__(self, address: int, bus: int = const.I2C_BUS):
        self._address = const.BASE_I2C_ADDRESS | (address & 0x0F)
        self._bus = SMBus(bus)
    
    def close(self) -> None:
        """Close the I2C bus connection."""
        self._bus.close()
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False
        
    def _write_8(self, reg: int, value: int) -> None:
        """Write an 8-bit value to a register."""
        self._bus.write_byte_data(self._address, reg, value & 0xFF)
    
    def _write_16(self, reg_high: int, value: int) -> None:
        """Write a 16-bit value to two consecutive registers (high byte first)."""
        value = int(value) & 0xFFFF
        high = (value >> 8) & 0xFF
        low = value & 0xFF
        
        self._bus.write_byte_data(self._address, reg_high, high)
        self._bus.write_byte_data(self._address, reg_high + 1, low)
    
    def _read_8(self, reg: int) -> int:
        """Read an 8-bit value from a register."""
        return self._bus.read_byte_data(self._address, reg)
    
    def _read_16(self, reg_high: int) -> int:
        """Read a 16-bit value from two consecutive registers (high byte first)."""
        high = self._bus.read_byte_data(self._address, reg_high)
        low = self._bus.read_byte_data(self._address, reg_high + 1)
        return (high << 8) | low
    
    def enable(self, state: bool) -> None:
        """Enable or disable the stepper driver output."""
        if not isinstance(state, bool):
            raise ValueError("state must be a boolean")
        self._write_8(const.REG_ENABLE, 0x01 if state else 0x00)