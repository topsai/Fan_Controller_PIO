"""PySerial transport with deterministic line filtering and timeouts."""

import time


class SerialTransport:
    def __init__(self, port, baudrate=115200):
        import serial

        self.port = port
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=0.1, write_timeout=1.0)

    def exchange(self, line, timeout=2.0):
        self.serial.reset_input_buffer()
        self.serial.write((line + "\n").encode("ascii"))
        self.serial.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.serial.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").strip()
            if text.startswith("{") and text.endswith("}"):
                return text
        raise TimeoutError(f"no HIL response within {timeout:.1f}s for {line!r}")

    def close(self):
        if self.serial.is_open:
            self.serial.close()


def available_ports():
    try:
        from serial.tools import list_ports
    except ImportError:
        return []
    return [port.device for port in list_ports.comports()]
