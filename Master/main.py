from src.stepper_i2c import StepperController
import time

try:
    with StepperController(address=15) as controller:
        print("✓ I2C verbinding OK!")
        
        # Test: schakel stepper in
        controller.move_steps(100,100)
        print("✓ stepper moving steps")
        
        controller.wait_until_complete()
        print("✓ stepper movement complete")
        
        time.sleep(2)
        
        controller.move_degrees(90)
        print("✓ stepper moving degrees")
        controller.wait_until_complete()
        print("✓ stepper movement complete")
        # controller.enable(False)
        # print("✓ Stepper disabled")
        
except Exception as e:
    print(f"X Fout: {e}")