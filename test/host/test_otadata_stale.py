#!/usr/bin/env python3
"""TASK-1919 host harness: does the main firmware erase the stale same-seq otadata entry, and only that one?

    python test/host/test_otadata_stale.py

With one OTA slot every esp_ota_set_boot_partition() leaves the same seq in the other otadata sector, so an image
that fails its first boot runs a second time (the bootloader boots the other same-seq entry) instead of rolling
back to the safeboot updater (09's dumps on .61, 2026-11-15, scratchpad t1919/). The fix
(OTGW-ModUpdateServer-esp32.h, the otadataStale block, sliced here between its markers) erases the sector the
bootloader does not boot from, early in setup().

Each case is a pair of otadata entries as 09 dumped them. The harness compiles the REAL block, asks which sector
it erases, and then runs a model of the ESP-IDF bootloader (bootloader_utility.c:396-404 PENDING_VERIFY ->
ABORTED at boot, bootloader_common_loader.c:71-79 validity, :144-168 selection; no valid entry -> factory, the
safeboot updater) for one failed boot of the running image:
  E   handover only:      [0] seq1 NEW         | [1] erased            -> erase nothing
  G   first boot:         [0] seq1 PENDING     | [1] seq1 NEW          -> erase 1; a failure then boots safeboot
  H   after confirm:      [0] seq1 VALID       | [1] seq1 NEW          -> erase 1
  L   second life:        [0] seq1 ABORTED     | [1] seq1 PENDING      -> erase 0; a failure then boots safeboot
  B   both erased:        [0] erased           | [1] erased            -> erase nothing
  S1  only sector 1:      [0] erased           | [1] seq1 VALID        -> erase nothing (it is the one booted)
  X   other invalid:      [0] seq1 VALID       | [1] seq2 ABORTED      -> erase 1 (higher seq, but not bootable)
  C   bad CRC in [0]:     [0] seq1 VALID (bad) | [1] seq1 NEW          -> erase 0 (the bootloader boots [1])
  P   confirmed, copy:    [0] seq1 VALID       | [1] seq1 VALID        -> erase nothing (the copy is the fallback)
  PL  power loss in the confirm's rewrite: [0] seq1 PENDING | [1] seq1 VALID copy -> erase nothing; a reset then
      boots ota_0 from the copy (the image had passed the health gate), not the safeboot updater
Never erased: the sector the bootloader boots from (checked for every case).
Mutant MN (the code before TASK-1919: nothing erased) must fail G and L; mutant MK (the VALID copy erased as well)
must fail P and PL.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, host_cxx_build

UPD = REPO / "src" / "OTGW-firmware" / "OTGW-ModUpdateServer-esp32.h"
INO = REPO / "src" / "OTGW-firmware" / "OTGW-firmware.ino"

CASES = [  # id, (seq0, state0, crc0ok), (seq1, state1, crc1ok), want_erase, want_after_failure
    ("E",  (1, 0, True),  (0xFFFFFFFF, 0xFFFFFFFF, False), -1, None),   # before the first boot: the app never runs in E
    ("G",  (1, 1, True),  (1, 0, True),                     1, "safeboot"),
    ("H",  (1, 2, True),  (1, 0, True),                     1, None),
    ("L",  (1, 4, True),  (1, 1, True),                     0, "safeboot"),
    ("B",  (0xFFFFFFFF, 0xFFFFFFFF, False), (0xFFFFFFFF, 0xFFFFFFFF, False), -1, None),
    ("S1", (0xFFFFFFFF, 0xFFFFFFFF, False), (1, 2, True),   -1, None),
    ("X",  (1, 2, True),  (2, 4, True),                     1, None),
    ("C",  (1, 2, False), (1, 0, True),                     0, None),
    # 95's review: the VALID copy written before the confirm is kept, and covers a power loss during the confirm.
    ("P",  (1, 2, True),  (1, 2, True),                    -1, None),
    ("PL", (1, 1, True),  (1, 2, True),                    -1, "ota_0"),
]

DRIVER = r"""
#include <cstdint>
#include <cstdio>
#include "block.inc"
// Bootloader model: PENDING_VERIFY -> ABORTED at boot, then the active valid entry (highest seq, equal -> 0).
static int bootPick(uint32_t seq[2], uint32_t st[2], const bool crc[2]) {
  for (int i = 0; i < 2; i++) if (st[i] == 1u) st[i] = 4u;
  bool v[2] = {otadataEntryValid(seq[0], st[0], crc[0]), otadataEntryValid(seq[1], st[1], crc[1])};
  if (v[0] && v[1]) return seq[0] >= seq[1] ? 0 : 1;
  if (v[0]) return 0;
  if (v[1]) return 1;
  return -1;   // factory: the safeboot updater
}
static void run(const char* id, uint32_t s0, uint32_t t0, bool c0, uint32_t s1, uint32_t t1, bool c1) {
  uint32_t seq[2] = {s0, s1}, st[2] = {t0, t1};
  bool crc[2] = {c0, c1};
  bool v[2] = {otadataEntryValid(seq[0], st[0], crc[0]), otadataEntryValid(seq[1], st[1], crc[1])};
  // The sector the bootloader boots from now (state as on flash while the app runs).
  int booted = (v[0] && v[1]) ? (seq[0] >= seq[1] ? 0 : 1) : v[0] ? 0 : v[1] ? 1 : -1;
  int erase = otadataStaleSector(seq, st, v);
  if (erase >= 0) { seq[erase] = 0xFFFFFFFFu; st[erase] = 0xFFFFFFFFu; crc[erase] = false; }
  // The running image fails before it confirms: what does the next boot start?
  int next = bootPick(seq, st, crc);
  std::printf("CASE %s erase=%d booted=%d after_failure=%s\n", id, erase, booted, next < 0 ? "safeboot" : "ota_0");
}
int main() {
CALLS
  return 0;
}
"""


def block(text):
    m = re.search(r"// --- otadataStale begin.*?\n(.*?)// --- otadataStale end", text, re.S)
    if not m:
        raise SystemExit("otadataStale markers not found")
    return "#include <cstdint>\n" + m.group(1)


def build_and_run(label, code):
    d = GEN / "otadata_stale" / label
    d.mkdir(parents=True, exist_ok=True)
    (d / "block.inc").write_text(code, encoding="utf-8")
    calls = "\n".join(
        f'  run("{cid}", {a[0]}u, {a[1]}u, {str(a[2]).lower()}, {b[0]}u, {b[1]}u, {str(b[2]).lower()});'
        for cid, a, b, _, _ in CASES)
    (d / "t.cpp").write_text(DRIVER.replace("CALLS", calls), encoding="utf-8")
    exe = d / "t.exe"
    exe.unlink(missing_ok=True)
    ok, out_ = host_cxx_build([d / 't.cpp'], exe, includes=[d], workdir=d)
    if not ok:
        print(out_[-3000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    out = subprocess.run([str(exe)], capture_output=True, text=True).stdout
    return {m.group(1): (int(m.group(2)), int(m.group(3)), m.group(4))
            for m in re.finditer(r"^CASE (\S+) erase=(-?\d+) booted=(-?\d+) after_failure=(\S+)", out, re.M)}


def judge(res):
    bad = []
    for cid, _, _, want_erase, want_after in CASES:
        erase, booted, after = res[cid]
        if erase != want_erase:
            bad.append(f"{cid}: erases {erase}, want {want_erase}")
        if booted >= 0 and erase == booted:
            bad.append(f"{cid}: erases the sector the bootloader boots from ({booted})")
        if want_after and after != want_after:
            bad.append(f"{cid}: a failed boot then starts {after}, want {want_after}")
    return bad


def main():
    text = UPD.read_text(encoding="utf-8")
    fix = build_and_run("fix", block(text))
    print("== FIX (working tree) ==")
    for cid in fix:
        print(f"  {cid}: erase={fix[cid][0]} booted={fix[cid][1]} after a failure: {fix[cid][2]}")
    fix_bad = judge(fix)
    mutant_src = re.sub(r"(inline int otadataStaleSector\([^)]*\) \{)", r"\1\n  return -1;   // MN", block(text), count=1)
    mn = build_and_run("MN", mutant_src)
    mn_failed = sorted({b.split(":")[0] for b in judge(mn)})
    print(f"== MN (nothing erased, the code before TASK-1919) -> failing cases: {', '.join(mn_failed) or 'none'}")
    keep_line = "  if (valid[other] && state[other] == 2u /* VALID */ && seq[other] == seq[active]) return -1;"
    mk_src = block(text).replace(keep_line, "")
    if mk_src == block(text):
        raise SystemExit("mutant MK: the keep-copy line was not found")
    mk = build_and_run("MK", mk_src)
    mk_failed = sorted({b.split(":")[0] for b in judge(mk)})
    print(f"== MK (the VALID copy is erased too) -> failing cases: {', '.join(mk_failed) or 'none'}")
    # The call site: as the first call in setup() hasSafeboot()'s flash read hung the boot (09, beta.324 on .61);
    # right after the LittleFS mount it runs (beta.324dbg2). One call, after LittleFS.begin(), before readSettings().
    ino = INO.read_text(encoding="utf-8")
    body = ino[ino.index("void setup() {"):ino.index("void loop()")]
    calls = [m.start() for m in re.finditer(r"^\s*safebootDropStaleOtadata\(", body, re.M)]
    mount = body.find("LittleFSmounted = LittleFS.begin();")
    settings = body.find("readSettings(true);")
    site_ok = len(calls) == 1 and 0 <= mount < calls[0] < settings
    print(f"== call site: {len(calls)} call(s) in setup(), mount at {mount}, call at {calls[0] if calls else -1}, readSettings at {settings}")
    checks = [("FIX meets every case and never erases the booted sector", not fix_bad),
              ("setup() calls it once, after LittleFS.begin() and before readSettings()", site_ok),
              ("MN fails G and L (the second life)", {"G", "L"} <= set(mn_failed)),
              ("MK fails P and PL (the copy that covers a power loss during the confirm)", {"P", "PL"} <= set(mk_failed))]
    for b in fix_bad:
        print("  FIX: " + b)
    print("== verdict ==")
    for t, ok in checks:
        print(f"  {t}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
