#!/usr/bin/env python3
"""TASK-1654 host harness: the OpenTherm boiler simulator engine answers each scenario as its
expected_wire says.

    python test/host/test_ot_boiler_sim.py

Compiles tools/bench/ot-boiler-sim/src/boiler_sim.cpp (the engine the OTGW32 sketch runs, unchanged)
with tools/bench/ot-boiler-sim/host/sim_driver.cpp under MSVC (vswhere, as test_ot_reserved_range.py),
then feeds it request streams built here and checks every answer frame. Expected frames are encoded
by the 1412.20 generator's own frame() (scripts/sat_bench_scenarios_1412_20.py), an encoder the
engine does not share. The requests come from this test, not from the replay fixtures: a wire boiler
answers what the master asks, so CH active and the flame follow the master's MsgID 0 flags.

Also checked: the committed data/*.scn equal a fresh export, the sketch's pins equal boards.h, and
the parser refuses an answer delay outside the OT spec v4.2 slave window (20..800 ms).
Proves the engine only. The sketch glue (main.cpp) is not compiled here and nothing ran on a board.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_ot_reserved_range import GEN, REPO, find_vcvars  # noqa: E402

SIM = REPO / "tools" / "bench" / "ot-boiler-sim"
DATA = SIM / "data"
sys.path.insert(0, str(REPO / "scripts"))
sys.path.insert(0, str(REPO / "tools" / "research" / "sat"))
import sat_bench_scenarios_1412_20 as gen  # noqa: E402

READ_DATA, WRITE_DATA = gen.READ_DATA, gen.WRITE_DATA
READ_ACK, WRITE_ACK, DATA_INVALID, UNKNOWN = gen.READ_ACK, gen.WRITE_ACK, gen.DATA_INVALID, gen.UNKNOWN_DATAID
f88 = gen.f88

FAILS = []


def check(name, ok, detail=""):
    print("CASE %s %s%s" % (name, "pass" if ok else "FAIL", "" if ok else "  " + detail))
    if not ok:
        FAILS.append(name)


def build():
    out = GEN / "ot_boiler_sim"
    out.mkdir(parents=True, exist_ok=True)
    exe = out / "sim_driver.exe"
    if exe.exists():
        exe.unlink()
    bat = out / "_compile.bat"
    fwd = str(out).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W4 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{SIM / 'host' / 'sim_driver.cpp'}\" \"{SIM / 'src' / 'boiler_sim.cpp'}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print("COMPILATION FAILED")
        sys.exit(2)
    return exe


EXE = None


def run(scn, requests):
    """scn: path or scenario text. requests: [(t_s, frame_hex8)]. Returns [answer_hex8 or None], [delay]."""
    path = scn
    if not isinstance(scn, Path):
        path = GEN / "ot_boiler_sim" / "_inline.scn"
        path.write_text(scn, encoding="ascii")
    stdin = "".join("%d %s\n" % (int(round(t * 1000)), f) for t, f in requests)
    r = subprocess.run([str(EXE), str(path)], input=stdin, capture_output=True, text=True)
    if r.returncode != 0:
        return None, r.stderr.strip()
    ans, delays = [], []
    for line in r.stdout.splitlines():
        if line == "-":
            ans.append(None)
            delays.append(None)
        else:
            h, d = line.split()
            ans.append(h)
            delays.append(int(d))
    return ans, delays


def rd(mid):
    return gen.frame(READ_DATA, mid, 0)


def status_req(ch=True, dhw=True):
    return gen.frame(READ_DATA, 0, ((1 if ch else 0) | (2 if dhw else 0)) << 8)


def at(scn_name, reqs):
    ans, _ = run(DATA / scn_name, reqs)
    return ans


def main():
    global EXE
    EXE = build()

    # --- SB05: MsgID 25 Data-Invalid from 60 to 180 s, Ack outside (expected_wire: I, then recovers)
    di25 = lambda t: gen.frame(DATA_INVALID, 25, 0)
    ack25 = gen.frame(READ_ACK, 25, f88(40.0))
    ts = [30, 59.9, 60.0, 120, 179.9, 180.0, 600]
    ans = at("SB05-id25-data-invalid.scn", [(t, rd(25)) for t in ts])
    want = [ack25, ack25, di25(0), di25(0), di25(0), ack25, ack25]
    check("SB05_id25_data_invalid_60_to_180s", ans == want, "%s != %s" % (ans, want))
    ans = at("SB05-id25-data-invalid.scn", [(100, rd(28)), (100.5, status_req())])
    check("SB05_other_ids_still_ack", ans[0] == gen.frame(READ_ACK, 28, f88(30.0)) and ans[1] is not None, str(ans))

    # --- SB06: MsgID 25 Unknown-DataID from 60 s; a later Read-Ack retracts it (healthy hold)
    ans = at("SB06-id25-unknown.scn", [(10, rd(25)), (61, rd(25)), (179, rd(25)), (181, rd(25))])
    want = [ack25, gen.frame(UNKNOWN, 25, 0), gen.frame(UNKNOWN, 25, 0), ack25]
    check("SB06_id25_unknown_then_ack", ans == want, "%s != %s" % (ans, want))

    # --- SB07: no answer to MsgID 25 from 60 to 150 s; other IDs answered
    ans = at("SB07-id25-no-answer.scn", [(59, rd(25)), (61, rd(25)), (100, rd(28)), (149, rd(25)), (151, rd(25))])
    check("SB07_id25_silent_60_to_150s_others_answered",
          ans == [ack25, None, gen.frame(READ_ACK, 28, f88(30.0)), None, ack25], str(ans))

    # --- SB08: MsgID 28 Data-Invalid for 6 minutes from 30 s
    ack28 = gen.frame(READ_ACK, 28, f88(30.0))
    ans = at("SB08-id28-data-invalid.scn", [(29, rd(28)), (31, rd(28)), (389, rd(28)), (391, rd(28))])
    want = [ack28, gen.frame(DATA_INVALID, 28, 0), gen.frame(DATA_INVALID, 28, 0), ack28]
    check("SB08_id28_data_invalid_30_to_390s", ans == want, "%s != %s" % (ans, want))

    # --- SB09: MsgID 28 Unknown-DataID for 180 s, flow unaffected
    ans = at("SB09-id28-unknown.scn", [(1, rd(28)), (179, rd(28)), (2, rd(25)), (181, rd(28))])
    check("SB09_id28_unknown", ans == [gen.frame(UNKNOWN, 28, 0)] * 2 + [ack25, ack28], str(ans))

    # --- SB11: reactive slow ignition, flame 120 s after the master's CH enable, wherever it falls
    def lb(frame_hex):
        return int(frame_hex, 16) & 0xFF
    reqs = [(0, status_req(ch=False)), (40, status_req()), (41, rd(17)), (159.9, status_req()),
            (160.0, status_req()), (161, rd(17)),
            (300, status_req(ch=False)), (310, status_req()), (429.9, status_req()), (430.0, status_req())]
    ans = at("SB11-slow-ignition.scn", reqs)
    lbs = [lb(a) if a and int(a, 16) >> 16 & 0xFF == 0 else a for a in ans]
    want = [0x00, 0x02, gen.frame(READ_ACK, 17, 0), 0x02, 0x0A, gen.frame(READ_ACK, 17, f88(30.0)),
            0x00, 0x02, 0x02, 0x0A]
    check("SB11_flame_120s_after_ch_enable_and_after_re_enable", lbs == want, "%s != %s" % (lbs, want))

    # --- MsgID 0 echoes the master flags in the high byte
    ans = at("SB05-id25-data-invalid.scn", [(1, status_req(ch=True, dhw=True)), (2, status_req(ch=False, dhw=True))])
    check("ID0_echoes_master_high_byte", ans == [gen.frame(READ_ACK, 0, 0x030A), gen.frame(READ_ACK, 0, 0x0200)], str(ans))

    # --- MsgID 1 Write-Ack echoes SAT's TSet; read back returns it; MsgID 3 and writes SAT sends
    w1 = gen.frame(WRITE_DATA, 1, f88(55.5))
    w14 = gen.frame(WRITE_DATA, 14, f88(40.0))
    ans = at("SB05-id25-data-invalid.scn", [(1, w1), (2, rd(1)), (3, rd(3)), (4, w14), (5, rd(99)),
                                             (6, gen.frame(WRITE_DATA, 7, 0))])
    want = [gen.frame(WRITE_ACK, 1, f88(55.5)), gen.frame(READ_ACK, 1, f88(55.5)), gen.frame(READ_ACK, 3, 0x0004),
            gen.frame(WRITE_ACK, 14, f88(40.0)), gen.frame(UNKNOWN, 99, 0), gen.frame(UNKNOWN, 7, 0)]
    check("ID1_ID3_ID14_acks_and_unknown_ids", ans == want, "%s != %s" % (ans, want))

    # --- DHW draw: DHW active + flame, CH active cleared; CH resumes after the tap closes
    ans = at("SIM-dhw-draw.scn", [(10, status_req()), (70, status_req()), (71, rd(25)), (190, status_req())])
    check("DHW_draw_sets_dhw_and_flame_clears_ch",
          [lb(ans[0]), lb(ans[1]), ans[2], lb(ans[3])] == [0x0A, 0x0C, gen.frame(READ_ACK, 25, f88(55.0)), 0x0A], str(ans))

    # --- fault bit: set, no flame although CH is enabled
    ans, _ = run("scenario t\nphase 0 fault=1\n", [(1, status_req()), (200, status_req())])
    check("fault_bit_set_and_no_flame", [lb(a) for a in ans] == [0x01, 0x01], str(ans))

    # --- whole-link silence and a request with bad parity
    ans, _ = run("scenario t\nphase 0 silent=1\n", [(1, status_req()), (2, rd(25))])
    bad = "%08X" % (int(rd(25), 16) ^ 0x80000000)
    ans2, _ = run("scenario t\nphase 0\n", [(1, bad)])
    check("silent_and_bad_parity_get_no_answer", ans == [None, None] and ans2 == [None], "%s %s" % (ans, ans2))

    # --- answer delay inside the OT spec slave window; the parser refuses one outside it and unknown keys
    _, delays = run(DATA / "SB05-id25-data-invalid.scn", [(1, rd(25)), (61, rd(25))])
    r_bad_delay, e1 = run("scenario t\nphase 0 answer_ms=900\n", [(1, rd(25))])
    r_bad_key, e2 = run("scenario t\nphase 0 flame=1\n", [(1, rd(25))])
    check("answer_delay_100ms_and_parser_refusals",
          delays == [100, 100] and r_bad_delay is None and "answer_ms" in e1 and r_bad_key is None and "flame" in e2,
          "%s %s %s" % (delays, e1, e2))

    # --- every answer frame has even parity (independent check with the generator's parity_ok)
    allans = at("SB05-id25-data-invalid.scn", [(t, rd(m)) for t in (1, 70) for m in (0, 1, 3, 15, 17, 18, 25, 26, 27, 28)])
    check("answers_have_even_parity", all(a and gen.parity_ok(a) for a in allans), str(allans))

    # --- committed scenario files equal a fresh export
    r = subprocess.run([sys.executable, str(SIM / "export_scenarios.py"), "--check"], capture_output=True, text=True)
    check("committed_scn_files_match_export", r.returncode == 0, r.stdout + r.stderr)

    # --- sketch pins equal boards.h
    boards = (REPO / "src/libraries/Platform/src/boards.h").read_text(encoding="utf-8")
    sketch = (SIM / "src" / "main.cpp").read_text(encoding="utf-8")
    pins_ok, detail = True, []
    for name in ("PIN_OT_SLAVE_IN", "PIN_OT_SLAVE_OUT", "PIN_STEPUP_ENABLE", "PIN_BYPASS_RELAY"):
        b = re.findall(r"#define\s+%s\s+(\d+)" % name, boards)
        s = re.findall(r"SIM_%s\s*=\s*(\d+)" % name, sketch)
        if len(set(b)) != 1 or s != b[:1]:
            pins_ok = False
            detail.append("%s boards=%s sketch=%s" % (name, b, s))
    check("sketch_pins_match_boards_h", pins_ok, "; ".join(detail))

    print("RESULT:", "FAIL (%s)" % ", ".join(FAILS) if FAILS else "PASS")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
