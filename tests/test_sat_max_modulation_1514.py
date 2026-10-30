"""TASK-1514: the Max relative modulation slider rides on existing plumbing plus one new HA number.

Source-structure guard (the discovery writer is firmware code that does not run on the host; the bench
check covers the real payload): the MQTT set topic, the REST route and the state topic exist, and the HA
number entity idx 1 is discovered with the 0..100 % range and the same topic for command and state.

Run: python tests/test_sat_max_modulation_1514.py
"""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


class MaxModulationNumber(unittest.TestCase):
    def test_mqtt_set_topic_maps_to_setting(self):
        self.assertRegex(read("MQTTstuff.ino"), r'\{\s*"max_modulation",\s*"SATmaxmodulation"')

    def test_rest_route_exists(self):
        self.assertIn('PSTR("max_modulation")', read("restAPI.ino"))

    def test_state_topic_published(self):
        self.assertIn('F("sat/max_modulation")', read("SATcontrol.ino"))

    def test_discovery_requests_number_idx_1(self):
        self.assertRegex(read("MQTTstuff.ino"), r"streamSatNumberDiscovery\(1,\s*ctx\)")

    def test_number_entity_range_and_topics(self):
        cpp = read("MQTTHaDiscovery.cpp")
        fn = cpp[cpp.index("bool streamSatNumberDiscovery("):]
        fn = fn[: fn.index("\n}\n")]
        self.assertIn("numberIdx > 1", fn)
        self.assertIn("sat_max_modulation/config", fn)
        self.assertIn("sat-max-modulation", fn)
        self.assertRegex(fn, r'\\"unit_of_measurement\\":\\"%\\",\\"min\\":0,\\"max\\":100,\\"step\\":1')
        # cmd_t and stat_t both end in /sat/max_modulation
        self.assertEqual(len(re.findall(r"/sat/max_modulation", fn)), 2)


if __name__ == "__main__":
    unittest.main()
