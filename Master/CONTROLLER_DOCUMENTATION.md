# StepperController Documentation

## Overview

`StepperController` is a high-level Python I2C master interface for controlling a DM320T stepper motor driver connected to an Arduino Nano R4 slave device. It provides an easy-to-use API for motor control while handling the low-level I2C register communication automatically.

**Key Features:**
- Remote motor control via I2C protocol
- Speed control in percentage or RPM
- Relative motion control (steps, degrees, revolutions)
- Continuous and finite motion modes
- Motion state monitoring
- Context manager support for automatic resource cleanup

---

## Architecture

### Communication Model

The controller communicates with an Arduino slave over I2C using a register-based interface. The slave device maintains the state of the motor (enabled/disabled, direction, speed, pulse count) and handles the hardware timing for pulse generation.

```
  Master (Python)                   I2C Bus                  Slave (Arduino)
┌──────────────────┐           ┌──────────────┐           ┌──────────────────┐
│ StepperController├───────────┤ I2C Bus (SMB)├───────────┤ Arduino DM320T   │
│                  │           │              │           │ Controller       │
│ - High-level API │           │ Register Map │           │ - Hardware timing│
│ - I2C commands   │           │              │           │ - Pin control    │
└──────────────────┘           └──────────────┘           └──────────────────┘
```

### Register Map

The slave device exposes the following I2C registers:
```

| Address | Name | Type | Size | Purpose |
|---------|------|------|------|---------|
| 0x00 | REG_ENABLE | R/W | 1 byte | Enable/disable driver (`0x00`=disabled, `0x01`=enabled + start motion) |
| 0x01 | REG_DIRECTION | R/W | 1 byte | Motor direction (`0x00`=forward, `0x01`=reverse) |
| 0x02 | REG_PERIOD_US_H | R/W | 1 byte | Step period high byte (part of 16-bit big-endian value) |
| 0x03 | REG_PERIOD_US_L | R/W | 1 byte | Step period low byte (combined: period = (H << 8) \| L, in µs) |
| 0x04 | REG_PCOUNT_H | R/W | 1 byte | Pulse count high byte (part of 16-bit big-endian value) |
| 0x05 | REG_PCOUNT_L | R/W | 1 byte | Pulse count low byte (combined: count = (H << 8) \| L) |
| 0x06 | MOTION_COMPLETE_FLAG | R | 1 byte | Motion status (`0x00`=moving, `0x01`=complete/idle) |

**Motion Modes:**
- **Continuous:** Set `REG_PCOUNT` to 0; motor runs indefinitely until disabled
- **Finite:** Set `REG_PCOUNT` to desired count; motor stops after that many pulses

---

## Usage Examples

### Basic Setup

```python
from stepper_i2c.controller import StepperController

# Create controller for slave at address 0x08 (base + offset 0)
controller = StepperController(address=0, bus=1)

try:
    # Your control code here
    pass
finally:
    controller.close()
```

### Using Context Manager (Recommended)

```python
from stepper_i2c.controller import StepperController

# Automatically closes connection on exit
with StepperController(address=0) as motor:
    motor.start()  # Run continuously at 50% speed, clockwise
    motor.stop()   # Disable the motor
```

### Speed Control

```python
with StepperController(address=0) as motor:
    # Set speed as percentage (0-100)
    motor.set_speed_percent(75.0)  # 75% of maximum speed
    
    # Set speed in RPM
    motor.set_speed_rpm(300)  # 300 revolutions per minute
    
    # Change speed relative to current
    motor.change_speed(+10)  # Speed up by 10%
    motor.change_speed(-20)  # Slow down by 20%
    
    # Get current speed
    current_speed = motor.get_speed_percent()
    print(f"Current speed: {current_speed}%")
```

### Relative Motion (No Position Tracking)

```python
with StepperController(address=0) as motor:
    # Move by exact number of steps
    motor.move_steps(steps=100, speed_percent=50.0, clockwise=True)
    
    # Move by degrees (relative rotation)
    motor.move_degrees(degrees=45.0, speed_percent=50.0, clockwise=True)
    
    # Rotate by full revolutions
    motor.rotate(revs=2.5, speed_percent=50.0, clockwise=False)
    
    # Run continuously until stopped
    motor.run_continuous(speed_percent=60.0, clockwise=True)
```

### Direction Control

```python
with StepperController(address=0) as motor:
    # Clockwise rotation
    motor.set_direction(clockwise=True)
    motor.move_steps(100)
    
    # Counter-clockwise rotation
    motor.set_direction(clockwise=False)
    motor.move_steps(100)
```

### Motion Monitoring

```python
with StepperController(address=0) as motor:
    # Check if motor is currently moving
    if motor.is_moving():
        print("Motor is moving")
    else:
        print("Motor is idle")
    
    # Wait for motion to complete (with timeout)
    try:
        motor.move_steps(500, speed_percent=50)
        motor.wait_until_complete(timeout_sec=10.0)
        print("Motion completed successfully")
    except TimeoutError:
        print("Motion did not complete within timeout")
```

### Reading Controller State

```python
with StepperController(address=0) as motor:
    state = motor.get_state()
    print(f"Enabled: {state['enabled']}")
    print(f"Direction: {'CW' if state['clockwise'] else 'CCW'}")
    print(f"Speed: {state['speed_percent']}%")
    print(f"Pulse Count: {state['pulse_count']}")
    print(f"Continuous Mode: {state['is_continuous']}")
    print(f"Period (µs): {state['period_us']}")
```

---

## API Reference

### Initialization

#### `__init__(address: int | str, bus: int = 1, steps_per_rev: int = 200, i2c_retry_count: int = 3, i2c_retry_delay: float = 0.05, i2c_retry_backoff: float = 2.0)`

Create a new stepper controller instance.

**Parameters:**
- `address` (int or str): Low 4-bit I2C address offset (0-15 or "0000"-"1111" binary). Final I2C address = base (0x20) (module address) + offset
- `bus` (int, optional): I2C bus number. Default: 1
- `steps_per_rev` (int, optional): Motor steps per full revolution. (can be set by flipping switches on driver module) Default: 200
- `i2c_retry_count` (int, optional): Number of retry attempts for transient I2C errors. Default: 3
- `i2c_retry_delay` (float, optional): Initial delay in seconds before retrying. Default: 0.05
- `i2c_retry_backoff` (float, optional): Multiplier for exponential backoff between retries. Default: 2.0

**Example:**
```python
motor = StepperController(address=0, bus=1, steps_per_rev=200)
motor = StepperController(address="0101", bus=1)  # Binary address
```

---

### Motion Control

#### `move_steps(steps: int, speed_percent: float = 50.0, clockwise: bool = True)`

Execute a finite move by an exact number of steps.

**Parameters:**
- `steps` (int): Number of motor steps to execute
- `speed_percent` (float): Speed as percentage (0-100). Default: 50%
- `clockwise` (bool): True for clockwise, False for counter-clockwise. Default: True

**Raises:**
- `ValueError`: If parameters are invalid

**Example:**
```python
motor.move_steps(steps=400, speed_percent=75, clockwise=True)
```

---

#### `move_degrees(degrees: float, speed_percent: float = 50.0, clockwise: bool = True)`

Execute a move by degrees of rotation.

**Parameters:**
- `degrees` (float): Degrees to rotate
- `speed_percent` (float): Speed as percentage (0-100). Default: 50%
- `clockwise` (bool): Rotation direction. Default: True

**Example:**
```python
motor.move_degrees(degrees=180, speed_percent=50)  # Half rotation
```

---

#### `rotate(revs: float, speed_percent: float = 50.0, clockwise: bool = True)`

Rotate by a number of full revolutions.

**Parameters:**
- `revs` (float): Number of revolutions (can be fractional)
- `speed_percent` (float): Speed as percentage (0-100). Default: 50%
- `clockwise` (bool): Rotation direction. Default: True

**Example:**
```python
motor.rotate(revs=2.5, speed_percent=60)  # 2.5 full rotations
```

---

#### `run_continuous(speed_percent: float = 50.0, clockwise: bool = True)`

Start continuous rotation until `stop()` is called.

**Parameters:**
- `speed_percent` (float): Speed as percentage (0-100). Default: 50%
- `clockwise` (bool): Rotation direction. Default: True

**Example:**
```python
motor.run_continuous(speed_percent=75, clockwise=True)
# Motor runs indefinitely...
motor.stop()  # Stop the motor
```

---

#### `start()`

Start the motor with default settings (50% speed, clockwise, continuous).

**Example:**
```python
motor.start()
motor.stop()
```

---

#### `stop()`

Stop the motor immediately by disabling the driver output.

**Example:**
```python
motor.stop()
```

---

### Speed Control

#### `set_speed_percent(speed_percent: float)`

Set motor speed as a percentage of maximum speed.

**Parameters:**
- `speed_percent` (float): Speed percentage (0-100)
  - 0% = slowest (maximum step period = 65535 µs)
  - 100% = fastest (minimum step period = 1000 µs)

**Example:**
```python
motor.set_speed_percent(50)   # 50% speed
motor.set_speed_percent(100)  # Maximum speed
```

---

#### `set_speed_rpm(rpm: float)`

Set motor speed in revolutions per minute.

**Parameters:**
- `rpm` (float): Desired RPM (must be > 0)

**Raises:**
- `ValueError`: If RPM is ≤ 0

**Example:**
```python
motor.set_speed_rpm(300)  # 300 RPM
```

---

#### `change_speed(delta_percent: float)`

Adjust speed by a relative percentage of current speed.

**Parameters:**
- `delta_percent` (float): Speed change (-100 to +100)
  - Positive: speed up
  - Negative: slow down

**Example:**
```python
motor.change_speed(+20)   # Speed up by 20%
motor.change_speed(-10)   # Slow down by 10%
```

---

#### `get_speed_percent() -> float`

Get the current motor speed as a percentage.

**Returns:** Current speed percentage (0-100)

**Example:**
```python
speed = motor.get_speed_percent()
print(f"Current speed: {speed}%")
```

---

### Direction Control

#### `set_direction(clockwise: bool)`

Set the motor rotation direction.

**Parameters:**
- `clockwise` (bool): True for clockwise, False for counter-clockwise

**Example:**
```python
motor.set_direction(clockwise=True)   # Clockwise
motor.set_direction(clockwise=False)  # Counter-clockwise
```

---

### Low-Level Control

#### `enable(state: bool)`

Enable or disable the stepper driver output.

**Parameters:**
- `state` (bool): True to enable, False to disable

**Example:**
```python
motor.enable(True)   # Enable driver
motor.enable(False)  # Disable driver
```

---

### State Monitoring

#### `get_state() -> dict`

Read the complete state from the slave device.

**Returns:** Dictionary with keys:
- `enabled` (bool): Driver is enabled
- `clockwise` (bool): Rotation direction
- `period_us` (int): Step period in microseconds
- `speed_percent` (float): Speed as percentage
- `pulse_count` (int): Configured pulse count
- `is_continuous` (bool): Whether in continuous mode

**Example:**
```python
state = motor.get_state()
if state['is_continuous']:
    print("Running in continuous mode")
```

---

#### `is_moving() -> bool`

Check if the motor is currently executing motion.

**Returns:** True if enabled and motion is not complete, False otherwise

**Example:**
```python
while motor.is_moving():
    print("Still moving...")
    time.sleep(0.1)
```

---

#### `wait_until_complete(timeout_sec: float = 30.0) -> bool`

Block execution until the current motion completes or timeout is reached.

**Parameters:**
- `timeout_sec` (float): Maximum seconds to wait. Default: 30

**Returns:** True if motion completed before timeout

**Raises:**
- `TimeoutError`: If motion did not complete within timeout

**Example:**
```python
motor.move_steps(500)
try:
    motor.wait_until_complete(timeout_sec=10)
    print("Motion completed!")
except TimeoutError:
    print("Motion timeout!")
```

---

### Resource Management

#### `close()`

Close the I2C bus connection and release resources.

**Example:**
```python
motor.close()  # Always call when done
```

---

#### Context Manager: `__enter__()` and `__exit__()`

Use as a context manager for automatic resource cleanup.

**Example:**
```python
with StepperController(address=0) as motor:
    motor.move_steps(100)
    # Connection automatically closed here
```

---

## Constants

Constants are defined in `stepper_i2c/constants.py`:
You are not supposed to change these but you can if you really want to.

```python
BASE_I2C_ADDRESS = 0x20  # Base I2C address
I2C_BUS = 1              # Default I2C bus number
STEPS_PER_REV = 200      # Standard stepper motor steps per revolution
MIN_PERIOD_US = 1000     # Minimum step period (fastest speed)
MAX_PERIOD_US = 65535    # Maximum step period (slowest speed)
```

---

## Important Notes

### No Position Tracking

This controller **does not track absolute position**. It only supports **relative motion**:
- ✅ Move 100 steps forward
- ✅ Rotate 45 degrees clockwise
- ✅ Run continuously until stopped
- ❌ Move to absolute angle 85°
- ❌ Query current position

All motion is **relative** to wherever the motor currently is.

### Speed Mapping

Speed percentage maps inversely to step period:
- **0%** = slowest motion (period = 65535 µs)
- **50%** = medium speed (period ≈ 33267 µs)
- **100%** = fastest motion (period = 1000 µs)

### I2C Addressing

The final I2C address is calculated as:
```
Final Address = BASE_I2C_ADDRESS | (address & 0x0F)
```

For example:
- `StepperController(address=0)` connects to I2C address `0x20`
- `StepperController(address="1000")` connects to I2C address `0x28` (0x20 | 0x08)
- `StepperController(address=15)` connects to I2C address `0x2F` (0x20 | 0x0F)

### Motion Completion

For finite moves, monitor completion with:
```python
motor.move_steps(500)
while motor.is_moving():
    time.sleep(0.01)
print("Move complete!")
```

Or use the blocking method:
```python
motor.move_steps(500)
motor.wait_until_complete(timeout_sec=10)
```

---

## Troubleshooting

### Motor Not Responding

1. Check I2C bus communication: `i2cdetect -y 1`
2. Verify correct bus number (usually 1)
3. Verify correct address offset (DIP switches on Arduino)
4. Check physical I2C connections (SDA/SCL)

### Motion Too Fast/Slow

Adjust speed using `set_speed_percent()` or `set_speed_rpm()`. Note that very high speeds (>90%) or very low speeds (<10%) may be unreliable.

### Timeout Errors

If `wait_until_complete()` times out, increase the timeout value or check if the motor is mechanically stuck.

---
