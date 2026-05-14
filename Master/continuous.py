

from src.stepper_i2c import StepperController


while True:
    ctrl = StepperController("1000")
    ctrl.start()
    ctrl.set_speed_percent(100.0)

ctrl.stop()





