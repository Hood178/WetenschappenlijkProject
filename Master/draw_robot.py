"""Two-motor demo that checks both controllers separately and draws shapes."""

from src.stepper_i2c import StepperController
import math


BUS = 1
MOTOR_X_ADDRESS = "1000"
MOTOR_Y_ADDRESS = "0001"
SQUARE_SIDE_STEPS = 800
CIRCLE_RADIUS = 100
SPEED_PERCENT = 100.0


def move_axis(ctrl: StepperController, label: str, steps: int, clockwise: bool) -> None:
    direction = "CW" if clockwise else "CCW"
    print(f"{label}: move {steps} steps {direction}")
    ctrl.move_steps(steps, speed_percent=SPEED_PERCENT, clockwise=clockwise)
    ctrl.wait_until_complete(timeout_sec=60)


def draw_square(x_ctrl: StepperController, y_ctrl: StepperController) -> None:
    print("Initial X state:", x_ctrl.get_state())
    print("Initial Y state:", y_ctrl.get_state())

    try:
        move_axis(x_ctrl, "X", SQUARE_SIDE_STEPS, True)
        move_axis(y_ctrl, "Y", SQUARE_SIDE_STEPS, True)
        move_axis(x_ctrl, "X", SQUARE_SIDE_STEPS, False)
        move_axis(y_ctrl, "Y", SQUARE_SIDE_STEPS, False)
    finally:
        x_ctrl.stop()
        y_ctrl.stop()

    print("Final X state:", x_ctrl.get_state())
    print("Final Y state:", y_ctrl.get_state())


def draw_rhombus(x_ctrl: StepperController, y_ctrl: StepperController) -> None:
    """Draw a rhombus/diamond with both motors running in parallel."""
    print("Initial X state:", x_ctrl.get_state())
    print("Initial Y state:", y_ctrl.get_state())

    half_side = max(1, SQUARE_SIDE_STEPS // 2)

    try:
        print("Segment 1: X+ Y+ (parallel)")
        x_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=True)
        y_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=True)
        x_ctrl.wait_until_complete(timeout_sec=60)
        y_ctrl.wait_until_complete(timeout_sec=60)

        print("Segment 2: X+ Y- (parallel)")
        x_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=True)
        y_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=False)
        x_ctrl.wait_until_complete(timeout_sec=60)
        y_ctrl.wait_until_complete(timeout_sec=60)

        print("Segment 3: X- Y- (parallel)")
        x_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=False)
        y_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=False)
        x_ctrl.wait_until_complete(timeout_sec=60)
        y_ctrl.wait_until_complete(timeout_sec=60)

        print("Segment 4: X- Y+ (parallel)")
        x_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=False)
        y_ctrl.move_steps(half_side, speed_percent=SPEED_PERCENT, clockwise=True)
        x_ctrl.wait_until_complete(timeout_sec=60)
        y_ctrl.wait_until_complete(timeout_sec=60)
    finally:
        x_ctrl.stop()
        y_ctrl.stop()

    print("Final X state:", x_ctrl.get_state())
    print("Final Y state:", y_ctrl.get_state())


def main() -> None:
    try:
        with StepperController(address=MOTOR_X_ADDRESS, bus=BUS, i2c_retry_count=5) as x_ctrl:
            with StepperController(address=MOTOR_Y_ADDRESS, bus=BUS, i2c_retry_count=5) as y_ctrl:
                print(f"Connected X at address {MOTOR_X_ADDRESS}")
                print(f"Connected Y at address {MOTOR_Y_ADDRESS}")
                draw_rhombus(x_ctrl, y_ctrl)
    except Exception as exc:
        print("Error:", exc)


if __name__ == "__main__":
    main()