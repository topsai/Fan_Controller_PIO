import json
import tempfile
import unittest
from pathlib import Path

from hil.runner import ScenarioRunner
from hil.protocol import HilCommandError


class FakeClient:
    def __init__(self, responses=None):
        self.commands = []
        self.responses = list(responses or [])

    def command(self, command, timeout=2.0):
        self.commands.append(command)
        if self.responses:
            response = self.responses.pop(0)
            if isinstance(response, Exception):
                raise response
            return response
        return {"type": "ack", "ok": True, "outputs_unlocked": False}


class ScenarioRunnerTest(unittest.TestCase):
    def test_asserts_nested_fields_and_writes_three_report_formats(self):
        client = FakeClient([{"ok": True, "outputs_unlocked": False, "actual_outputs": {"pwm": 76}}])
        scenario = {
            "version": 1,
            "name": "safe output",
            "steps": [{"command": "STATUS", "expect": {"outputs_unlocked": False, "actual_outputs.pwm": 76}}],
        }
        with tempfile.TemporaryDirectory() as directory:
            result = ScenarioRunner(client).run(scenario, Path(directory), port="COM4")
            self.assertTrue(result["passed"])
            self.assertTrue((Path(directory) / "safe_output.json").exists())
            self.assertTrue((Path(directory) / "safe_output.csv").exists())
            self.assertTrue((Path(directory) / "safe_output.md").exists())
            saved = json.loads((Path(directory) / "safe_output.json").read_text(encoding="utf-8"))
            self.assertEqual("COM4", saved["serial_port"])

    def test_always_locks_outputs_after_failure(self):
        client = FakeClient([RuntimeError("boom")])
        scenario = {"version": 1, "name": "failure", "steps": [{"command": "PING"}]}
        with tempfile.TemporaryDirectory() as directory:
            result = ScenarioRunner(client).run(scenario, Path(directory))
        self.assertFalse(result["passed"])
        self.assertEqual("OUTPUTS LOCK", client.commands[-1])

    def test_rejects_unlock_without_explicit_scenario_permission(self):
        client = FakeClient()
        scenario = {"version": 1, "name": "unsafe", "steps": [{"command": "OUTPUTS UNLOCK"}]}
        with tempfile.TemporaryDirectory() as directory:
            result = ScenarioRunner(client).run(scenario, Path(directory))
        self.assertFalse(result["passed"])
        self.assertNotIn("OUTPUTS UNLOCK", client.commands)

    def test_expected_firmware_error_counts_as_pass(self):
        client = FakeClient([HilCommandError("unknown_command", {"ok": False, "error": "unknown_command"})])
        scenario = {
            "version": 1,
            "name": "invalid command",
            "steps": [{"command": "NOPE", "expect_error": "unknown_command"}],
        }
        with tempfile.TemporaryDirectory() as directory:
            result = ScenarioRunner(client).run(scenario, Path(directory))
        self.assertTrue(result["steps"][0]["passed"])


if __name__ == "__main__":
    unittest.main()
