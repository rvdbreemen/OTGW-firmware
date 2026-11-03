#!/usr/bin/env python3
"""TASK-1490: Home Assistant discovery for the SAT health, sync, window, fault and setpoint topics
(TASK-1486 audit, sections 1 and 2).

The discovery payloads come from the REAL composers: the harness test/host/test_ha_discovery_json.py
compiles MQTTHaDiscovery.cpp (verbatim) with the slices of MQTTstuff.ino it needs and records every
publish. This test builds that program from the working tree (or from the revision in
OTGW_DISC_REV, for the red run on the old code) and asserts, per new row, the topic, device class,
unit, state class, entity category and payload convention the audit asks for.

Also checked, because rows were inserted in the middle of the two tables:
  dispatch   doAutoConfigureMsgid(254) publishes every new row (the index points at the right row),
             and the ids behind the insert point (240, 241, 242, 244) still publish their rows;
  switch     the SAT switches publish one distinct config each (the case 12 fall-through is gone);
  select     the SAT preset select and the out-of-range index.

Run:  python tests/test_sat_health_discovery.py
Red:  OTGW_DISC_REV=HEAD python tests/test_sat_health_discovery.py   (before the change is committed)
"""
import json
import os
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HOST = REPO / "test" / "host"
sys.path.insert(0, str(HOST))
import test_ha_discovery_json as H   # noqa: E402  (the harness: export(), build_and_run())

REV = os.environ.get("OTGW_DISC_REV") or None

# object id (topic tail) -> expected fields of the payload. None = key must be absent.
# Binary sensors: ON = problem for health, sync and fault flags; window_open is a window.
BIN = {
    "device_health":   dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "cycle_health":    dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "pressure_health": dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "setpoint_sync":   dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "modulation_sync": dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "ch_sync":         dict(device_class="problem", entity_category="diagnostic", pl_on=None),
    "window_open":     dict(device_class="window",  entity_category=None, pl_on="true", pl_off="false"),
    "fallback_active": dict(device_class="problem", entity_category="diagnostic", pl_on="true", pl_off="false"),
    "room_temp_lost":  dict(device_class="problem", entity_category="diagnostic", pl_on="true", pl_off="false"),
    "ot_link_lost":    dict(device_class="problem", entity_category="diagnostic", pl_on="true", pl_off="false"),
    "push_refused":    dict(device_class="problem", entity_category="diagnostic", pl_on="true", pl_off="false"),
}
SENSOR = {
    "requested_setpoint": dict(device_class="temperature", unit_of_measurement="°C",
                               state_class="measurement", entity_category=None),
    "heating_curve_recommendation": dict(device_class=None, unit_of_measurement=None,
                                         state_class=None, entity_category=None),
    "manufacturer":       dict(device_class=None, unit_of_measurement=None,
                               state_class=None, entity_category="diagnostic"),
    "pressure_drop_rate": dict(device_class=None, unit_of_measurement="bar/h",
                               state_class="measurement", entity_category="diagnostic"),
}
# Rows the audit lists but the firmware does not need a new row for.
NOT_ADDED = {
    "consumption": "already discovered since TASK-1506 (volume_flow_rate, m3/h)",
    "max_setpoint": "already a number entity since TASK-1373 (stat_t sat/max_setpoint)",
}


def payloads(run, comp, tag_prefix):
    """{object id tail: json} for the SAT sensors of one component from direct composer calls."""
    out = {}
    for p in run[2]:
        if not p["tag"].startswith(tag_prefix) or not p["raw"]:
            continue
        parts = p["topic"].split("/")
        if parts[1] != comp:
            continue
        out[parts[3]] = json.loads(p["raw"].decode("utf-8"))
    return out


class SatHealthDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = H.GEN / "ha_discovery_json" / ("sat_health_old" if REV else "sat_health")
        H.export(REV, root)
        cls.rec = H.build_and_run(f"TASK-1490 ({REV or 'working tree'})", root, REV)
        cls.ctx = cls.rec[0]
        cls.bin = payloads(cls.rec, "binary_sensor", "streamBinarySensorDiscovery")
        cls.sen = payloads(cls.rec, "sensor", "streamSensorDiscovery")

    def stat_t(self, name):
        return f"{self.ctx['pub']}/sat/{name}"

    def check_fields(self, name, got, want):
        self.assertEqual(got["stat_t"], self.stat_t(name), name)
        for key, val in want.items():
            if val is None:
                self.assertNotIn(key, got, f"{name}: {key} must be absent")
            else:
                self.assertEqual(got.get(key), val, f"{name}: {key}")
        self.assertNotIn("cmd_t", got, f"{name}: read-only entity has a command topic")

    def test_binary_rows(self):
        for name, want in BIN.items():
            with self.subTest(name):
                got = self.bin.get(f"sat_sat_{name}")
                self.assertIsNotNone(got, f"binary_sensor sat_sat_{name} is not announced")
                self.check_fields(name, got, want)

    def test_sensor_rows(self):
        for name, want in SENSOR.items():
            with self.subTest(name):
                got = self.sen.get(f"sat_sat_{name}")
                self.assertIsNotNone(got, f"sensor sat_sat_{name} is not announced")
                self.check_fields(name, got, want)

    def test_audit_rows_not_added_exist_already(self):
        # consumption: sensor row from TASK-1506. max_setpoint: number entity, not a sensor.
        self.assertIn("sat_sat_consumption", self.sen)
        self.assertEqual(self.sen["sat_sat_consumption"]["unit_of_measurement"], "m³/h")
        self.assertNotIn("sat_sat_max_setpoint", self.sen)
        nums = [p for p in self.rec[2] if p["tag"] == "streamSatNumberDiscovery" and p["raw"]]
        self.assertTrue(any(json.loads(p["raw"])["stat_t"] == self.stat_t("max_setpoint") for p in nums))

    def test_dispatcher_publishes_every_new_row_and_the_shifted_ids(self):
        # doAutoConfigureMsgid(id) walks the index tables: a stale index entry drops a whole id.
        topics = {p["topic"] for p in self.rec[2] if p["tag"] == "doAutoConfigureMsgid" and p["raw"]}
        tails = {t.split("/")[-2] for t in topics}
        for name in list(BIN) + list(SENSOR):
            self.assertIn(f"sat_{name}", tails, f"id 254 does not publish sat_{name}")
        # ids behind the insert points: 240 (command_dropped, command_retry_exceeded), 241, 242, 244
        for label in ("command_dropped", "command_retry_exceeded", "dhw_water_total", "hvac_mode",
                      "hvac_action", "gateway_mode", "otgw_connected", "sat_flame_status",
                      "sat_flame_health"):
            self.assertTrue(any(label in t for t in tails), f"the dispatcher lost {label}")

    def test_switches_are_distinct_and_enabled_switch_exists(self):
        sw = [json.loads(p["raw"]) for p in self.rec[2]
              if p["tag"] == "streamSatSwitchDiscovery" and p["raw"] and "first=1" in p["args"]
              and "engine=pic" in p["args"]]
        uniq = [s["uniq_id"] for s in sw]
        self.assertEqual(len(uniq), len(set(uniq)), "two switch calls announced the same entity")
        en = [s for s in sw if s["uniq_id"].endswith("-sat_sat-enabled")]
        self.assertEqual(len(en), 1, "SAT enabled switch is missing")
        self.assertEqual(en[0]["cmd_t"], f"{self.ctx['sub']}/sat/enabled")
        self.assertEqual(en[0]["stat_t"], self.stat_t("enabled"))
        self.assertEqual((en[0]["pl_on"], en[0]["pl_off"]), ("true", "false"))

    def test_preset_select(self):
        sel = [json.loads(p["raw"]) for p in self.rec[2]
               if p["tag"] == "streamSatSelectDiscovery" and p["raw"] and "engine=pic" in p["args"]
               and "first=1" in p["args"]]
        pre = [s for s in sel if s["uniq_id"].endswith("-sat_sat-preset")]
        self.assertEqual(len(pre), 1, "SAT preset select is missing")
        self.assertEqual(pre[0]["cmd_t"], f"{self.ctx['sub']}/sat/preset")
        self.assertEqual(pre[0]["stat_t"], self.stat_t("preset"))
        self.assertEqual(pre[0]["options"], ["none", "away", "eco", "comfort", "sleep", "activity", "home"])
        self.assertEqual(len({s["uniq_id"] for s in sel}), len(sel))


if __name__ == "__main__":
    unittest.main(verbosity=2)
