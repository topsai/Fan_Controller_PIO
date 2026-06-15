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
STATUS_RE = re.compile(
    r"DIAG STATUS role=(?P<role>\S+) connected=(?P<connected>[01]) "
    r"rx=(?P<rx>\d+) lost=(?P<lost>\d+) faults=(?P<faults>\d+)"
)


def parse_args():
    parser = argparse.ArgumentParser(description="Run remote/receiver diagnostic connectivity tests.")
    parser.add_argument("--receiver-port", required=True, help="Receiver serial port, e.g. COM4")
    parser.add_argument("--remote-port", required=True, help="Remote serial port, e.g. COM3 or COM7")
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
        return {key: int(value) if key != "role" else value for key, value in match.groupdict().items()}


def run_test(args):
    receiver = DiagnosticPort("receiver", args.receiver_port, args.baud)
    remote = DiagnosticPort("remote", args.remote_port, args.baud)
    try:
        time.sleep(1.5)
        receiver.read_available(0.2)
        remote.read_available(0.2)

        print(receiver.command("DIAG PING", "DIAG PONG"))
        print(remote.command("DIAG PING", "DIAG PONG"))

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
            receiver.command(f"DIAG SIMCTRL {throttle} 1 0 0", "DIAG OK")
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


def main():
    try:
        run_test(parse_args())
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
