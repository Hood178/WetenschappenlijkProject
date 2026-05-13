"""High-level I2C master interface for the stepper controller."""

from smbus2 import SMBus
import time
from . import constants as const

class StepperController:
    """High-level I2C master interface for the stepper controller.

    This class exposes the public API used by application code and keeps the
    low-level register access methods internal.
    """
    
    def __init__(self, address: int, bus: int = const.I2C_BUS, steps_per_rev: int = const.STEPS_PER_REV):
        """Create a controller for one stepper slave.

        Args:
            address: Low 4-bit address offset for the slave.
            bus: I2C bus number to open.
            steps_per_rev: Number of motor steps per full revolution.
        """
        self._address = const.BASE_I2C_ADDRESS | (address & 0x0F)
        self._bus = SMBus(bus)
        self._steps_per_rev = steps_per_rev

    def close(self) -> None:
        """Close the I2C bus connection."""
        self._bus.close()
    
    def __enter__(self):
        """Enter a context manager block and return this controller."""
        return self
    
    def __exit__(self, exc_type, exc, tb):
        """Exit a context manager block and always close the bus."""
        self.close()
        return False
    
    
    
    """-------- Low-level register access methods -------"""
        
    def _write_8(self, reg: int, value: int) -> None:
        """Write an 8-bit value to a register.

        Args:
            reg: Register address.
            value: Value to write. Only the lowest 8 bits are used.
        """
        value = int(value) & 0xFF
        self._bus.write_byte_data(self._address, reg, value & 0xFF)
    
    def _write_16(self, reg_high: int, value: int) -> None:
        """Write a 16-bit value to two consecutive registers.

        The value is written big-endian: high byte first, then low byte.

        Args:
            reg_high: Register address of the high byte.
            value: Value to write.
        """
        value = int(value) & 0xFFFF
        high = (value >> 8) & 0xFF
        low = value & 0xFF
        
        self._bus.write_byte_data(self._address, reg_high, high)
        self._bus.write_byte_data(self._address, reg_high + 1, low)
    
    def _read_8(self, reg: int) -> int:
        """Read an 8-bit value from a register.

        Args:
            reg: Register address.

        Returns:
            The byte read from the register.
        """
        return self._bus.read_byte_data(self._address, reg)
    
    def _read_16(self, reg_high: int) -> int:
        """Read a 16-bit value from two consecutive registers.

        The value is read big-endian: high byte first, then low byte.

        Args:
            reg_high: Register address of the high byte.

        Returns:
            The 16-bit value read from the registers.
        """
        high = self._bus.read_byte_data(self._address, reg_high)
        low = self._bus.read_byte_data(self._address, reg_high + 1)
        return (high << 8) | low 
    
    
    """-------- low-level control methods -------"""
    
    def enable(self, state: bool) -> None:
        """Enable or disable the stepper driver output.

        Args:
            state: True to enable the driver, False to disable it.

        Raises:
            ValueError: If state is not a boolean.
        """
        if not isinstance(state, bool):
            raise ValueError("state must be a boolean")
        self._write_8(const.REG_ENABLE, 0x01 if state else 0x00)
    
    def set_direction(self, clockwise: bool) -> None:
        """Set the stepper motor direction.

        Args:
            clockwise: True for clockwise, False for counter-clockwise.

        Raises:
            ValueError: If clockwise is not a boolean.
        """
        if not isinstance(clockwise, bool):
            raise ValueError("clockwise must be a boolean")
        self._write_8(const.REG_DIRECTION, 0x01 if clockwise else 0x00)
    
    def _set_period_us(self, period_us: float) -> None:
        """Set the step period in microseconds.

        The value is clipped to the supported range.

        Args:
            period_us: Desired step period in microseconds.
        """
        clipped = max(const.MIN_PERIOD_US, min(const.MAX_PERIOD_US, int(period_us)))
        self._write_16(const.REG_PERIOD_US_H, clipped)

    def _set_step_count(self, count: int) -> None:
        """Set the number of steps to execute.

        0 = continuous motion while ENABLE is on.
        > 0 = finite motion that stops after exactly that many steps.

        Args:
            count: Step count to write.

        Raises:
            ValueError: If count is not an integer.
        """
        if not isinstance(count, int):
            raise ValueError("count must be an integer")
        clipped = max(0, min(65535, count))
        self._write_16(const.REG_PCOUNT_H, clipped)
    
    def _speed_percent_to_period_us(self, percent: float) -> int:
        """Convert a speed percentage to a period in microseconds.

        Args:
            percent: Speed percentage between 0 and 100.

        Returns:
            The equivalent step period in microseconds.

        Raises:
            ValueError: If percent is invalid.
        """
        if not isinstance(percent, (int, float)):
            raise ValueError("percent must be a number")
        if not (0 <= percent <= 100):
            raise ValueError("percent must be between 0 and 100")
        # Map 0% to MIN_PERIOD_US (max speed) and 100% to MAX_PERIOD_US (min speed)
        return int(const.MAX_PERIOD_US - (const.MAX_PERIOD_US - const.MIN_PERIOD_US) * (percent / 100.0))
    
    """-------- high-level control methods -------"""
    def set_speed_percent(self, speed_percent: float) -> None:
        """Set the speed as a percentage of maximum speed.

        Args:
            speed_percent: Speed percentage between 0 and 100.
        """
        if not isinstance(speed_percent, (int, float)):
            raise ValueError("speed_percent must be a number")
        self._set_period_us(self._speed_percent_to_period_us(speed_percent))
        
    def set_speed_rpm(self, rpm: float) -> None:
        """Set the speed in revolutions per minute.

        Args:
            rpm: Desired speed in revolutions per minute.

        Raises:
            ValueError: If rpm is invalid.
        """
        if not isinstance(rpm, (int, float)):
            raise ValueError("rpm must be a number")
        if rpm <= 0:
            raise ValueError("rpm must be bigger than 0")

        steps_per_sec = (rpm / 60.0) * self._steps_per_rev
        period_us = 1_000_000 / steps_per_sec
        self._set_period_us(period_us)
    
    def change_speed(self, delta_percent: float) -> None:
        """Change the speed by a relative percentage.

        Args:
            delta_percent: Positive to speed up, negative to slow down.

        Raises:
            ValueError: If delta_percent is invalid.
        """
        if not isinstance(delta_percent, (int, float)):
            raise ValueError("delta_percent must be a number")
        if not (-100 <= delta_percent <= 100):
            raise ValueError("delta_percent must be between -100 and 100")
        current_state = self.get_state()
        new_speed = current_state["speed_percent"] + delta_percent
        if new_speed < 0:
            new_speed = 0
        if new_speed > 100:
            new_speed = 100
        self.set_speed_percent(new_speed)
    
    def start(self) -> None:
        """Start the motor with default settings.

        This is equivalent to running continuously at 50% speed clockwise.
        """
        self.run_continuous(speed_percent=50.0, clockwise=True)
    
    def stop(self) -> None:
        """Stop the motor by disabling the driver output."""
        self.enable(False)
    
    def move_steps(self, steps: int, speed_percent: float = 50.0, clockwise: bool = True) -> None:
        """Execute a finite move by a number of steps.

        Args:
            steps: Number of steps to execute.
            speed_percent: Speed percentage between 0 and 100.
            clockwise: True for clockwise, False for counter-clockwise.

        Raises:
            ValueError: If any argument is invalid.
        """
        if not isinstance(steps, int):
            raise ValueError("steps must be an integer")
        if not isinstance(speed_percent, (int, float)):
            raise ValueError("speed_percent must be a number")
        if not (0 <= speed_percent <= 100):
            raise ValueError("speed_percent must be between 0 and 100")
        self.enable(True)
        self.set_direction(clockwise)
        self._set_period_us(self._speed_percent_to_period_us(speed_percent))
        self._set_step_count(steps)

    def move_degrees(self, degrees: float, speed_percent: float = 50.0, clockwise: bool = True) -> None:
        """Execute a move by degrees.

        Args:
            degrees: Number of degrees to move.
            speed_percent: Speed percentage between 0 and 100.
            clockwise: True for clockwise, False for counter-clockwise.

        Raises:
            ValueError: If any argument is invalid.
        """
        if not isinstance(degrees, (int, float)):
            raise ValueError("degrees must be a number")
        if not isinstance(speed_percent, (int, float)):
            raise ValueError("speed_percent must be a number")
        if not (0 <= speed_percent <= 100):
            raise ValueError("speed_percent must be between 0 and 100")
        steps = int((degrees / 360.0) * self._steps_per_rev)
        self.move_steps(steps=steps, speed_percent=speed_percent, clockwise=clockwise)

    def run_continuous(self, speed_percent: float = 50.0, clockwise: bool = True) -> None:
        """Start continuous motion until `stop()` is called.

        Args:
            speed_percent: Speed percentage between 0 and 100.
            clockwise: True for clockwise, False for counter-clockwise.

        Raises:
            ValueError: If any argument is invalid.
        """
        if not isinstance(speed_percent, (int, float)):
            raise ValueError("speed_percent must be a number")
        if not (0 <= speed_percent <= 100):
            raise ValueError("speed_percent must be between 0 and 100")
        self.enable(True)
        self.set_direction(clockwise)
        self._set_period_us(self._speed_percent_to_period_us(speed_percent))
        self._set_step_count(0)  # 0 = continuous

    def get_state(self) -> dict:
        """Read back the current slave state.

        Returns:
            A dictionary containing enabled state, direction, period, speed,
            pulse count, and whether the motion is continuous.
        """
        en = bool(self._read_8(const.REG_ENABLE))
        dir_val = bool(self._read_8(const.REG_DIRECTION))
        period_us = self._read_16(const.REG_PERIOD_US_H)
        pcount = self._read_16(const.REG_PCOUNT_H)

        # Calculate speed percent inverse of _speed_percent_to_period_us()
        # Mapping: 0% -> MIN_PERIOD_US (fastest), 100% -> MAX_PERIOD_US (slowest)
        if const.MAX_PERIOD_US == const.MIN_PERIOD_US:
            speed_percent = 0.0
        else:
            period_clamped = max(const.MIN_PERIOD_US, min(const.MAX_PERIOD_US, period_us))
            speed_percent = (
                (const.MAX_PERIOD_US - period_clamped)
                / (const.MAX_PERIOD_US - const.MIN_PERIOD_US)
            ) * 100.0

        return {
            "enabled": en,
            "clockwise": dir_val,
            "period_us": period_us,
            "speed_percent": float(speed_percent),
            "pulse_count": pcount,
            "is_continuous": pcount == 0,
        }
    
    def rotate(self, revs: float, speed_percent: float = 50.0, clockwise: bool = True) -> None:
        """Rotate a given number of revolutions.

        Args:
            revs: Number of revolutions to move.
            speed_percent: Speed percentage between 0 and 100.
            clockwise: True for clockwise, False for counter-clockwise.

        Raises:
            ValueError: If any argument is invalid.
        """
        if not isinstance(revs, (int, float)):
            raise ValueError("revs must be a number")
        if not isinstance(speed_percent, (int, float)):
            raise ValueError("speed_percent must be a number")
        if not (0 <= speed_percent <= 100):
            raise ValueError("speed_percent must be between 0 and 100")
        steps = int(revs * self._steps_per_rev)
        self.move_steps(steps=steps, speed_percent=speed_percent, clockwise=clockwise)
    
    def is_moving(self) -> bool:
        """Check whether the motor is currently moving.

        Returns:
            True if ENABLE is on and motion is not complete, otherwise False.
        """
        en = bool(self._read_8(const.REG_ENABLE))
        complete = bool(self._read_8(const.MOTION_COMPLETE_FLAG))
        return en and not complete
    
    def wait_until_complete(self, timeout_sec : float =30.0) -> bool | None:
        """Block until motion is complete or until the timeout is reached.

        Args:
            timeout_sec: Maximum time to wait in seconds.

        Returns:
            True if the motion completes before the timeout.

        Raises:
            TimeoutError: If the timeout is exceeded.
        """
        start = time.time()
        while time.time() - start < timeout_sec:
            if not self.is_moving():
                return True
            time.sleep(0.01)
        raise TimeoutError("Motion did not complete within timeout")
    
    def get_speed_percent(self) -> float:
        """Get the current speed as a percentage of maximum speed.

        Returns:
            Current speed percentage.
        """
        state = self.get_state()
        return state["speed_percent"]
    

    