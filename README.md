# WetenschappenlijkProject - Stepper Motor Controller

## Overview

A complete stepper motor control system consisting of:
- **Arduino Nano R4 Slave**: I2C slave that controls a DM320T stepper motor driver
- **Python Master Controller**: High-level Python API for remote motor control via I2C

---

## Hardware Setup

### Arduino Nano R4 Pin Mapping

| Signal | Arduino Pin | DM320T Driver Pin |
|--------|-------------|-------------------|
| PUL (Pulse) | 9 | STEP |
| DIR (Direction) | 8 | DIR |
| EN (Enable) | 7 | ENABLE |
| SDA | 18 | – |
| SCL | 19 | – |
| GND | GND | GND |

### Address Configuration

The I2C slave address is configured via 4 DIP switches on pins 2–5:

| DIP Switch | Arduino Pin |
|------------|-------------|
| S0 | 5 |
| S1 | 4 |
| S2 | 3 |
| S3 | 2 |

The 7-bit slave address is calculated as: **0x20 + (DIP nibble)**

**Examples:**
- All switches OFF (0000): address **0x20**
- Binary 1000: address **0x28**
- Binary 1111: address **0x2F**

---

## I2C Register Map

### Write/Read Registers (Master ↔ Slave)

| Address | Name | Type | Description |
|---------|------|------|-------------|
| 0x00 | REG_ENABLE | R/W | Enable/disable driver (0x00 or 0x01) |
| 0x01 | REG_DIRECTION | R/W | Motor direction (0x00=forward, 0x01=reverse) |
| 0x02–0x03 | REG_PERIOD_US | R/W | Step period in microseconds (16-bit big-endian) |
| 0x04–0x05 | REG_PCOUNT | R/W | Pulse count for finite moves (16-bit big-endian) |
| 0x06 | MOTION_COMPLETE_FLAG | R | Motion status (0x00=moving, 0x01=complete) |

### Motion Modes

- **Continuous**: Set `REG_PCOUNT` to 0 → motor runs indefinitely
- **Finite**: Set `REG_PCOUNT` to N > 0 → motor executes exactly N pulses then stops

---

## Python Master Setup

### Installation

```bash
cd Master
pip install smbus2
```

### Usage Example

```python
from src.stepper_i2c import StepperController

# Create controller for slave at address 0x20 (DIP switches all OFF)
with StepperController(address=0, bus=1) as motor:
    # Set direction and speed
    motor.set_direction(clockwise=True)
    motor.set_speed_percent(50.0)
    
    # Move 500 steps
    motor.move_steps(500, speed_percent=75.0)
    motor.wait_until_complete(timeout_sec=30.0)
    
    # Run continuously
    motor.run_continuous(speed_percent=60.0)
    # ... motor runs until stopped
    motor.stop()
```

### API Documentation

See [CONTROLLER_DOCUMENTATION.md](Master/CONTROLLER_DOCUMENTATION.md) for comprehensive API reference and examples.

---

## Project Structure

```
stepper-motor/
├── Master/
│   ├── CONTROLLER_DOCUMENTATION.md    # Complete API documentation
│   ├── draw_robot.py                  # Example drawing application
│   ├── main.py                        # Alternative example
│   ├── continuous.py                  # Continuous motion example
│   └── src/stepper_i2c/
│       ├── __init__.py
│       ├── controller.py              # Main controller class
│       └── constants.py               # I2C register definitions
└── Slave/
    └── StepperMotorController/
        └── StepperMotorController.ino # Arduino sketch
```

---

## Key Features

✅ High-level Python API for motor control  
✅ I2C register-based communication  
✅ Speed control (percentage or RPM)  
✅ Relative motion support (steps, degrees, revolutions)  
✅ Continuous and finite motion modes  
✅ Motion completion detection  
✅ I2C error retry with exponential backoff  
✅ Context manager support for resource management  

---

## Notes

- **No position tracking**: Controller supports relative motion only
- **Speed range**: 0% (slowest, 65535 µs period) to 100% (fastest, 1000 µs period)
- **Default parameters**: 200 steps/rev, I2C bus 1, 50% speed