"""Versioned scenario execution and JSON/CSV/Markdown reports."""

import csv
import json
import time
from datetime import datetime, timezone
from pathlib import Path

from hil.protocol import HilCommandError


def load_scenario(path):
    scenario = json.loads(Path(path).read_text(encoding="utf-8"))
    if scenario.get("version") != 1 or not isinstance(scenario.get("steps"), list):
        raise ValueError("unsupported or malformed scenario")
    return scenario


def nested_value(response, path):
    value = response
    for key in path.split("."):
        if not isinstance(value, dict) or key not in value:
            raise KeyError(path)
        value = value[key]
    return value


class ScenarioRunner:
    def __init__(self, client, progress=None, stop_requested=None):
        self.client = client
        self.progress = progress or (lambda event: None)
        self.stop_requested = stop_requested or (lambda: False)

    def run(self, scenario, report_directory, port=None, allow_outputs=False):
        started_wall = datetime.now(timezone.utc)
        started = time.monotonic()
        steps = []
        cleanup = {"locked": False, "error": None}
        scenario_allows_outputs = bool(scenario.get("requires_outputs")) and allow_outputs
        try:
            for index, definition in enumerate(scenario["steps"], start=1):
                if self.stop_requested():
                    raise RuntimeError("cancelled")
                step_started = time.monotonic()
                response = None
                failures = []
                try:
                    command = definition.get("command")
                    if command == "OUTPUTS UNLOCK" and not scenario_allows_outputs:
                        raise PermissionError("scenario has no confirmed output permission")
                    if "wait_seconds" in definition:
                        self._wait(float(definition["wait_seconds"]), float(definition.get("heartbeat_seconds", 2.0)))
                    elif command:
                        response = self.client.command(command, timeout=float(definition.get("timeout", 2.0)))
                        if definition.get("expect_error"):
                            failures.append(f"expected error {definition['expect_error']!r}, command succeeded")
                        for path, expected in definition.get("expect", {}).items():
                            actual = nested_value(response, path)
                            if actual != expected:
                                failures.append(f"{path}: expected {expected!r}, got {actual!r}")
                    else:
                        raise ValueError("step requires command or wait_seconds")
                except HilCommandError as error:
                    if definition.get("expect_error") != error.error:
                        failures.append(str(error))
                    response = error.response
                except Exception as error:
                    failures.append(str(error))
                item = {
                    "index": index,
                    "definition": definition,
                    "response": response,
                    "assertions": definition.get("expect", {}),
                    "passed": not failures,
                    "failures": failures,
                    "duration_ms": round((time.monotonic() - step_started) * 1000, 3),
                }
                steps.append(item)
                self.progress({"event": "step", **item})
                if failures:
                    break
        finally:
            try:
                self.client.command("OUTPUTS LOCK", timeout=1.0)
                cleanup["locked"] = True
            except Exception as error:
                cleanup["error"] = str(error)

        result = {
            "project": "Fan_Controller_PIO",
            "scenario": scenario["name"],
            "scenario_version": scenario["version"],
            "serial_port": port,
            "started_utc": started_wall.isoformat(),
            "ended_utc": datetime.now(timezone.utc).isoformat(),
            "duration_ms": round((time.monotonic() - started) * 1000, 3),
            "passed": len(steps) == len(scenario["steps"]) and all(step["passed"] for step in steps) and cleanup["locked"],
            "steps": steps,
            "cleanup": cleanup,
        }
        self._write_reports(result, Path(report_directory))
        return result

    def _wait(self, seconds, heartbeat):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if self.stop_requested():
                raise RuntimeError("cancelled")
            remaining = deadline - time.monotonic()
            time.sleep(min(heartbeat, max(0.0, remaining)))
            if time.monotonic() < deadline:
                self.client.command("PING")

    @staticmethod
    def _write_reports(result, directory):
        directory.mkdir(parents=True, exist_ok=True)
        stem = result["scenario"].replace(" ", "_").lower()
        (directory / f"{stem}.json").write_text(
            json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        with (directory / f"{stem}.csv").open("w", newline="", encoding="utf-8-sig") as handle:
            writer = csv.writer(handle)
            writer.writerow(["step", "command", "duration_ms", "result", "failure"])
            for step in result["steps"]:
                writer.writerow([
                    step["index"], step["definition"].get("command", "wait"), step["duration_ms"],
                    "PASS" if step["passed"] else "FAIL", "; ".join(step["failures"]),
                ])
        lines = [
            f"# {result['scenario']}", "",
            f"- 结果：{'通过' if result['passed'] else '失败'}",
            f"- 串口：{result['serial_port'] or '未记录'}",
            f"- 开始：{result['started_utc']}",
            f"- 耗时：{result['duration_ms']} ms", "", "| 步骤 | 命令 | 结果 | 耗时 | 失败原因 |",
            "|---:|---|---|---:|---|",
        ]
        for step in result["steps"]:
            lines.append(
                f"| {step['index']} | `{step['definition'].get('command', 'wait')}` | "
                f"{'PASS' if step['passed'] else 'FAIL'} | {step['duration_ms']} ms | "
                f"{'; '.join(step['failures'])} |"
            )
        lines.extend(["", f"清理锁定：{'成功' if result['cleanup']['locked'] else '失败'}"])
        (directory / f"{stem}.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
