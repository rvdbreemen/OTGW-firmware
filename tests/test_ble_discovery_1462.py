#!/usr/bin/env python3
"""TASK-1462: Home Assistant discovery of the SAT BLE diagnostics.

  sat/ble_sensor_rssi   sensor, unit dBm, device_class signal_strength, state_class measurement
  sat/ble_temp_valid    binary_sensor (published as "true"/"false"), no longer a sensor; the old
                        sensor config (sensor/<node>/sat_sat_ble_temp_valid/config) is emptied
                        (retained) by the SAT discovery pass.

The payloads come from the REAL composers: test/host/test_ha_discovery_json.py compiles
MQTTHaDiscovery.cpp verbatim and records every publish. OTGW_DISC_REV=<git rev> builds the same
test from an older revision (red run on the old code).

Run:  python tests/test_ble_discovery_1462.py
Red:  OTGW_DISC_REV=origin/dev python tests/test_ble_discovery_1462.py
"""
import json
import os
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HOST = REPO / "test" / "host"
sys.path.insert(0, str(HOST))
import test_ha_discovery_json as H   # noqa: E402

REV = os.environ.get("OTGW_DISC_REV") or None
OLD_SENSOR = "sat_sat_ble_temp_valid"


class BleDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = H.GEN / "ha_discovery_json" / ("ble_1462_old" if REV else "ble_1462")
        H.export(REV, root)
        cls.rec = H.build_and_run(f"TASK-1462 ({REV or 'working tree'})", root, REV)
        cls.ctx = cls.rec[0]
        cls.pubs = cls.rec[2]

    def by_comp(self, comp, tags=None):
        out = {}
        for p in self.pubs:
            if not p["raw"] or (tags and p["tag"] not in tags):
                continue
            parts = p["topic"].split("/")
            if parts[1] == comp:
                out[parts[3]] = json.loads(p["raw"].decode("utf-8"))
        return out

    def test_rssi_unit_and_device_class(self):
        got = self.by_comp("sensor", ("streamSensorDiscovery",)).get("sat_sat_ble_sensor_rssi")
        self.assertIsNotNone(got, "sensor sat_sat_ble_sensor_rssi is not announced")
        self.assertEqual(got.get("unit_of_measurement"), "dBm")
        self.assertEqual(got.get("device_class"), "signal_strength")
        self.assertEqual(got.get("state_class"), "measurement")
        self.assertEqual(got.get("entity_category"), "diagnostic")
        self.assertEqual(got["stat_t"], f"{self.ctx['pub']}/sat/ble_sensor_rssi")

    def test_temp_valid_is_a_binary_sensor(self):
        got = self.by_comp("binary_sensor", ("streamBinarySensorDiscovery",)).get(OLD_SENSOR)
        self.assertIsNotNone(got, "binary_sensor sat_sat_ble_temp_valid is not announced")
        self.assertEqual(got["stat_t"], f"{self.ctx['pub']}/sat/ble_temp_valid")
        # SATble.ino publishes "true"/"false" (publishIfChangedB)
        self.assertEqual((got.get("pl_on"), got.get("pl_off")), ("true", "false"))
        self.assertEqual(got.get("entity_category"), "diagnostic")
        self.assertNotIn("cmd_t", got)

    def test_temp_valid_is_no_longer_a_sensor(self):
        sensors = self.by_comp("sensor", ("streamSensorDiscovery",))
        self.assertNotIn(OLD_SENSOR, sensors, "ble_temp_valid is still announced as a sensor")

    def test_old_sensor_config_is_retired(self):
        tail = f"/sensor/{self.ctx['node']}/{OLD_SENSOR}/config"
        hits = [p for p in self.pubs if p["topic"].endswith(tail)]
        self.assertTrue([p for p in hits if p["raw"] == b"" and p["retain"]],
                        f"no empty retained publish on {tail}; saw {[(h['tag'], len(h['raw'])) for h in hits]}")

    def test_neighbours_and_shifted_indexes_still_publish(self):
        # sensor 251 lost a row, binary 251 gained one: the ids behind the change must be intact.
        sen = {p["topic"].split("/")[3] for p in self.pubs
               if p["raw"] and p["tag"] == "doAutoConfigureMsgid" and p["topic"].split("/")[1] == "sensor"}
        for tail in ("sat_sat_ble_sensor_rssi", "sat_sat_ble_battery", "sat_sat_ble_sensor_count",
                     "sat_sat_ch_pressure_status", "sat_sat_target", "sat_sat_ble_temp",
                     "sat_sat_manufacturer", "command_dropped", "dhw_water_total", "hvac_mode"):
            self.assertTrue(any(tail in t for t in sen), f"the dispatcher lost sensor {tail}")
        binr = {p["topic"].split("/")[3] for p in self.pubs
                if p["raw"] and p["tag"] == "doAutoConfigureMsgid" and p["topic"].split("/")[1] == "binary_sensor"}
        for tail in ("sat_sat_ble_temp_valid", "sat_sat_active", "sat_sat_sh_rejected",
                     "gateway_mode", "command_retry_exceeded", "sat_sat_weather_is_day"):
            self.assertTrue(any(tail in t for t in binr), f"the dispatcher lost binary_sensor {tail}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
