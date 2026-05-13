from src.stepper_i2c import StepperController
import time

try:
    with StepperController(address=15,steps_per_rev=400) as controller:
        print("✓ I2C verbinding OK!")
        
        controller.move_steps(400,100)
        controller.wait_until_complete()
        controller.move_degrees(360,100,True)
        print("✓ stepper movement complete")
        
except Exception as e:
    print(f"X Fout: {e}")