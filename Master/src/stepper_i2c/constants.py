""""Constants for the stepper motor I2C controller."""
BASE_I2C_ADDRESS = 0x20
I2C_BUS = 1

"""Stepper motor parameters"""
STEPS_PER_REV = 200
MIN_PERIOD_US = 1000
MAX_PERIOD_US = 65535

"""Register addresses (all 1 byte except where noted)."""
REG_ENABLE           = 0x00  # 1 byte: 0=disable, 1=enable
REG_DIRECTION        = 0x01  # 1 byte: 0=forward, 1=reverse
REG_PERIOD_US_H      = 0x02  # 2 bytes: step period in microseconds (big-endian)
REG_PERIOD_US_L      = 0x03  # 2 bytes: step period in microseconds (big-endian)
REG_PCOUNT_H         = 0x04  # 2 bytes: pulse count (big-endian)
REG_PCOUNT_L         = 0x05  # 2 bytes: pulse count (big-endian)
MOTION_COMPLETE_FLAG = 0x06  # 1 byte: 0=not complete, 1=complete