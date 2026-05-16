# WetenschappenlijkProject - Stepper Motor Controller

## Overview

A complete stepper motor control system consisting of:
- **Arduino Nano R4 Slave** (`Slave/`): I2C slave microcontroller that directly controls a TB6600 stepper motor driver via PWM and GPIO
- **Python Master Controller** (`Master/`): High-level Python API for remote motor control via I2C from Raspberry Pi or Linux computer

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Master (Python)                          │
│                   - High-level API                          │
│               - Motor control commands                      │
│                - Error handling & retry                     │
└────────────┬────────────────────────────────────────────────┘
             │ I2C Bus (SMBus) - 400 kHz
             │ SDA (GPIO 2) / SCL (GPIO 3) on RPi
             ↓
┌─────────────────────────────────────────────────────────────┐
│              Arduino Nano R4 (Slave)                        │
│        - I2C register interface (0x20-0x2F)                 │
│        - Motion state machine                               │
│        - Hardware PWM pulse generation                      │
│        - DIP-switch address configuration                   │
└────┬────────────────────────────────────────┬───────────────┘
     │ GPIO Pins                              │ I2C
     ├─ Pin 7 (EN)  ────→ DM320T ENABLE       ├─ SDA (pin 18)
     ├─ Pin 8 (DIR) ────→ DM320T DIRECTION    └─ SCL (pin 19)
     └─ Pin 9 (PUL) ────→ DM320T PULSE
             ↓
    ┌────────────────────┐
    │  TB6600 Stepper    │
    │ Motor Driver       │
    │ (with A4988-like   │
    │  interface)        │
    └────────────────────┘
             ↓
    ┌────────────────────┐
    │  Stepper Motor     │
    │  (NEMA17, etc.)    │
    └────────────────────┘
```

---

## Quick Start

### 1. Arduino Setup (Slave)

#### Hardware Requirements
- Arduino Nano R4
- TB6600 Stepper Driver
- Stepper Motor (e.g., NEMA17 with 200 steps/rev)
- 4x DIP Switches (for I2C address configuration)
- Breadboard & jumper wires

#### Wiring

| Signal | Arduino Pin | DM320T Pin | Purpose |
|--------|-------------|-----------|---------|
| **PUL** | 9 | STEP | Pulse signal (rising edge = one step) |
| **DIR** | 8 | DIR | Direction control (HIGH=forward, LOW=reverse) |
| **EN** | 7 | ENABLE | Enable driver (HIGH=enabled, LOW=disabled) |
| **SDA** | 18 | – | I2C data line |
| **SCL** | 19 | – | I2C clock line |
| **GND** | GND | GND | Common ground |
| **5V** | 5V | Logic VCC | Arduino 5V logic supply |
| – | VMOT | VMOT | Motor power supply (separate 12-24V) |

#### I2C Address Configuration (DIP Switches)

The Arduino's I2C slave address is configured via 4 DIP switches on Arduino pins 2, 3, 4, 5:

| DIP | Pin | Binary Pos |
|-----|-----|-----------|
| S0 | 5 | bit 0 |
| S1 | 4 | bit 1 |
| S2 | 3 | bit 2 |
| S3 | 2 | bit 3 |

**Final I2C Address = 0x20 + (DIP nibble value)**

**Common Configurations:**
- All OFF (0000): **0x20**
- Bit 3 ON (1000): **0x28** 
- Bit 0 ON (0001): **0x21**
- All ON (1111): **0x2F**

#### Upload Arduino Sketch

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Open `Slave/StepperMotorController/StepperMotorController.ino`
3. Select Board: **Arduino Nano r4**
4. Select Port
5. Click **Upload**

---

### 2. Python Setup (Master)

#### Installation

```bash
# Install Python dependencies
cd Master
pip install smbus2

# Optional: For development
pip install -e src/
```

#### Verify I2C Connection

```bash
# List connected I2C devices
i2cdetect -y 1

# Expected output: Your Arduino address (e.g., 0x20, 0x28)
```

#### Quick Test

```python
from src.stepper_i2c import StepperController

# Create controller (address depends on DIP switch configuration)
with StepperController(address=0, bus=1) as motor:
    print("Motor State:", motor.get_state())
    
    # Move 100 steps at 50% speed
    motor.move_steps(100, speed_percent=50.0, clockwise=True)
    motor.wait_until_complete(timeout_sec=10)
    
    print("Done!")
```

## Python API Usage

### Basic Control

```python
from src.stepper_i2c import StepperController

with StepperController(address=0, bus=1) as motor:
    # Get current state
    state = motor.get_state()
    print(f"Speed: {state['speed_percent']}%")
    print(f"Moving: {state['enabled'] and not state.get('is_complete', True)}")
    
    # Control motor
    motor.set_direction(clockwise=True)
    motor.set_speed_percent(75.0)
    motor.enable(True)
    
    # Stop
    motor.stop()
```

### Examples

See the complete example in `Master/`:
- **draw_robot.py** – Multi-axis drawing application with two motors running in parallel

For detailed API documentation and more usage patterns, see [Master/CONTROLLER_DOCUMENTATION.md](Master/CONTROLLER_DOCUMENTATION.md).

---

## Troubleshooting

### Arduino Won't Upload
- Check USB cable (data cable, not power-only)
- Verify correct board selected: **Arduino Nano 33 IoT** or **Arduino Nano 4 Wifi**
- Try different USB port
- Update Arduino IDE bootloader

### I2C Not Detected
```bash
# Check if Arduino is visible
i2cdetect -y 1

# If not visible:
# 1. Verify SDA/SCL wiring (pins 18/19 on Nano R4)
# 2. Check pull-up resistors (typically 2.2kΩ required)
# 3. Verify DIP switch setting matches your slave address
```

### Motor Not Moving
1. **Check DIP switches** – Verify address matches Python code
2. **Verify hardware wiring** – EN, DIR, PUL pins connected
3. **Test with Python**:
   ```python
   motor.get_state()  # Check if slave responds
   motor.enable(True)  # Try enabling
   ```
4. **Check period value** – Must be between 1000-65535 µs (controlled by Arduino)
5. **Verify motor supply voltage** – VMOT should be 12-24V (depends on driver)

### Intermittent I2C Errors
- Add 100nF capacitors near Arduino 5V and GND
- Reduce I2C cable length or shield cables
- Use pull-up resistors on SDA/SCL (2.2k-10k)
- Enable I2C retry in Python: `i2c_retry_count=5`

### Motion Too Fast/Slow
- Use Python API: `motor.set_speed_percent(50.0)` for direct control
- Speed range: 0% (slowest) to 100% (fastest)
- Minimum practical period: 1000 µs (Arduino enforces minimum 20 µs)

---

## Project Structure

```
stepper-motor/
├── README.md                              # This file
├── Master/
│   ├── CONTROLLER_DOCUMENTATION.md        # Complete Python API reference
│   ├── draw_robot.py                      # Multi-axis drawing example
│   └── src/stepper_i2c/
│       ├── __init__.py                    # Package init
│       ├── controller.py                  # Main StepperController class
│       └── constants.py                   # I2C register definitions
└── Slave/
    └── StepperMotorController/
        └── StepperMotorController.ino     # Arduino sketch (I2C slave)
```

---

## Key Features

✅ **Arduino Slave:**
- I2C register-based interface
- DIP-configurable slave address (0x20-0x2F)
- Hardware PWM pulse generation
- Motion state machine (continuous/finite modes)
- Robust I2C communication with timeout

✅ **Python Master:**
- High-level API for motor control
- Speed control in % or RPM
- Relative motion (steps, degrees, revolutions)
- I2C error retry with exponential backoff
- Context manager for resource management
- Motion completion detection

---

## Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| I2C Bus Speed | 400 kHz | Standard SMBus speed |
| I2C Address Range | 0x20–0x2F | 16 possible addresses via DIP |
| Step Period Range | 20–65535 µs | Arduino: min 20 µs, Python: min 1000 µs |
| Pulse Count | 0–65535 | 0 = continuous, >0 = finite move |
| Motion Completion Check | Polling | Read REG_MOTION_COMPLETE_FLAG |
| Max Simultaneous Motors | 16 | Limited by I2C addresses (one per master) |

---

## Notes

- **No position tracking** – All motion is relative
- **I2C timeout** – Arduino has 25ms timeout on I2C; if master stalls, Arduino resets
- **Step period minimum** – Arduino enforces minimum 20 µs period (Python default 1000 µs for stability)
- **Separate power supplies recommended** – Motor supply (12-24V) separate from logic (5V)
