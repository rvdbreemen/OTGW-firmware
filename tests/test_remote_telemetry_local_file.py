#!/usr/bin/env python3
"""TASK-2907: a local download of the one recorder decodes like the uploaded records.

Records from the real rtel::Recorder (tests/remote_telemetry_harness.cpp, 30 min of OT frames, SATTICK, debug lines and
events) go through the real rtel::lzEncode into the real rtel::RecRing, and the real rtel::ringFill reads them out
behind the OTGWMIX1 header, in small chunks, as the download route does. decode_all.read_mix_file() and
decode_records() must give back every OT frame with its ms time, every SATTICK line and every event, and the
records must be stored LZ-coded where that is smaller.
"""
import base64, os, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools", "remote-telemetry"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import otrec  # noqa: E402
import decode_all  # noqa: E402
import test_remote_telemetry_codec as tcc  # noqa: E402

SRC = r'''
#include <cstdio>
#include <cstring>
#include <vector>
#include "@DIR@/RemoteTelemetryCodec.h"
#include "@DIR@/RecRing.h"
static rtel::LzWork w;
static rtel::RecRing ring;
static void noop(void *, uint8_t) {}
static void put(void *, uint8_t b) { ring.byte(b); }
int main() {
  static std::vector<uint8_t> mem(256 * 1024);
  ring.init(mem.data(), (uint32_t)mem.size());
  static char line[1 << 16];
  unsigned long long seq = 1000;
  while (fgets(line, sizeof line, stdin)) {
    long long t0; char hex[1 << 15];
    if (sscanf(line, "%lld %s", &t0, hex) != 2) continue;
    std::vector<uint8_t> in;
    for (char *h = hex; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); in.push_back((uint8_t)v); }
    rtel::Buf c; c.initSink(noop, nullptr);
    rtel::lzEncode(in.data(), in.size(), c, w);
    bool lz = c.n < in.size();
    if (!ring.begin(lz ? 5 : 4, (uint32_t)(lz ? c.n : in.size()), seq++, t0, 0)) { fprintf(stderr, "ring full\n"); return 1; }
    if (lz) { rtel::Buf b; b.initSink(put, nullptr); rtel::lzEncode(in.data(), in.size(), b, w); }
    else for (uint8_t x : in) ring.byte(x);
    ring.commit();
  }
  const char hdr[] = "OTGWMIX1";
  std::vector<uint8_t> out(hdr, hdr + 8);
  const char *texts[3] = {"2.0.0-test", "abcdef1", "otgw-0123456789ab"};
  out.push_back(1); out.push_back(0); out.push_back(0); out.push_back(0);
  for (const char *t : texts) { out.push_back((uint8_t)strlen(t)); for (const char *p = t; *p; p++) out.push_back((uint8_t)*p); }
  out[10] = (uint8_t)out.size(); out[11] = (uint8_t)(out.size() >> 8);
  rtel::RingCursor cur;
  uint8_t buf[700];
  for (int k = 0;; k++) {
    size_t n = rtel::ringFill(ring, cur, buf, 1 + (k * 131) % sizeof(buf), 0);
    if (!n) break;
    out.insert(out.end(), buf, buf + n);
  }
  for (uint8_t x : out) printf("%02x", x);
  printf("\n");
}
'''.replace("@DIR@", os.path.join(ROOT, "src", "OTGW-firmware").replace("\\", "/"))


def build():
    cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        return None
    d = tempfile.mkdtemp(prefix="crfile-")
    src, exe = os.path.join(d, "f.cpp"), os.path.join(d, "f")
    open(src, "w").write(SRC)
    subprocess.run([cxx, "-std=c++17", "-O1", "-Wall", "-Wextra", "-o", exe, src], check=True)
    return exe


EXE = build()


@unittest.skipIf(EXE is None, "no C++ compiler")
class LocalFileTest(unittest.TestCase):
    def test_download_decodes_like_the_upload(self):
        tcc.Harness.exe = tcc.Harness.build()
        lines, frames, sat, _, img = tcc.build_events(30, 300)
        # events in the same run
        ev, t = [], tcc.T0 + 31 * 60000                         # after the other lines: input stays in time order
        for k in range(300):
            t += 6000
            payload = otrec.evt_encode_item(t, t, 14, ["boiler_temp", f"{40 + k % 7}.50"])[2:]
            ev.append(t)
            lines.insert(len(lines) - 1, f"V {t} 14 {payload.hex()}")
        recs = tcc.Harness.run(lines)
        r = subprocess.run([EXE], input="\n".join(f"{t0} {p.hex()}" for t0, p in recs) + "\n",
                           capture_output=True, text=True, check=True)
        d = tempfile.mkdtemp(prefix="crfile-out-")
        path = os.path.join(d, "otgw-recording.bin")
        open(path, "wb").write(bytes.fromhex(r.stdout.strip()))
        records, hdr = decode_all.read_mix_file(path)
        self.assertEqual(hdr["uid"], "otgw-0123456789ab")
        self.assertEqual(len(records), len(recs))
        self.assertTrue(any(x["fmt"] == otrec.FMT_MIX_LZ for x in records))
        for x, (t0, p) in zip(records, recs):                      # every record back, byte for byte
            self.assertEqual(otrec.record_payload(x["fmt"], base64.b64decode(x["payload"])), p)
            self.assertEqual(x["t0"], t0)
        ot, sat_out, events = [], [], []
        for x in records:
            sec = otrec.unpack_mix(x["t0"], otrec.record_payload(x["fmt"], base64.b64decode(x["payload"])))
            ot += sec.get(otrec.SEC_OT, [])
            sat_out += otrec.sat_decode(x["t0"], sec.get(otrec.SEC_SAT, b""))
            events += [tt for tt, ty, f in otrec.evt_decode(x["t0"], sec.get(otrec.SEC_EVT, b"")) if ty == 14]
        self.assertEqual(ot, frames)
        self.assertEqual(sat_out, sat)
        self.assertEqual(events, ev)


if __name__ == "__main__":
    unittest.main(verbosity=2)
