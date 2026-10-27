#!/usr/bin/env python3
"""TASK-1411: reproduce selected traces using the actual pinned SAT PWM class.

Stdlib only, read-only, no Home Assistant, network or physical appliance. Import
statements are removed from the source AST; dependency names and input records
are supplied below. The PWM methods and their control flow are not rewritten.
"""
from __future__ import annotations

import argparse
import ast
from dataclasses import dataclass
import datetime as dt
from enum import Enum
import hashlib
import json
import logging
from pathlib import Path
import sys
import types
from typing import Optional, Tuple

PIN = "7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb"


class PWMStatus(Enum):
    IDLE = "idle"
    ON = "on"
    OFF = "off"


def load_reference(repo: Path):
    root = repo / "other-projects/SAT-releases-thermo-nova"
    marker = (root / "UPSTREAM_COMMIT").read_text(encoding="utf-8").split()[0]
    if marker != PIN:
        raise ValueError(f"Reference pin changed: expected {PIN}, found {marker}")
    path = root / "custom_components/sat/pwm.py"
    source = path.read_text(encoding="utf-8")
    constants_path = root / "custom_components/sat/const.py"
    constants_tree = ast.parse(constants_path.read_text(encoding="utf-8"))
    startup_seconds = next(ast.literal_eval(node.value) for node in constants_tree.body
        if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name)
        and target.id == "HEATER_STARTUP_TIMEFRAME" for target in node.targets))
    if startup_seconds != 180:
        raise ValueError(f"Reference startup constant changed: {startup_seconds}")
    tree = ast.parse(source, filename=str(path))
    tree.body = [node for node in tree.body if not isinstance(node, (ast.Import, ast.ImportFrom))]
    module = types.ModuleType("_sat_reference_pwm")
    # The type-checking-only import block in the original source stays inactive.
    module.__dict__.update(logging=logging, dataclass=dataclass, Optional=Optional,
        Tuple=Tuple, TYPE_CHECKING=False, State=object, DeviceState=object,
        PwmConfig=object, HeatingSystem=object, PWMStatus=PWMStatus,
        CycleClassification=types.SimpleNamespace(OVERSHOOT="overshoot", UNDERHEAT="underheat"),
        CycleControlMode=types.SimpleNamespace(CONTINUOUS="continuous", PWM="pwm"),
        timestamp=lambda: 0.0, HEATER_STARTUP_TIMEFRAME=startup_seconds)
    sys.modules[module.__name__] = module
    exec(compile(tree, str(path), "exec"), module.__dict__)
    return module.PWM, path, constants_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--output", type=Path, help="JSON report path; default: stdout")
    args = parser.parse_args()
    try:
        pwm_class, source_path, constants_path = load_reference(args.repo.resolve())
        config = types.SimpleNamespace(cycles_per_hour=3, duty_cycle_seconds=900)
        system = types.SimpleNamespace(base_offset=27.2)

        def device(flame=False):
            return types.SimpleNamespace(flow_temperature=45.0, flame_active=flame,
                                         hot_water_active=False)

        mapped = []
        mapper = pwm_class(config, system)
        mapper._effective_on_temperature = 45.0
        for duty in (.06, .075, .10, .15, .50, .925, .93):
            request = 27.2 + duty * (45.0 - 27.2)
            on, off = mapper._calculate_duty_cycle(request, device())
            mapped.append({"inputDuty": duty, "requestedC": request,
                           "onSeconds": on, "offSeconds": off})

        low = pwm_class(config, system)
        low.enable()
        ultra = []
        for timestamp in (0, 2400, 4800, 7200):
            low.update(device(), 27.2 + .06 * (45.0 - 27.2), timestamp)
            ultra.append({"timeSeconds": timestamp, "phase": low.status.value,
                          "phaseStarts": low._current_cycle,
                          "onOffSeconds": low._duty_cycle})

        relight_pwm = pwm_class(config, system)
        relight_pwm.enable()
        relight = []
        for timestamp, flame in ((0, True), (60, False), (120, True), (180, True)):
            relight_pwm.update(device(flame), 27.2 + .15 * (45.0 - 27.2), timestamp)
            relight.append({"timeSeconds": timestamp, "flameInput": flame,
                            "phase": relight_pwm.status.value,
                            "storedFlameOnSeconds": relight_pwm._flame_on_at})

        checks = {
            "ultraLowRemainsOff": len(ultra) == 4 and all(
                row["phase"] == "off" and row["phaseStarts"] == 0 for row in ultra),
            "relightDoesNotRestartMinimumAnchor": [row["phase"] for row in relight]
                == ["on", "on", "on", "off"] and relight[2]["storedFlameOnSeconds"] == 0,
            "mappedUltraLowZeroOn": mapped[0]["onSeconds"] == 0,
        }
        report = {"schemaVersion": 1, "referencePin": PIN,
            "generatedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "source": {"path": str(source_path),
                       "sha256": hashlib.sha256(source_path.read_bytes()).hexdigest()},
            "constants": {"path": str(constants_path),
                          "sha256": hashlib.sha256(constants_path.read_bytes()).hexdigest()},
            "adapter": "Original PWM methods; import AST nodes removed; HA/type/config/clock names stubbed",
            "limitations": "No full HA scheduling, transport, physical boiler or efficiency validation; only the named PWM scenarios are asserted",
            "dutyMapping": mapped, "ultraLowTrace": ultra, "relightTrace": relight,
            "checks": checks, "referenceTraceReproduced": all(checks.values())}
        encoded = json.dumps(report, indent=2) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(encoded, encoding="utf-8")
        else:
            print(encoded, end="")
        return 0 if all(checks.values()) else 1
    except (OSError, ValueError, SyntaxError, AttributeError, TypeError, StopIteration) as exc:
        print(f"SAT reference inspection failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
