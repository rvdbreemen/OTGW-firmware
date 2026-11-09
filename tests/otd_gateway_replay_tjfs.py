"""TASK-1707 AC1: replay a recorded thermostat stream through the real OT-Direct gateway code.

Usage: python -I tests/otd_gateway_replay_tjfs.py <capture.txt> [<OTDirect.ino> ...]
(default OTDirect.ino: src/OTGW-firmware/OTDirect.ino)

The capture is an OTmonitor-style decoded log ("hh:mm:ss.ffffff Thermostat  T80000200  0 Read-Data ...").
Only the thermostat's own frames (the "Thermostat T........" lines) are taken, at their recorded times
(ms resolution). They are fed to handleSlaveRequest() of the real OTDirect.ino slices that
tests/test_otd_gateway_alt_slots.py compiles (same harness, same fake boiler, same build); the
gateway's boiler side runs loopOTDirect() every 2 ms. The fake boiler answers every Data-ID except the
ones the capture's boiler answered Unknown-DataID (taken from the capture's "Unknown-Data-Id" lines).

Printed per OTDirect.ino:
  tframes / bframes        thermostat frames fed / frames the gateway put on the boiler bus
  minper / maxper          boiler frames between two thermostat frames (PIC model: always 1)
  replies                  frames answered to the thermostat (must equal tframes)
  altslots / badslotreply  thermostat requests for an unsupported ID answered Unknown-DataID / wrong reply
The capture comes from a tester and is user data: keep it out of the repo, pass its path.
Not a unit test (needs the capture), so it is not named test_*.py. Exit code 0; read the output.
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_otd_gateway_alt_slots as g  # noqa: E402  (PRE, MAIN, slices(), lib_and_types(), compile_cpp)

T_LINE = re.compile(r"^(\d\d):(\d\d):(\d\d\.\d+)\s+Thermostat\s+T([0-9A-F]{8})\b")
UNKNOWN = re.compile(r"\sB[0-9A-F]{8}\s+(\d+)\s+Unknown-Data-Id")

REPLAY = r"""
static bool g_unsup[256];
static Run replay(const std::vector<std::pair<uint32_t, unsigned long>>& tf) {
  Run r; int sinceLast = -1; size_t seenSent = 0, k = 0;
  uint32_t end = g_now + tf.back().first + 2000;
  for (; g_now < end; g_now += 1) {
    while (k < tf.size() && g_now >= 1000 + tf[k].first) {
      if (sinceLast >= 0) { if (sinceLast < r.minPer) r.minPer = sinceLast; if (sinceLast > r.maxPer) r.maxPer = sinceLast; }
      sinceLast = 0;
      handleSlaveRequest(tf[k++].second, OpenThermResponseStatus::SUCCESS);
      r.tframes++;
      state.otd.bThermostatConnected = true;
    }
    if (g_now % 2 == 0) { boilerTick(); loopOTDirect(); }
    while (seenSent < otMaster.sent.size()) {
      unsigned long q = otMaster.sent[seenSent++];
      r.bframes++; if (sinceLast >= 0) sinceLast++;
      r.perId[idOf(q)]++;
    }
  }
  r.replies = (int)otSlave.sent.size();
  for (size_t i = 0; i + 1 < g_log.size(); i++)
    if (g_log[i].prefix == 'T' && g_log[i + 1].prefix == 'A' && !boilerSupports(idOf(g_log[i].frame))) {
      r.altSlots++;
      const unsigned long a = g_log[i + 1].frame, t = g_log[i].frame;
      if (typeOf(a) != 7 || idOf(a) != idOf(t) || (a & 0xFFFF) != (t & 0xFFFF) || OpenTherm::parity(a)) r.badSlotReply++;
    }
  return r;
}
int main() {
  std::vector<std::pair<uint32_t, unsigned long>> tf;
  unsigned ms; unsigned long f; int unsup;
  while (scanf("U %d\n", &unsup) == 1) g_unsup[unsup & 0xFF] = true;
  while (scanf("%u %lx\n", &ms, &f) == 2) tf.push_back({ ms, f });
  g_no24 = g_unsup[24];
  resetAll();
  print("R", replay(tf));
  return 0;
}
"""


def secs(h, m, s):
    return int(h) * 3600 + int(m) * 60 + float(s)


def read_capture(path):
    frames, unsup = [], set()
    t0 = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if (u := UNKNOWN.search(line)):
                unsup.add(int(u[1]))
            m = T_LINE.match(line)
            if not m:
                continue
            t = secs(m[1], m[2], m[3])
            if t0 is None:
                t0 = t
            frames.append((int(round((t - t0) * 1000)), int(m[4], 16)))
    return frames, unsup


def build_and_run(ino, stdin):
    src = Path(ino).read_text(encoding="utf-8")
    enums, statics, types = g.lib_and_types()
    pre = g.PRE.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)
    harness = g.MAIN[:g.MAIN.index("int main() {")]
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "r.cpp"
        cpp.write_text(pre + g.slices(src) + harness + REPLAY, encoding="utf-8")
        exe = g.compile_cpp(cpp, Path(d) / ("r.exe" if os.name == "nt" else "r"))
        if exe is None:
            sys.exit("no host C++ compiler")
        return subprocess.run([str(exe)], input=stdin, capture_output=True, text=True, check=True).stdout


def main():
    frames, unsup = read_capture(sys.argv[1])
    inos = sys.argv[2:] or [str(g.OTD_INO)]
    ids = sorted({(f >> 16) & 0xFF for _, f in frames})
    print(f"capture: {len(frames)} thermostat frames over {frames[-1][0] / 1000:.1f} s, Data-IDs {ids}, "
          f"boiler Unknown-DataID for {sorted(unsup)}")
    stdin = "".join(f"U {u}\n" for u in sorted(unsup)) + "".join(f"{ms} {f:08X}\n" for ms, f in frames)
    for ino in inos:
        print(f"== {ino}")
        print(build_and_run(ino, stdin), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
