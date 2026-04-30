from src.stepper_i2c import StepperController
import time

def main():
    ctrl= StepperController(0)
    ctrl.enable(True)
    time.sleep(1)
    ctrl.enable(False)
    ctrl.close()