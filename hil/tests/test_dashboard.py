import unittest

from hil.dashboard import ConnectionController, STATUS_FIELDS, disconnected_snapshot


class FakeTransport:
    def close(self):
        pass


class DashboardStateTest(unittest.TestCase):
    def test_initialization_does_not_open_a_serial_port(self):
        opened = []
        controller = ConnectionController(lambda port: opened.append(port) or FakeTransport())
        self.assertFalse(controller.connected)
        self.assertEqual([], opened)

    def test_disconnected_snapshot_has_all_fields_without_fake_values(self):
        snapshot = disconnected_snapshot()
        self.assertTrue(STATUS_FIELDS.issubset(snapshot))
        self.assertTrue(all(value is None for value in snapshot.values()))

    def test_disconnect_keeps_controller_but_clears_live_snapshot(self):
        controller = ConnectionController(lambda port: FakeTransport())
        controller.connect("COM4")
        controller.snapshot = {"connected": True, "role": "receiver"}
        controller.disconnect()
        self.assertFalse(controller.connected)
        self.assertIsNone(controller.snapshot["connected"])
        self.assertIn("role", controller.snapshot)


if __name__ == "__main__":
    unittest.main()
