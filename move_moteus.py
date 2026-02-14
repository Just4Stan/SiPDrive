#!/usr/bin/env python3

import argparse
import asyncio
import moteus


async def run(args):
    controller = moteus.Controller(id=args.id)

    # Clear any prior fault/timeout before motion commands.
    await controller.set_stop()

    print(f"Moving servo {args.id} to {args.target} rev")
    result = await controller.set_position_wait_complete(
        position=args.target,
        velocity_limit=args.velocity_limit,
        accel_limit=args.accel_limit,
    )
    print(
        "Reached target:",
        result.values.get(moteus.Register.POSITION),
    )

    if args.hold_s > 0.0:
        await asyncio.sleep(args.hold_s)

    print(f"Returning servo {args.id} to {args.home} rev")
    result = await controller.set_position_wait_complete(
        position=args.home,
        velocity_limit=args.velocity_limit,
        accel_limit=args.accel_limit,
    )
    print(
        "Reached home:",
        result.values.get(moteus.Register.POSITION),
    )

    await controller.set_stop()
    print("Done")


def main():
    parser = argparse.ArgumentParser(
        description="Move a moteus servo to a target position and back."
    )
    parser.add_argument("--id", type=int, default=1, help="moteus CAN ID (default: 1)")
    parser.add_argument(
        "--target",
        type=float,
        default=0.5,
        help="target position in revolutions (default: 0.5)",
    )
    parser.add_argument(
        "--home",
        type=float,
        default=0.0,
        help="home position in revolutions (default: 0.0)",
    )
    parser.add_argument(
        "--velocity-limit",
        type=float,
        default=1.0,
        help="velocity limit in rev/s (default: 1.0)",
    )
    parser.add_argument(
        "--accel-limit",
        type=float,
        default=2.0,
        help="acceleration limit in rev/s^2 (default: 2.0)",
    )
    parser.add_argument(
        "--hold-s",
        type=float,
        default=0.3,
        help="time to pause at target in seconds (default: 0.3)",
    )
    args = parser.parse_args()

    asyncio.run(run(args))


if __name__ == "__main__":
    main()
