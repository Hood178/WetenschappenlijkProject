"""High-level I2C master interface for the stepper controller."""

from smbus2 import SMBus
import time
from . import constants as const

class StepperController:
    """High-level I2C master interface for the stepper controller.

    This class exposes the public API used by application code and keeps the
    low-level register access methods internal.
    """
    
    def __init__(
        self,
        address: int | str,
        bus: int = const.I2C_BUS,
        steps_per_rev: int = const.STEPS_PER_REV,
        i2c_retry_count: int = 3,
        i2c_retry_delay: float = 0.05,
        i2c_retry_backoff: float = 2.0,
        invert: bool = False,
    ):
        """Create a controller for one stepper slave.

        Args:
            address: Low 4-bit address offset for the slave.
            bus: I2C bus number to open.
            steps_per_rev: Number of motor steps per full revolution.
            i2c_retry_count: Number of retry attempts for transient I2C errors.
            i2c_retry_delay: Initial delay in seconds before retrying.
            i2c_retry_backoff: Multiplier for exponential backoff between retries.
            invert: If True, inverts all direction commands (clockwise becomes counter-clockwise and vice versa).
                    Useful when motor is physically oriented differently. Default: False
        """
        if isinstance(address, str):
            address = int(address, 2)

        if i2c_retry_count < 1:
            raise ValueError("i2c_retry_count must be at least 1")
        if i2c_retry_delay < 0:
            raise ValueError("i2c_retry_delay must be >= 0")
        if i2c_retry_backoff < 1:
            raise ValueError("i2c_retry_backoff must be >= 1")
        
        self._address = const.BASE_I2C_ADDRESS | (address & 0x0F)
        self._bus_id = bus
        self._bus = SMBus(bus)
        self._steps_per_rev = steps_per_rev
        self._i2c_retry_count = int(i2c_retry_count)
        self._i2c_retry_delay = float(i2c_retry_delay)
        self._i2c_retry_backoff = float(i2c_retry_backoff)
        self._invert = bool(invert)

    def close(self) -> None:
        """Close the I2C bus connection."""
        self._bus.close()

    def _reopen_bus(self) -> None:
        """Re-open the SMBus handle after a transient I2C failure."""
        try:
            self._bus.close()
        except OSError:
            pass
        self._bus = SMBus(self._bus_id)

    def _should_retry_i2c_error(self, error: OSError) -> bool:
        """Return whether an OSError is typically transient on I2C."""
        retryable_errnos = {5, 110, 121}
        return error.errno in retryable_errnos

    def _i2c_retry_delay_for_attempt(self, attempt: int) -> float:
        """Compute exponential backoff delay for a retry attempt (1-based)."""
        return self._i2c_retry_delay * (self._i2c_retry_backoff ** max(0, attempt - 1))

    def _run_i2c_transaction(self, func):
        """Run an SMBus transaction with retry/reopen on transient I2C failures."""
        for attempt in range(1, self._i2c_retry_count + 1):
            try:
                return func()
            except OSError as error:
                is_last_attempt = attempt >= self._i2c_retry_count
                if is_last_attempt or not self._should_retry_i2c_error(error):
                    raise

                time.sleep(self._i2c_retry_delay_for_attempt(attempt))
                self._reopen_bus()
    
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
        self._run_i2c_transaction(lambda: self._bus.write_byte_data(self._address, reg, value & 0xFF))
    
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

        def transaction() -> None:
            self._bus.write_byte_data(self._address, reg_high, high)
            self._bus.write_byte_data(self._address, reg_high + 1, low)

        self._run_i2c_transaction(transaction)
    
    def _read_8(self, reg: int) -> int:
        """Read an 8-bit value from a register.

        Args:
            reg: Register address.

        Returns:
            The byte read from the register.
        """
        return self._run_i2c_transaction(lambda: self._bus.read_byte_data(self._address, reg))
    
    def _read_16(self, reg_high: int) -> int:
        """Read a 16-bit value from two consecutive registers.

        The value is read big-endian: high byte first, then low byte.

        Args:
            reg_high: Register address of the high byte.

        Returns:
            The 16-bit value read from the registers.
        """
        def transaction() -> int:
            high = self._bus.read_byte_data(self._address, reg_high)
            low = self._bus.read_byte_data(self._address, reg_high + 1)
            return (high << 8) | low

        return self._run_i2c_transaction(transaction)
    
    
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
                      If invert=True was set during initialization, this will be flipped.

        Raises:
            ValueError: If clockwise is not a boolean.
        """
        if not isinstance(clockwise, bool):
            raise ValueError("clockwise must be a boolean")
        # Invert direction if invert flag is set
        actual_direction = not clockwise if self._invert else clockwise
        self._write_8(const.REG_DIRECTION, 0x01 if actual_direction else 0x00)
    
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
        # Map 0% to MAX_PERIOD_US (min speed) and 100% to MIN_PERIOD_US (max speed)
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
        self.set_direction(clockwise)
        self._set_period_us(self._speed_percent_to_period_us(speed_percent))
        self._set_step_count(steps)
        print("steps:"+str(steps))
        self.enable(True)

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
        self.set_direction(clockwise)
        self._set_period_us(self._speed_percent_to_period_us(speed_percent))
        self._set_step_count(0)  # 0 = continuous
        self.enable(True)

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
        # Mapping: 0% -> MAX_PERIOD_US (slowest), 100% -> MIN_PERIOD_US (fastest)
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
        consecutive_complete_reads = 0
        while time.time() - start < timeout_sec:
            if not self.is_moving():
                consecutive_complete_reads += 1
                if consecutive_complete_reads >= 2:
                    return True
            else:
                consecutive_complete_reads = 0
            time.sleep(0.2)
        raise TimeoutError("Motion did not complete within timeout")
    
    def get_speed_percent(self) -> float:
        """Get the current speed as a percentage of maximum speed.

        Returns:
            Current speed percentage.
        """
        state = self.get_state()
        return state["speed_percent"]
    

    