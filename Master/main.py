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


def draw_circle(x_ctrl: StepperController, y_ctrl: StepperController, segments: int = 16) -> None:
    """Draw a circle approximated by straight line segments.
    
    Args:
        x_ctrl: X-axis motor controller
        y_ctrl: Y-axis motor controller
        segments: Number of segments to divide the circle into (more = smoother)
    """
    print(f"Initial X state: {x_ctrl.get_state()}")
    print(f"Initial Y state: {y_ctrl.get_state()}")

    radius = CIRCLE_RADIUS  # Use small radius for testing
    positions = []
    
    # Calculate circle positions using parametric equations
    for i in range(segments):
        angle = (2 * math.pi * i) / segments
        x = radius * math.cos(angle)
        y = radius * math.sin(angle)
        positions.append((int(x), int(y)))
    
    # Start at first position
    current_x, current_y = 0, 0
    
    try:
        for i, (next_x, next_y) in enumerate(positions):
            delta_x = next_x - current_x
            delta_y = next_y - current_y
            
            x_dir = delta_x >= 0
            y_dir = delta_y >= 0
            
            abs_delta_x = abs(delta_x)
            abs_delta_y = abs(delta_y)
            
            print(f"Segment {i+1}/{segments}: X {delta_x:+4d}, Y {delta_y:+4d} (parallel)")
            
            x_ctrl.move_steps(abs_delta_x, speed_percent=SPEED_PERCENT, clockwise=x_dir)
            y_ctrl.move_steps(abs_delta_y, speed_percent=SPEED_PERCENT, clockwise=y_dir)
            x_ctrl.wait_until_complete(timeout_sec=60)
            y_ctrl.wait_until_complete(timeout_sec=60)
            
            current_x, current_y = next_x, next_y
    finally:
        x_ctrl.stop()
        y_ctrl.stop()

    print(f"Final X state: {x_ctrl.get_state()}")
    print(f"Final Y state: {y_ctrl.get_state()}")


def main() -> None:
    try:
        with StepperController(address=MOTOR_X_ADDRESS, bus=BUS, i2c_retry_count=5) as x_ctrl:
            with StepperController(address=MOTOR_Y_ADDRESS, bus=BUS, i2c_retry_count=5) as y_ctrl:
                print(f"Connected X at address {MOTOR_X_ADDRESS}")
                print(f"Connected Y at address {MOTOR_Y_ADDRESS}")
                draw_circle(x_ctrl, y_ctrl)
    except Exception as exc:
        print("Error:", exc)


if __name__ == "__main__":
    main()