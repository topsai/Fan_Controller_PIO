import unittest
from pathlib import Path


class BuildIsolationTest(unittest.TestCase):
    def test_platformio_defines_separate_hil_environments(self):
        config = Path("platformio.ini").read_text(encoding="utf-8")
        for role in (
            "receiver_hil", "transmitter_hil", "s3_transmitter_hil",
            "s3_transmitter_new_pcb_hil",
        ):
            self.assertIn(f"[env:{role}]", config)
        self.assertIn("-DFAN_CONTROLLER_HIL=1", config)

    def test_built_formal_images_do_not_contain_hil_markers(self):
        markers = (b"line_too_long", b"outputs_unlocked", b"fan-controller-receiver")
        for environment in ("transmitter", "receiver", "s3_transmitter", "s3_transmitter_new_pcb"):
            image = Path(f".pio/build/{environment}/firmware.bin")
            if not image.exists():
                self.skipTest("firmware images require PlatformIO builds")
            data = image.read_bytes()
            for marker in markers:
                self.assertNotIn(marker, data, f"{environment} leaked {marker!r}")

    def test_built_hil_images_contain_protocol_markers(self):
        markers = (b"line_too_long", b"outputs_unlocked", b"invalid_argument")
        for environment in (
            "transmitter_hil", "receiver_hil", "s3_transmitter_hil",
            "s3_transmitter_new_pcb_hil",
        ):
            image = Path(f".pio/build/{environment}/firmware.bin")
            if not image.exists():
                self.skipTest("firmware images require PlatformIO builds")
            data = image.read_bytes()
            for marker in markers:
                self.assertIn(marker, data, f"{environment} misses {marker!r}")


if __name__ == "__main__":
    unittest.main()
