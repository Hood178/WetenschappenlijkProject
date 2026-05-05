# WetenschappenlijkProject

## StepperMotorController

An Arduino Nano R4 sketch that acts as an **I2C (SMBus) slave** for a DM320T stepper driver.

---

### Hardware wiring

| Signal       | Arduino pin |
|--------------|-------------|
| SDA          | 18          |
| SCL          | 19          |
| EN           | 6           |
| DIR          | 7           |
| OPTO         | 8           |
| PUL          | 9           |
| DIP bit0     | 2           |
| DIP bit1     | 3           |
| DIP bit2     | 4           |
| DIP bit3     | 5           |

The slave address is read at boot as `000` + DIP switches (`pins 2-5` as low 4 bits).

---

### I2C command reference

Slave address: **`000` + DIP[3:0]**

#### Write registers (master → Arduino)

| Register | Name           | Payload                                        |
|----------|----------------|------------------------------------------------|
| `0x00`   | ENABLE         | `0x01` enable, `0x00` disable                  |

#### Read registers (Arduino → master)

| Register | Name           | Response                                       |
|----------|----------------|------------------------------------------------|
| `0x00`   | ENABLE         | `0x01` enabled, `0x00` disabled                |

---

### Example master script (Raspberry Pi / Linux)

See [`StepperMotorController/stepper_master_example.py`](StepperMotorController/stepper_master_example.py) for a Python example using `smbus2`.

```bash
pip install smbus2
python StepperMotorController/stepper_master_example.py
```

---

### Basic usage sequence

1. Write `0x00 = 0x01` to enable the driver.
2. Write `0x00 = 0x00` to disable the driver.