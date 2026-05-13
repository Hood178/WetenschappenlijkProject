"""Simple one-motor test script for the stepper controller."""

import time

from src.stepper_i2c import StepperController


ADDRESS = 0b1111
BUS = 1
STEPS_PER_REV = 400


def print_state(controller: StepperController, label: str) -> None:
    state = controller.get_state()
    direction = "CW" if state["clockwise"] else "CCW"
    print(
        f"{label}: enabled={state['enabled']}, moving={controller.is_moving()}, "
        f"dir={direction}, speed={state['speed_percent']:.1f}%, "
        f"period_us={state['period_us']}, steps_left={state['pulse_count']}, "
        f"continuous={state['is_continuous']}"
    )


def wait_for_motion(controller: StepperController, timeout_sec: float = 15.0) -> None:
    controller.wait_until_complete(timeout_sec=timeout_sec)
    time.sleep(0.5)


try:
    with StepperController(address=ADDRESS, bus=BUS, steps_per_rev=STEPS_PER_REV) as controller:
        print("I2C verbinding OK")

        print("\nTest 1: enable + status")
        controller.enable(True)
        print_state(controller, "Na enable")

        print("\nTest 2: korte beweging vooruit")
        controller.set_speed_percent(25)
        controller.move_steps(100, speed_percent=25, clockwise=True)
        wait_for_motion(controller)
        print_state(controller, "Na 100 stappen vooruit")

        print("\nTest 3: korte beweging achteruit")
        controller.start()
        controller.set_speed_rpm(30)
        controller.rotate(0.25, speed_percent=35, clockwise=False)
        wait_for_motion(controller)
        print_state(controller, "Na 1/4 omwenteling achteruit")

        print("\nTest 4: continu draaien + speed change")
        controller.run_continuous(speed_percent=20, clockwise=True)
        time.sleep(2.0)
        print_state(controller, "Tijdens continu draaien")
        controller.change_speed(20)
        time.sleep(2.0)
        print_state(controller, "Na speed change")
        controller.stop()
        time.sleep(1.0)
        print_state(controller, "Na stop")

        print("\nTest 5: uitschakelen")
        controller.enable(False)
        print_state(controller, "Na disable")

        print("\nKlaar.")

except Exception as e:
    print(f"Fout: {e}")