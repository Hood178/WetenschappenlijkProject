from src.stepper_i2c import StepperController
import time

try:
    with StepperController(address=15,steps_per_rev=400) as controller:
        print("✓ I2C verbinding OK!")
        
        # Test 1: Draai CW
        print("\n>>> Test 1: Draai 360° CLOCKWISE")
        controller.move_degrees(360, 50, clockwise=True)
        controller.wait_until_complete()
        print("✓ Klaar met CW draai")
        
        # Stop en wacht even
        time.sleep(1)
        
        # Test 2: Draai CCW
        print("\n>>> Test 2: Draai 360° COUNTER-CLOCKWISE")
        controller.move_degrees(360, 50, clockwise=False)
        controller.wait_until_complete()
        print("✓ Klaar met CCW draai")
        
        print("\n✓ Beide richtingen getest!")
        
except Exception as e:
    print(f"X Fout: {e}")