"""Quick sequential test that pings two addresses: 0x0e and 0x0f."""

from typing import Iterable

from src.stepper_i2c import StepperController


BUS = 1


def run_for_addresses(addrs: Iterable[str]) -> None:
    for addr in addrs:
        print(f"\n--- Testing address {addr} ---")
        try:
            with StepperController(address=addr, bus=BUS, i2c_retry_count=5) as ctrl:
                print("Connected")
                ctrl.move_degrees(2*360,100)
                ctrl.wait_until_complete()
                ctrl.move_degrees(2*360,100,False)
                ctrl.wait_until_complete()
                ctrl.move_degrees(2*360,100)
                ctrl.wait_until_complete()
                ctrl.move_degrees(2*360,100,False)
                ctrl.wait_until_complete()
                try:
                    ctrl.wait_until_complete(timeout_sec=30)
                except TimeoutError:
                    print("Move timeout")
                print("State:", ctrl.get_state())
                ctrl.enable(False)
        except Exception as e:
            print("Error for", addr, "->", e)


if __name__ == '__main__':
    # provide binary strings so intent is clear
    addresses = ["1110", "1111"]
    run_for_addresses(addresses)