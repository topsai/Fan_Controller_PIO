#!/usr/bin/env python3
"""Serial diagnostic connectivity and stability test.

The test talks to firmware diagnostic commands over USB serial. It injects
simulated control data into the receiver and simulated status data into the
remote, then checks both sides report a connected diagnostic state.
"""

import argparse
import re
import sys
import time

try:
    import serial
except ImportError as exc:
    raise SystemExit("pyserial is required. Run this through PlatformIO's Python or install pyserial.") from exc


SHORT_SECONDS = 10
LONG_SECONDS = 30 * 60
CONTROL_FLAG_TAKEOVER_REQUEST = 0x01
STATUS_RE = re.compile(
    r"DIAG STATUS role=(?P<role>\S+) connected=(?P<connected>[01]) "
    r"rx=(?P<rx>\d+) lost=(?P<lost>\d+) faults=(?P<faults>\d+)"
    r"(?: ignored=(?P<ignored>\d+))?(?: active=(?P<active>\S+))?"
)


def parse_args():
    parser = argparse.ArgumentParser(description="Run remote/receiver diagnostic connectivity tests.")
    parser.add_argument("--receiver-port", required=True, help="Receiver serial port, e.g. COM4")
    parser.add_argument("--remote-port", help="Single remote serial port, e.g. COM3 or COM5")
    parser.add_argument("--remote-a", help="Primary/basic remote serial port for switching tests, e.g. COM5")
    parser.add_argument("--remote-b", help="Secondary/S3 remote serial port for switching tests, e.g. COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=int, default=SHORT_SECONDS, help="Test duration in seconds")
    parser.add_argument("--long", action="store_true", help="Run the explicit 30 minute stability test")
    parser.add_argument("--allow-long", action="store_true", help="Allow custom durations longer than 10 seconds")
    parser.add_argument("--interval", type=float, default=1.0, help="Simulation interval in seconds")
    args = parser.parse_args()
    if args.long:
        args.duration = LONG_SECONDS
        args.allow_long = True
    if args.duration > SHORT_SECONDS and not args.allow_long:
        parser.error("duration > 10 seconds requires --allow-long, or use --long for the 30 minute test")
    if args.remote_port and (args.remote_a or args.remote_b):
        parser.error("--remote-port cannot be combined with --remote-a/--remote-b")
    if not args.remote_port and not (args.remote_a and args.remote_b):
        parser.error("provide --remote-port, or provide both --remote-a and --remote-b")
    return args


class DiagnosticPort:
    def __init__(self, name, port, baud):
        self.name = name
        self.serial = serial.Serial(port, baudrate=baud, timeout=0.15, write_timeout=1)
        self.lines = []

    def close(self):
        self.serial.close()

    def write_line(self, line):
        self.serial.write((line + "\n").encode("utf-8"))
        self.serial.flush()

    def read_available(self, seconds=0.2):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            raw = self.serial.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").strip()
            if text:
                self.lines.append(text)

    def command(self, line, expected_prefix, timeout=2.0):
        start_index = len(self.lines)
        self.write_line(line)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.read_available(0.05)
            for item in reversed(self.lines[start_index:]):
                if item.startswith(expected_prefix):
                    return item
        raise RuntimeError(f"{self.name} did not respond to {line!r}; last lines: {self.lines[-5:]}")

    def status(self):
        line = self.command("DIAG STATUS", "DIAG STATUS")
        match = STATUS_RE.match(line)
        if not match:
            raise RuntimeError(f"{self.name} returned malformed status: {line}")
        parsed = {}
        for key, value in match.groupdict().items():
            if value is None:
                parsed[key] = 0 if key == "ignored" else None
            elif key in ("role", "active"):
                parsed[key] = value
            else:
                parsed[key] = int(value)
        return parsed


def run_single_remote_test(args):
    receiver = DiagnosticPort("receiver", args.receiver_port, args.baud)
    remote = DiagnosticPort("remote", args.remote_port, args.baud)
    try:
        time.sleep(1.5)
        receiver.read_available(0.2)
        remote.read_available(0.2)

        receiver_pong = receiver.command("DIAG PING", "DIAG PONG")
        remote_pong = remote.command("DIAG PING", "DIAG PONG")
        print(receiver_pong)
        print(remote_pong)
        remote_source = "s3" if "role=s3_transmitter" in remote_pong else "c3"

        start = time.monotonic()
        next_tick = start
        samples = 0
        last_receiver_status = None
        last_remote_status = None
        while time.monotonic() - start < args.duration:
            now = time.monotonic()
            if now < next_tick:
                time.sleep(min(0.05, next_tick - now))
                continue
            elapsed = int(now - start)
            throttle = ((elapsed % 5) - 2) * 120
            takeover_flags = CONTROL_FLAG_TAKEOVER_REQUEST if samples == 0 else 0
            inject_receiver_source(receiver, remote_source, throttle=throttle, flags=takeover_flags, packets=3)
            remote.command(f"DIAG SIMSTATUS -55 4800 {elapsed % 80} 2", "DIAG OK")
            last_receiver_status = receiver.status()
            last_remote_status = remote.status()
            if last_receiver_status["connected"] != 1 or last_remote_status["connected"] != 1:
                raise RuntimeError(f"connectivity dropped: receiver={last_receiver_status} remote={last_remote_status}")
            samples += 1
            print(f"sample={samples} receiver={last_receiver_status} remote={last_remote_status}")
            next_tick += args.interval

        if samples == 0:
            raise RuntimeError("no diagnostic samples were collected")
        print(f"PASS duration={args.duration}s samples={samples}")
    finally:
        receiver.close()
        remote.close()


def inject_receiver_source(receiver, source, throttle=0, flags=0, packets=1):
    for _ in range(packets):
        receiver.command(f"DIAG SIMCTRLFROM {source} {throttle} 1 0 {flags}", "DIAG OK")


def require_active(receiver, expected):
    status = receiver.status()
    if status.get("active") != expected:
        raise RuntimeError(f"expected active={expected}, got {status}")
    return status


def run_switching_test(args):
    receiver = DiagnosticPort("receiver", args.receiver_port, args.baud)
    remote_a = DiagnosticPort("remote-a", args.remote_a, args.baud)
    remote_b = DiagnosticPort("remote-b", args.remote_b, args.baud)
    try:
        time.sleep(1.5)
        for port in (receiver, remote_a, remote_b):
            port.read_available(0.2)

        print(receiver.command("DIAG PING", "DIAG PONG"))
        print(remote_a.command("DIAG PING", "DIAG PONG"))
        print(remote_b.command("DIAG PING", "DIAG PONG"))

        def status_remotes(speed):
            remote_a.command(f"DIAG SIMSTATUS -55 4800 {speed} 2", "DIAG OK")
            remote_b.command(f"DIAG SIMSTATUS -58 4800 {speed} 2", "DIAG OK")
            a_status = remote_a.status()
            b_status = remote_b.status()
            if a_status["connected"] != 1 or b_status["connected"] != 1:
                raise RuntimeError(f"remote diagnostic connection dropped: A={a_status} B={b_status}")

        start = time.monotonic()
        next_tick = start
        samples = 0
        active = "c3"
        inject_receiver_source(receiver, active, throttle=120, packets=3)
        require_active(receiver, active)
        status_remotes(0)

        while time.monotonic() - start < args.duration:
            now = time.monotonic()
            if now < next_tick:
                time.sleep(min(0.05, next_tick - now))
                continue

            challenger = "s3" if active == "c3" else "c3"
            inject_receiver_source(receiver, challenger, throttle=240, packets=1)
            locked_status = require_active(receiver, active)
            inject_receiver_source(receiver, challenger, throttle=0, flags=CONTROL_FLAG_TAKEOVER_REQUEST, packets=3)
            switched_status = require_active(receiver, challenger)
            status_remotes(samples % 80)

            samples += 1
            print(f"switch={samples} locked={locked_status} switched={switched_status}")
            active = challenger
            next_tick += args.interval

        if samples == 0:
            raise RuntimeError("no switching samples were collected")
        print(f"PASS switching duration={args.duration}s samples={samples}")
    finally:
        receiver.close()
        remote_a.close()
        remote_b.close()


def main():
    try:
        args = parse_args()
        if args.remote_port:
            run_single_remote_test(args)
        else:
            run_switching_test(args)
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
