"""Command-line HIL scenario runner."""

import argparse
from pathlib import Path

from hil.protocol import HilClient
from hil.runner import ScenarioRunner, load_scenario
from hil.transport import SerialTransport


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("scenario")
    parser.add_argument("--reports", default="hil/reports")
    parser.add_argument("--allow-outputs", action="store_true")
    args = parser.parse_args()
    transport = SerialTransport(args.port)
    try:
        result = ScenarioRunner(HilClient(transport)).run(
            load_scenario(args.scenario), Path(args.reports), args.port, args.allow_outputs
        )
        print("PASS" if result["passed"] else "FAIL")
        return 0 if result["passed"] else 1
    finally:
        transport.close()


if __name__ == "__main__":
    raise SystemExit(main())
