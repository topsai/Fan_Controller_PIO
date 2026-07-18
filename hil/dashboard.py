"""Tkinter desktop dashboard for all Fan Controller HIL roles."""

import argparse
import json
import queue
import threading
from datetime import datetime
from pathlib import Path

from hil.protocol import HilClient
from hil.runner import ScenarioRunner, load_scenario
from hil.transport import SerialTransport, available_ports


STATUS_FIELDS = {
    "firmware", "protocol", "role", "uptime_ms", "last_sequence", "last_result",
    "outputs_unlocked", "watchdog_remaining_ms", "connected", "failsafe",
    "protocol_fault", "active_controller", "stable_packets", "remote", "vesc",
    "diagnostics", "expected_outputs", "actual_outputs", "armed", "settings_mode",
    "input_modes", "joystick", "speed_level", "buttons", "receiver", "battery",
    "sensors", "display",
}


def disconnected_snapshot():
    return {field: None for field in STATUS_FIELDS}


class ConnectionController:
    def __init__(self, transport_factory=SerialTransport):
        self.transport_factory = transport_factory
        self.transport = None
        self.client = None
        self.port = None
        self.snapshot = disconnected_snapshot()

    @property
    def connected(self):
        return self.transport is not None

    def connect(self, port):
        self.disconnect()
        self.transport = self.transport_factory(port)
        self.client = HilClient(self.transport)
        self.port = port

    def disconnect(self):
        if self.client is not None:
            try:
                self.client.command("OUTPUTS LOCK", timeout=0.5)
            except Exception:
                pass
        if self.transport is not None:
            self.transport.close()
        self.transport = None
        self.client = None
        self.port = None
        self.snapshot = disconnected_snapshot()


class Dashboard:
    def __init__(self, root, initial_port=None):
        import tkinter as tk
        from tkinter import messagebox, ttk

        self.tk = tk
        self.ttk = ttk
        self.messagebox = messagebox
        self.root = root
        self.connection = ConnectionController()
        self.events = queue.Queue()
        self.stop_event = threading.Event()
        self.scenario_running = False
        self.labels = {}
        self.history = {"time": [], "pwm": [], "speed": [], "temperature": [], "faults": []}
        self.root.title("遥控器 HIL 实时测试")
        self.root.geometry("1280x820")
        self.root.minsize(980, 680)
        self._build(initial_port)
        self.refresh_ports()
        if initial_port:
            self.port_var.set(initial_port)
            self.connect()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(200, self._drain_events)
        self.root.after(1000, self._poll)

    def _build(self, initial_port):
        ttk = self.ttk
        top = ttk.Frame(self.root, padding=8)
        top.pack(fill="x")
        self.port_var = self.tk.StringVar(value=initial_port or "")
        self.port_box = ttk.Combobox(top, textvariable=self.port_var, width=14, state="readonly")
        self.port_box.pack(side="left")
        ttk.Button(top, text="刷新端口", command=self.refresh_ports).pack(side="left", padx=4)
        ttk.Button(top, text="连接", command=self.connect).pack(side="left", padx=4)
        ttk.Button(top, text="断开", command=self.disconnect).pack(side="left")
        self.banner = self.tk.Label(top, text="未连接 / 输出已锁定", bg="#555", fg="white", padx=12, pady=5)
        self.banner.pack(side="left", padx=12)
        self.meta = ttk.Label(top, text="固件：不可用  HIL：不可用  最后响应：不可用")
        self.meta.pack(side="left", fill="x", expand=True)

        notebook = ttk.Notebook(self.root)
        notebook.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        live = ttk.Frame(notebook, padding=8)
        controls = ttk.Frame(notebook, padding=8)
        scenarios = ttk.Frame(notebook, padding=8)
        notebook.add(live, text="实时状态")
        notebook.add(controls, text="输入与安全")
        notebook.add(scenarios, text="自动场景")
        self._build_live(live)
        self._build_controls(controls)
        self._build_scenarios(scenarios)

    def _build_live(self, parent):
        fields = [
            ("role", "固件角色"), ("connected", "业务连接"), ("failsafe", "失控保护"),
            ("active_controller", "活动遥控器"), ("armed", "遥控器解锁"),
            ("joystick", "摇杆"), ("speed_level", "档位"), ("receiver", "接收器状态"),
            ("vesc", "VESC"), ("sensors", "本地传感器"), ("diagnostics", "诊断计数"),
            ("expected_outputs", "期望输出"), ("actual_outputs", "实际输出"),
        ]
        info = self.ttk.LabelFrame(parent, text="设备状态", padding=8)
        info.pack(side="left", fill="y")
        for row, (key, title) in enumerate(fields):
            self.ttk.Label(info, text=title, width=14).grid(row=row, column=0, sticky="w", pady=2)
            label = self.ttk.Label(info, text="不可用", width=48, wraplength=360)
            label.grid(row=row, column=1, sticky="w", pady=2)
            self.labels[key] = label
        chart_frame = self.ttk.LabelFrame(parent, text="实时曲线", padding=4)
        chart_frame.pack(side="left", fill="both", expand=True, padx=(8, 0))
        try:
            from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
            from matplotlib.figure import Figure
            figure = Figure(figsize=(7, 5), dpi=100)
            self.axis = figure.add_subplot(111)
            self.axis.set_xlabel("时间 / s")
            self.lines = {name: self.axis.plot([], [], label=name)[0] for name in self.history if name != "time"}
            self.axis.legend(loc="upper left")
            self.canvas = FigureCanvasTkAgg(figure, master=chart_frame)
            self.canvas.get_tk_widget().pack(fill="both", expand=True)
        except ImportError:
            self.axis = self.canvas = None
            self.lines = {}
            self.ttk.Label(chart_frame, text="未安装 matplotlib，状态功能仍可使用").pack(pady=30)

    def _build_controls(self, parent):
        safety = self.ttk.LabelFrame(parent, text="输出安全", padding=8)
        safety.pack(fill="x")
        self.ttk.Button(safety, text="锁定输出", command=lambda: self.send("OUTPUTS LOCK")).pack(side="left")
        self.ttk.Button(safety, text="解锁输出", command=self.unlock).pack(side="left", padx=6)
        groups = [
            ("接收器帧", ["REMOTE CONTROL c3 0 1 0 0", "REMOTE CONTROL c3 -1000 1 0 0",
                         "REMOTE CONTROL s3 0 1 0 1", "REMOTE REPEAT 3", "VESC VALUE 4800 12000", "VESC FAULT"]),
            ("遥控器输入", ["INPUT JOYSTICK 2048", "INPUT JOYSTICK 0", "INPUT JOYSTICK 4095",
                           "INPUT SPEED 1", "INPUT SPEED 2", "INPUT SPEED 3",
                           "BUTTON 1 CLICK", "BUTTON 1 HOLD 3000", "BUTTON 2 CLICK"]),
            ("S3 传感器", ["SENSOR MCU VALUE 43.2", "SENSOR MCU FAULT", "SENSOR BMP280 VALUE 25.0",
                          "SENSOR COMPASS VALUE 90", "SENSOR CW2015 VALUE 4.1"]),
        ]
        for title, commands in groups:
            frame = self.ttk.LabelFrame(parent, text=title, padding=8)
            frame.pack(fill="x", pady=6)
            for command in commands:
                self.ttk.Button(frame, text=command, command=lambda value=command: self.send(value)).pack(side="left", padx=2, pady=2)
        raw = self.ttk.LabelFrame(parent, text="自定义命令", padding=8)
        raw.pack(fill="x")
        self.command_var = self.tk.StringVar()
        self.ttk.Entry(raw, textvariable=self.command_var).pack(side="left", fill="x", expand=True)
        self.ttk.Button(raw, text="发送", command=lambda: self.send(self.command_var.get().strip())).pack(side="left", padx=4)

    def _build_scenarios(self, parent):
        row = self.ttk.Frame(parent)
        row.pack(fill="x")
        self.scenario_var = self.tk.StringVar()
        self.scenario_box = self.ttk.Combobox(row, textvariable=self.scenario_var, state="readonly", width=60)
        self.scenario_box.pack(side="left")
        self.ttk.Button(row, text="运行", command=self.run_scenario).pack(side="left", padx=4)
        self.ttk.Button(row, text="停止", command=self.stop_event.set).pack(side="left")
        self.progress_label = self.ttk.Label(row, text="未运行")
        self.progress_label.pack(side="left", padx=12)
        self.log = self.tk.Text(parent, height=28, wrap="none")
        self.log.pack(fill="both", expand=True, pady=(8, 0))
        paths = sorted(str(path) for path in Path("hil/scenarios").glob("*.json"))
        self.scenario_box.configure(values=paths)
        if paths:
            self.scenario_var.set(paths[0])

    def refresh_ports(self):
        ports = available_ports()
        self.port_box.configure(values=ports)
        if not self.port_var.get() and ports:
            self.port_var.set(ports[0])

    def connect(self):
        port = self.port_var.get()
        if not port:
            return
        try:
            self.connection.connect(port)
            self.connection.client.on_exchange = self._on_exchange
            self.banner.configure(text="已连接 / 正在读取", bg="#2266aa")
            self._log({"event": "connected", "port": port})
        except Exception as error:
            self.connection.disconnect()
            self._log({"error": str(error)})

    def disconnect(self):
        self.connection.disconnect()
        self._render(disconnected_snapshot())
        self._log({"event": "disconnected"})

    def unlock(self):
        if self.connection.connected and self.messagebox.askyesno(
            "确认真实输出", "确认已断开电机/危险负载并允许真实输出？"
        ):
            self.send("OUTPUTS UNLOCK")

    def send(self, command):
        if not command or not self.connection.connected:
            self._log({"error": "未连接串口"})
            return None
        try:
            response = self.connection.client.command(command)
            if response.get("type") == "status":
                self.connection.snapshot = response
                self._render(response)
            return response
        except Exception as error:
            self._log({"error": str(error), "command": command})
            return None

    def run_scenario(self):
        if not self.connection.connected or self.scenario_running or not self.scenario_var.get():
            return
        scenario = load_scenario(self.scenario_var.get())
        allow_outputs = False
        if scenario.get("requires_outputs"):
            allow_outputs = self.messagebox.askyesno("危险场景", "场景要求真实输出，确认低压安全接线？")
            if not allow_outputs:
                return
        self.stop_event.clear()
        self.scenario_running = True
        threading.Thread(target=self._scenario_worker, args=(scenario, allow_outputs), daemon=True).start()

    def _scenario_worker(self, scenario, allow_outputs):
        try:
            runner = ScenarioRunner(self.connection.client, self.events.put, self.stop_event.is_set)
            result = runner.run(scenario, Path("hil/reports"), self.connection.port, allow_outputs)
            self.events.put({"event": "scenario_result", "result": result})
        except Exception as error:
            self.events.put({"error": str(error)})
        finally:
            self.scenario_running = False

    def _poll(self):
        if self.connection.connected and not self.scenario_running:
            response = self.send("STATUS")
            if response:
                self.meta.configure(text=f"固件：{response.get('firmware', '不可用')}  HIL：{response.get('protocol', '不可用')}  最后响应：{datetime.now().strftime('%H:%M:%S')}")
        self.root.after(1000, self._poll)

    def _render(self, snapshot):
        unlocked = snapshot.get("outputs_unlocked") is True
        if snapshot.get("role") is None:
            self.banner.configure(text="未连接 / 输出已锁定", bg="#555")
            self.meta.configure(text="固件：不可用  HIL：不可用  最后响应：不可用")
            self._clear_chart()
        else:
            self.banner.configure(text="输出已解锁" if unlocked else "输出已锁定", bg="#b3261e" if unlocked else "#257a3e")
            self._update_chart(snapshot)
        for key, label in self.labels.items():
            value = snapshot.get(key)
            label.configure(text="不可用" if value is None else json.dumps(value, ensure_ascii=False) if isinstance(value, dict) else str(value))

    def _update_chart(self, snapshot):
        if self.canvas is None:
            return
        self.history["time"].append(float(snapshot.get("uptime_ms", 0)) / 1000.0)
        expected = snapshot.get("expected_outputs") or {}
        sensors = snapshot.get("sensors") or {}
        diagnostics = snapshot.get("diagnostics") or {}
        values = {
            "pwm": expected.get("pwm"), "speed": snapshot.get("speed_level") or (snapshot.get("remote") or {}).get("speed_level"),
            "temperature": sensors.get("mcu_temperature"), "faults": diagnostics.get("faults"),
        }
        for name, value in values.items():
            self.history[name].append(float("nan") if value is None else float(value))
            self.lines[name].set_data(self.history["time"][-300:], self.history[name][-300:])
        self.axis.relim()
        self.axis.autoscale_view()
        self.canvas.draw_idle()

    def _clear_chart(self):
        for values in self.history.values():
            values.clear()
        if self.canvas:
            for line in self.lines.values():
                line.set_data([], [])
            self.canvas.draw_idle()

    def _on_exchange(self, request, raw, response):
        self.events.put({"request": request, "response": raw})

    def _drain_events(self):
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                break
            if event.get("event") == "step":
                self.progress_label.configure(text=f"步骤 {event['index']}：{'通过' if event['passed'] else '失败'}")
            elif event.get("event") == "scenario_result":
                result = event["result"]
                self.progress_label.configure(text=f"{'通过' if result['passed'] else '失败'}，{result['duration_ms']} ms")
            self._log(event)
        self.root.after(200, self._drain_events)

    def _log(self, value):
        self.log.insert("end", json.dumps(value, ensure_ascii=False, default=str) + "\n")
        self.log.see("end")

    def close(self):
        self.stop_event.set()
        self.connection.disconnect()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description="Fan Controller HIL desktop dashboard")
    parser.add_argument("--port", help="optional serial port, for example COM4")
    args = parser.parse_args()
    import tkinter as tk

    root = tk.Tk()
    Dashboard(root, args.port)
    root.mainloop()


if __name__ == "__main__":
    main()
