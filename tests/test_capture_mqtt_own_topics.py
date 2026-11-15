#!/usr/bin/env python3
"""TASK-1988 (Robert, 2026-11-15): the capture records only the MQTT topics this gateway publishes.

gruijter's beta.250 capture held his whole zigbee2mqtt household because otgw_capture.py subscribed to '#'. By
default the capture now subscribes to this gateway's own namespace and HA discovery configs and drops every other
message before it is written; --mqtt-all-topics is the deliberate opt-in for '#', with a warning.

    python tests/test_capture_mqtt_own_topics.py

1. A fake broker stream (own topics, zigbee2mqtt, a second OTGW whose unique id starts with ours, the command topic
   others publish) through MqttCapture.handle(): only the own topics reach mqtt.log.
2. --mqtt-all-topics: '#', everything written, the warning on the console and in summary.txt.
3. --topic without --mqtt-all-topics is refused; an unreadable gateway records no MQTT instead of '#'.
4. Every topic format the firmware publishes, derived from the source (the discovery config format strings and the
   value namespace), is accepted by the filter; the BLE config format (unique id inside a level) is among them.
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
UID = "otgw-A0F262F586F8"
GW = {"enable": True, "broker": "10.0.0.2", "port": 1883, "user": "", "password_length": 0,
      "toptopic": "OTGW", "haprefix": "homeassistant", "uniqueid": UID}

OWN = [
    f"OTGW/value/{UID}",                                   # the retained LWT online/offline
    f"OTGW/value/{UID}/Tboiler",
    f"OTGW/value/{UID}/sat/ble/a4c1380c1d2e/temp",
    f"homeassistant/sensor/{UID}/Tboiler/config",
    f"homeassistant/climate/{UID}/climate/config",
    f"homeassistant/sensor/{UID}_ble_a4c1380c1d2e_temp/config",
]
FOREIGN = [
    "zigbee2mqtt/Side table",
    "zigbee2mqtt/bridge/logging",
    f"OTGW/value/{UID}0/Tboiler",                          # a second gateway whose id starts with ours
    "OTGW/value/otgw-112233445566/Tboiler",
    f"OTGW/set/{UID}/command",                             # a command another client publishes
    "homeassistant/sensor/otgw-112233445566/Tboiler/config",
    f"homeassistant/sensor/{UID}0_ble_a4c1380c1d2e_temp/config",
    "homeassistant/sensor/kitchen_plug/power/config",
    "homeassistant/status",
]


def run_stream(keep):
    tmp = tempfile.TemporaryDirectory()
    cap = oc.Capture(Path(tmp.name), echo=False)
    mc = oc.MqttCapture(cap, "10.0.0.2", 1883, ["x"], "", None, keep=keep)
    mc.log = oc.StreamWriter(cap.store, "mqtt.log")
    cap.store.touch("mqtt.log")
    for t in OWN + FOREIGN:
        mc.handle(t, b"1", retain=False)
    cap.store.close()
    topics = [line.split(" ", 1)[0] for line in (Path(tmp.name) / "mqtt.log").read_text(encoding="utf-8").splitlines()
              if line and not line.startswith("#")]
    # zigbee topics hold a space: compare against the start of each written line instead of the first word
    text = (Path(tmp.name) / "mqtt.log").read_text(encoding="utf-8")
    tmp.cleanup()
    return topics, text, mc


class DefaultRecordsOnlyOwnTopics(unittest.TestCase):
    def test_parser_default_is_not_everything(self):
        args = oc.build_parser().parse_args([])
        self.assertIsNone(args.topic)
        self.assertFalse(args.mqtt_all_topics)

    def test_fake_stream_keeps_only_own(self):
        filters, keep, warning, note = oc.mqtt_topic_filters(GW, False, None)
        self.assertNotIn("#", filters)
        self.assertIsNone(warning)
        _, text, mc = run_stream(keep)
        for t in OWN:
            self.assertIn(f"{t} 1\n", text, t)
        for t in FOREIGN:
            self.assertNotIn(f"{t} 1\n", text, t)
        self.assertEqual(mc.messages, len(OWN))
        self.assertEqual(mc.dropped, len(FOREIGN))


class OptInRecordsEverything(unittest.TestCase):
    def test_all_topics(self):
        filters, keep, warning, _ = oc.mqtt_topic_filters(GW, True, None)
        self.assertEqual(filters, ["#"])
        self.assertIsNone(keep)
        self.assertIn("other devices", warning)
        _, text, mc = run_stream(keep)
        for t in OWN + FOREIGN:
            self.assertIn(f"{t} 1\n", text, t)

    def test_warning_goes_to_console_and_summary(self):
        src = (REPO / "scripts" / "otgw_capture.py").read_text(encoding="utf-8")
        body = src[src.index("    mqtt_capture = None\n"):]
        self.assertRegex(body[:1500], r"if getattr\(args, \"mqtt_warning\", None\):\s*\n\s*s\(args\.mqtt_warning\)")
        self.assertIn("s = cap.summary", src)   # s() writes summary.txt and echoes to the console


class Refusals(unittest.TestCase):
    def test_topic_needs_the_opt_in(self):
        with self.assertRaises(ValueError):
            oc.mqtt_topic_filters(GW, False, "OTGW/#")
        self.assertEqual(oc.mqtt_topic_filters(GW, True, "OTGW/#")[0], ["OTGW/#"])

    def test_unknown_gateway_records_nothing(self):
        for gw in (None, dict(GW, uniqueid=""), dict(GW, toptopic=""), dict(GW, uniqueid="a/#")):
            filters, keep, _, note = oc.mqtt_topic_filters(gw, False, None)
            self.assertEqual(filters, [])
            self.assertIn("no MQTT is recorded", note)


def firmware_topic_samples():
    """One sample topic per format the firmware publishes, from the source: every '%s/<component>/...config' format
    string (the HA prefix, then the node or unique id) and the value namespace that sendMQTTData() prefixes."""
    samples = set()
    for f in list(SRC.glob("*.ino")) + list(SRC.glob("*.cpp")) + list(SRC.glob("*.h")):
        for fmt in re.findall(r'"(%s/[a-z_%]+/[^"]*/config)"', f.read_text(encoding="utf-8", errors="replace")):
            parts = fmt.split("%s")
            vals = ["homeassistant", UID] + ["x"] * (len(parts) - 3)
            if fmt.startswith("%s/%s/%s/"):   # the generic table format: prefix, component, node id
                vals = ["homeassistant", "sensor", UID] + ["x"] * (len(parts) - 4)
            t = parts[0]
            for v, p in zip(vals, parts[1:]):
                t += v + p
            samples.add(t.replace("%u", "1"))
    samples.add(f"OTGW/value/{UID}")
    samples.add(f"OTGW/value/{UID}/x")
    return sorted(samples)


class EveryFirmwareTopicIsKept(unittest.TestCase):
    def test_derived_from_source(self):
        samples = firmware_topic_samples()
        self.assertGreater(len(samples), 20, samples)
        self.assertTrue(any("_ble_" in s for s in samples), "the BLE config format must be among the samples")
        _, keep, _, _ = oc.mqtt_topic_filters(GW, False, None)
        missed = [s for s in samples if not keep(s)]
        self.assertEqual(missed, [], "firmware topics the default filter would not record")

    def test_publish_chokepoint_unchanged(self):
        # every firmware publish goes through mqttPublishRaw(); a new direct MQTTclient.publish() needs this test
        # (and own_mqtt_topic()) to be looked at again
        calls = []
        for f in list(SRC.glob("*.ino")) + list(SRC.glob("*.cpp")):
            for i, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if "MQTTclient.publish(" in line and not line.strip().startswith("//"):
                    calls.append(f"{f.name}:{i}")
        self.assertEqual(len(calls), 2, calls)   # mqttPublishRaw() itself and the LWT 'offline' on the namespace


if __name__ == "__main__":
    unittest.main(verbosity=2)
