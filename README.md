# WetenschappenlijkProject

## StepperMotorController

An Arduino Nano R4 sketch that acts as an **I2C (SMBus) slave** and drives a stepper motor via an A4988 (or compatible DRV8825 / TB6600) driver using the [StepperDriver](https://github.com/laurb9/StepperDriver) library.

---

### Hardware wiring

| Signal  | Arduino Nano R4 pin | A4988 pin |
|---------|---------------------|-----------|
| STEP    | D3                  | STEP      |
| DIR     | D4                  | DIR       |
| ENABLE  | D5                  | ENABLE    |
| MS1     | D6                  | MS1       |
| MS2     | D7                  | MS2       |
| MS3     | D8                  | MS3       |
| SDA     | A4                  | –         |
| SCL     | A5                  | –         |
| GND     | GND                 | GND       |

> The motor supply voltage is connected directly to the A4988 VMOT and GND pins (separate from the Arduino 5 V logic supply).

---

### Dependencies

Install the following library through the Arduino Library Manager (Tools → Manage Libraries):

- **StepperDriver** by Laurentiu Badea

---

### I2C command reference

Default slave address: **0x12**

#### Write registers (master → Arduino)

| Register | Name           | Payload                                        |
|----------|----------------|------------------------------------------------|
| `0x00`   | ENABLE         | `0x01` enable, `0x00` disable                  |
| `0x01`   | DIRECTION      | `0x00` forward, `0x01` reverse                 |
| `0x02`   | SET_RPM        | 1 byte RPM (1–255)                             |
| `0x03`   | MOVE_STEPS     | 2 bytes big-endian step count                  |
| `0x04`   | ROTATE_DEG     | 2 bytes big-endian degrees                     |
| `0x05`   | STOP           | any value – stops current movement             |
| `0x06`   | SET_MICROSTEP  | 1 byte: `1`, `2`, `4`, `8`, or `16`           |

#### Read registers (Arduino → master)

| Register | Name           | Response                                       |
|----------|----------------|------------------------------------------------|
| `0x10`   | STATUS         | 1 byte: bit0=busy, bit1=enabled, bit2=dir      |
| `0x11`   | CURRENT_RPM    | 1 byte RPM                                     |
| `0x12`   | STEPS_REMAIN   | 2 bytes big-endian steps remaining             |

---

### Example master script (Raspberry Pi / Linux)

See [`StepperMotorController/stepper_master_example.py`](StepperMotorController/stepper_master_example.py) for a Python example using `smbus2`.

```bash
pip install smbus2
python StepperMotorController/stepper_master_example.py
```

---

### Basic usage sequence

1. **Enable** the driver (`REG_ENABLE = 0x01`)
2. **Set RPM** (`REG_SET_RPM`)
3. **Set direction** (`REG_DIRECTION`)
4. **Move** (`REG_MOVE_STEPS` or `REG_ROTATE_DEG`)
5. Optionally **stop** at any time (`REG_STOP`)
6. **Disable** when done (`REG_ENABLE = 0x00`)