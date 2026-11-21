#!/usr/bin/env python3
"""TASK-3000: the per-MsgID OT support table on MQTT, next to otgw-firmware/boiler/unsupported_msgids.

gruijter's MQTT-only Homey app needs what GET /api/v2/otgw/ot-support shows. Two retained CSV topics in the shape of
unsupported_msgids ("<id>R" for every read MsgID, then "<id>W" for every write MsgID):
  otgw-firmware/boiler/supported_msgids   the boiler acknowledged it (ot-support blAR / blAW)
  otgw-firmware/thermostat/sent_msgids    the thermostat sent it     (ot-support tsR / tsW)

Both tests run the REAL code, cut out of the firmware sources and compiled on the host:
  DirtyBits   - the bitmap block of processOT (OTGW-Core.ino, cut as tests/test_boiler_support_bitmap_b_only.py does)
                sets OT_SUPPORT_LIST_BOILER on a new boiler ack and OT_SUPPORT_LIST_THERMOSTAT on a new thermostat
                request, and nothing for Unknown-DataID, gateway-made or replayed frames, so the minute tick republishes
                only the list that changed;
  Publisher   - publishMsgidCsv() / publishOtSupportMsgidLists() (MQTTstuff.ino): the payload lists every set bit in
                order, also with all 256 IDs set in both directions (no truncation), an empty list is not published
                (a retained list is never replaced by "" before the data is known, the TASK-1781 lesson), and a failed
                publish is not reported as done (the dirty bit stays for the next minute).
On the code from before TASK-3000 both classes fail (no dirty bits, no publisher). Set OTGW_CORE_INO / OTGW_MQTT_INO to
test other copies.
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_boiler_support_bitmap_b_only as bmp  # noqa: E402

MQTT = Path(os.environ.get("OTGW_MQTT_INO", REPO / "src" / "OTGW-firmware" / "MQTTstuff.ino"))

DIRTY_MAIN = r"""
}
int main(int argc, char** argv) {
  char buf[4096]; strncpy(buf, argv[1], sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
  for (char* tok = strtok(buf, " "); tok; tok = strtok(nullptr, " ")) {
    char k = tok[0]; int type = 0, id = 0; char f1 = 0;
    sscanf(tok + 2, "%d:%d:%c", &type, &id, &f1);
    feed(k, type, id, false, f1 == 'l', f1 == 'r');
  }
  printf("lists %d\n", (int)otSupportListsDirty);
  return 0;
}
"""

PUB_STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
typedef char __FlashStringHelper;
#define F(s) s
#define PSTR(s) s
#define snprintf_P snprintf
enum : uint8_t { OT_SUPPORT_LIST_BOILER = 1, OT_SUPPORT_LIST_THERMOSTAT = 2 };
struct { struct { bool bEnable = true; } mqtt; } settings;
struct { struct { bool bConnected = true; } mqtt; } state;
static uint8_t aR[32], aW[32], sR[32], sW[32];
static bool bitOf(const uint8_t* b, uint8_t id) { return (b[id >> 3] >> (id & 7)) & 1; }
static bool isBoilerMsgIdAckedRead(uint8_t id)     { return bitOf(aR, id); }
static bool isBoilerMsgIdAckedWrite(uint8_t id)    { return bitOf(aW, id); }
static bool isThermostatMsgIdSentRead(uint8_t id)  { return bitOf(sR, id); }
static bool isThermostatMsgIdSentWrite(uint8_t id) { return bitOf(sW, id); }
static bool sendOk = true;
static bool sendMQTTData(const __FlashStringHelper* topic, const char* payload, bool retain) {
  printf("PUB %s|%d|%s\n", topic, retain ? 1 : 0, payload);
  return sendOk;
}
"""

PUB_MAIN = r"""
static void setBits(uint8_t* b, const char* list) {   // "1,5,255" or "all"
  memset(b, 0, 32);
  if (!strcmp(list, "-")) return;
  if (!strcmp(list, "all")) { memset(b, 0xFF, 32); return; }
  std::string s(list); size_t p = 0;
  while (p <= s.size()) { size_t q = s.find(',', p); if (q == std::string::npos) q = s.size();
    int id = atoi(s.substr(p, q - p).c_str()); b[id >> 3] |= (uint8_t)(1u << (id & 7)); p = q + 1; }
}
int main(int argc, char** argv) {
  // argv: aR aW sR sW lists sendOk connected
  setBits(aR, argv[1]); setBits(aW, argv[2]); setBits(sR, argv[3]); setBits(sW, argv[4]);
  sendOk = atoi(argv[6]) != 0; state.mqtt.bConnected = atoi(argv[7]) != 0;
  printf("DONE %d\n", (int)publishOtSupportMsgidLists((uint8_t)atoi(argv[5])));
  return 0;
}
"""


def cut_publisher(src):
    start = src.find("static bool publishMsgidCsv(")
    if start < 0:
        raise AssertionError("publishMsgidCsv() not found in %s (code from before TASK-3000)" % MQTT)
    fn = src.find("\nuint8_t publishOtSupportMsgidLists(", start)
    end = src.find("\n}\n", fn)
    if fn < 0 or end < 0:
        raise AssertionError("publishOtSupportMsgidLists() not found in %s" % MQTT)
    return src[start:end + 3]


class DirtyBits(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "dirty.cpp"
        # the shared prelude declares otSupportListsDirty; on the code from before TASK-3000 it stays 0 (red run)
        cpp.write_text(bmp.PRELUDE + bmp.cut_bitmap_block(bmp.CORE.read_text(encoding="utf-8")) + DIRTY_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("dirty.exe" if os.name == "nt" else "dirty"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def lists(self, frames):
        out = subprocess.run([str(self.exe), " ".join(frames)], capture_output=True, text=True, check=True).stdout
        return int(out.split()[-1])

    def test_new_boiler_read_ack_marks_the_boiler_list(self):
        self.assertEqual(self.lists(["T:0:3", "B:4:3"]), 1 | 2)          # T read 3 (thermostat) + B read ack 3 (boiler)

    def test_boiler_write_ack_marks_the_boiler_list(self):
        self.assertEqual(self.lists(["R:1:16", "B:5:16"]), 1)            # the gateway's R is no thermostat request

    def test_thermostat_request_alone_marks_only_the_thermostat_list(self):
        self.assertEqual(self.lists(["T:1:24"]), 2)

    def test_unknown_gateway_made_and_replayed_frames_mark_nothing(self):
        self.assertEqual(self.lists(["R:0:28", "B:7:28"]), 0)            # Unknown-DataID: unsupported_msgids' own flag
        self.assertEqual(self.lists(["R:0:25", "B:4:25:l"]), 0)          # bLocalAnswer (loopback, SR=)
        self.assertEqual(self.lists(["T:0:25:r", "B:4:25:r"]), 0)        # replayed lines


class Publisher(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "pub.cpp"
        cpp.write_text(PUB_STUBS + cut_publisher(MQTT.read_text(encoding="utf-8")) + PUB_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("pub.exe" if os.name == "nt" else "pub"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_pub(self, aR="-", aW="-", sR="-", sW="-", lists=3, ok=1, conn=1):
        out = subprocess.run([str(self.exe), aR, aW, sR, sW, str(lists), str(ok), str(conn)],
                             capture_output=True, text=True, check=True).stdout
        pubs, done = {}, None
        for line in out.splitlines():
            if line.startswith("PUB "):
                topic, retain, payload = line[4:].split("|", 2)
                pubs[topic] = (retain, payload)
            elif line.startswith("DONE "):
                done = int(line[5:])
        return pubs, done

    def test_payload_shape_matches_unsupported_msgids(self):
        pubs, done = self.run_pub(aR="0,3,5,25,125", aW="1,16,56", sR="0,3,17", sW="1,16,24")
        self.assertEqual(pubs["otgw-firmware/boiler/supported_msgids"], ("1", "0R,3R,5R,25R,125R,1W,16W,56W"))
        self.assertEqual(pubs["otgw-firmware/thermostat/sent_msgids"], ("1", "0R,3R,17R,1W,16W,24W"))
        self.assertEqual(done, 3)

    def test_all_256_ids_in_both_directions_are_not_cut(self):
        pubs, _ = self.run_pub(aR="all", aW="all")
        payload = pubs["otgw-firmware/boiler/supported_msgids"][1]
        want = ",".join(["%dR" % i for i in range(256)] + ["%dW" % i for i in range(256)])
        self.assertEqual(payload, want)
        self.assertEqual(len(payload), len(want))   # 2282 bytes, far above unsupported_msgids' 256-byte buffer

    def test_empty_list_is_not_published_but_counts_as_done(self):
        pubs, done = self.run_pub(sR="3")
        self.assertNotIn("otgw-firmware/boiler/supported_msgids", pubs)     # nothing known yet: the retained list stays
        self.assertEqual(pubs["otgw-firmware/thermostat/sent_msgids"][1], "3R")
        self.assertEqual(done, 3)

    def test_only_the_requested_list(self):
        pubs, done = self.run_pub(aR="3", sR="3", lists=2)
        self.assertEqual(list(pubs), ["otgw-firmware/thermostat/sent_msgids"])
        self.assertEqual(done, 2)

    def test_failed_publish_is_not_done(self):
        pubs, done = self.run_pub(aR="3", sR="3", ok=0)
        self.assertEqual(done, 0)                     # the dirty bits stay, the minute tick tries again

    def test_disconnected_publishes_nothing(self):
        pubs, done = self.run_pub(aR="3", sR="3", conn=0)
        self.assertEqual((pubs, done), ({}, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
