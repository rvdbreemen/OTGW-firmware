#!/usr/bin/env python3
"""TASK-1412.20: generate the SAT bench-simulator scenario fixtures and their expected outputs.

One fixture per scenario, in the timed otframes.log layout the firmware replay reads (ADR-209):
    host_time TAB device_time TAB source TAB frame
plus one <id>.expected.json per scenario and an index.json for the run. The operator uploads a
fixture as /otgw_simulation.log and follows docs/sat/bench-simulator-1412-20.md.

What a fixture can prove is limited: the replay feeds lines to the decoder on the board, and the
firmware holds every command back from the bus while it runs (TASK-1580). Results are
"board-executed controller replay", never wire delivery or boiler proof. Replayed B frames do not
reach SAT's flow/return feeds (SATboilerFeed.h, satFeedEventForFrame), so every scenario names the
mode it needs: replay-only, satsim+replay, wire-slave-required (a real OpenTherm slave on the bus,
not available yet) or host-only.

Every expected output carries its source: a decision entry in docs/sat/sergeant-d-corrections.md,
a host test function, a code symbol or an ADR. An expectation no entry or test pins is written as
open (expected: open, ask). tests/test_sat_bench_scenarios_1412_20.py checks that each source exists.

Standard library only. Deterministic: the same version writes the same bytes.

Usage: python scripts/sat_bench_scenarios_1412_20.py [--out DIR] [--only ID[,ID...]]
Default out: build/sat-bench-1412-20 (gitignored)
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

VERSION = "1412.20-v1"
REPO = Path(__file__).resolve().parent.parent
DEFAULT_OUT = REPO / "build" / "sat-bench-1412-20"

# OpenTherm message types (spec v4.2, 3 bits at 28..30).
READ_DATA, WRITE_DATA, INVALID_DATA = 0, 1, 2
READ_ACK, WRITE_ACK, DATA_INVALID, UNKNOWN_DATAID = 4, 5, 6, 7

EXCHANGE_S = 1.0     # one request/answer pair per second, roughly a PIC monitor stream
ANSWER_S = 0.15      # boiler answer this long after the request
BASE_S = 12 * 3600   # device time of the first line: 12:00:00.000000 (no midnight inside a fixture)
MAX_GAP_S = 60.0     # the replay clamps a larger gap to 60 s (ADR-209)

# Poll rotation: MsgID 0 every other exchange (2 s), each other ID once per 14 s cycle (ID 25 well inside the 60 s flow window).
ROTATION = (0, 25, 0, 1, 0, 3, 0, 28, 0, 17, 0, 18, 0, 27)

DEFAULT_STATE = {
    "ch_enable": True, "dhw_enable": True,       # master status HB bits 0 and 1
    "fault": False, "ch": False, "dhw": False, "flame": False,   # slave status LB bits 0..3
    "slave_hb": 0x00, "member_id": 4,            # MsgID 3: HB flags (bit 4 = master low-off not allowed), LB member ID
    "tset": 40.0, "flow": 40.0, "ret": 30.0, "mod": 0.0, "pressure": 1.5, "outside": 8.0,
    "reply": {},                                 # MsgID -> "ack" (default) | "data_invalid" | "unknown" | "none"
    "silent": False,                             # True: no B line at all (whole-link silence)
}

# --------------------------------------------------------------------------- frames


def with_parity(word: int) -> int:
    """Set bit 31 so the 32-bit frame has even parity (OpenTherm spec v4.2)."""
    return word | (1 << 31) if bin(word).count("1") % 2 else word


def frame(msg_type: int, msg_id: int, value: int) -> str:
    return "%08X" % with_parity(((msg_type & 7) << 28) | ((msg_id & 0xFF) << 16) | (value & 0xFFFF))


def f88(celsius: float) -> int:
    return int(round(celsius * 256)) & 0xFFFF


def parity_ok(hex8: str) -> bool:
    return bin(int(hex8, 16)).count("1") % 2 == 0


def request_and_answer(msg_id: int, st: dict):
    """(request frame, answer type, answer value) for one exchange in state st."""
    master_hb = (1 if st["ch_enable"] else 0) | (2 if st["dhw_enable"] else 0)
    if msg_id == 0:
        slave_lb = (1 if st["fault"] else 0) | (2 if st["ch"] else 0) | (4 if st["dhw"] else 0) | (8 if st["flame"] else 0)
        return frame(READ_DATA, 0, master_hb << 8), READ_ACK, (master_hb << 8) | slave_lb
    if msg_id == 1:
        v = f88(st["tset"])
        return frame(WRITE_DATA, 1, v), WRITE_ACK, v
    if msg_id == 3:
        return frame(READ_DATA, 3, 0), READ_ACK, ((st["slave_hb"] & 0xFF) << 8) | (st["member_id"] & 0xFF)
    key = {17: "mod", 18: "pressure", 25: "flow", 27: "outside", 28: "ret"}[msg_id]
    return frame(READ_DATA, msg_id, 0), READ_ACK, f88(st[key])


def device_time(t_s: float) -> str:
    us = int(round((BASE_S + t_s) * 1_000_000))
    s, us = divmod(us, 1_000_000)
    return "%02d:%02d:%02d.%06d" % (s // 3600 % 24, s // 60 % 60, s % 60, us)


def host_time(t_s: float) -> str:
    # Synthetic: the replay reads only the last three columns. Same clock as device_time, so a reader can line them up.
    return "2026-11-05T%s+00:00" % device_time(t_s)[:12]


def build_timeline(phases, events=()):
    """phases: list of dicts {"duration": s, **state overrides}. events: (t_s, raw text) lines such as a PIC banner.
    Returns [(t_s, letter, payload)] sorted by time; payload is 8 hex digits for a frame."""
    rows = []
    t, k = 0.0, 0
    for ph in phases:
        st = dict(DEFAULT_STATE)
        st.update({key: v for key, v in ph.items() if key != "duration"})
        reply = dict(st.get("reply") or {})
        end = t + ph["duration"]
        while t < end - 1e-9:
            mid = ROTATION[k % len(ROTATION)]
            k += 1
            req, ans_type, ans_val = request_and_answer(mid, st)
            rows.append((round(t, 6), "T", req))
            mode = "none" if st["silent"] else reply.get(mid, "ack")
            if mode == "ack":
                rows.append((round(t + ANSWER_S, 6), "B", frame(ans_type, mid, ans_val)))
            elif mode == "data_invalid":
                rows.append((round(t + ANSWER_S, 6), "B", frame(DATA_INVALID, mid, int(req, 16) & 0xFFFF)))
            elif mode == "unknown":
                rows.append((round(t + ANSWER_S, 6), "B", frame(UNKNOWN_DATAID, mid, int(req, 16) & 0xFFFF)))
            elif mode != "none":
                raise ValueError("unknown reply mode %r" % mode)
            t = round(t + EXCHANGE_S, 6)
    for et, text in events:
        rows.append((round(et, 6), text[0], text[1:]))
    rows.sort(key=lambda r: r[0])
    return rows


def render(scenario, rows) -> str:
    head = [
        "# SAT bench simulator fixture %s (%s), TASK-1412.20, generator %s" % (scenario["id"], scenario["mode"], VERSION),
        "# %s" % scenario["title"],
        "# Synthetic: board-executed controller replay, not wire delivery. Columns: host_time TAB device_time TAB source TAB frame (ADR-209).",
        "# The replay starts again at the top after the last line; POST /api/v2/simulate/stop ends it.",
    ]
    body = ["%s\t%s\t%s\t%s" % (host_time(t), device_time(t), letter, payload) for t, letter, payload in rows]
    return "\n".join(head + body) + "\n"


# --------------------------------------------------------------------------- sources

def entry(n):  return {"kind": "entry", "ref": str(n)}
def test(path, name):  return {"kind": "test", "ref": "%s::%s" % (path, name)}
def code(path, symbol):  return {"kind": "code", "ref": "%s::%s" % (path, symbol)}
def adr(n):  return {"kind": "adr", "ref": "ADR-%03d" % n}


def exp(observable, where, value, *sources):
    return {"observable": observable, "where": where, "value": value, "sources": list(sources)}


def open_exp(observable, where, ask):
    return {"observable": observable, "where": where, "value": "expected: open, ask", "open": True, "ask": ask, "sources": []}


SLAVE = "tests/test_sat_slave_lowload.py"
FEED = "tests/test_sat_flow_return_validity.py"
LOSS = "tests/test_sat_ot_loss.py"
CACHE = "tests/test_sat_cmd_cache.py"
DELIV = "tests/test_sat_command_delivery.py"
ANCH = "tests/test_sat_pwm_anchors.py"
LIMIT = "tests/test_sat_pwm_start_limit.py"
FEEDH = "src/OTGW-firmware/SATboilerFeed.h"
CORE = "src/OTGW-firmware/OTGW-Core.ino"
SATC = "src/OTGW-firmware/SATcontrol.ino"

REPLAY_FEED_BOUNDARY = exp(
    "SATTICK fc/fage (flow) and rc/rage (return) in replay-only mode",
    "telnet (debug flag sat), SATTICK line",
    "fc=N fage=4294967295 and rc=N rage=4294967295 for the whole run, whatever the replayed MsgID 25/28 answers say: "
    "a replayed B frame is no boiler evidence and never stamps the feeds",
    code(FEEDH, "satFeedEventForFrame"), test(FEED, "test_only_a_boiler_reply_refreshes_the_flow"))
NO_PWM_WITHOUT_FLOW = exp(
    "control mode in replay-only mode",
    "REST /api/v2/sat (control_mode), MQTT sat/mode",
    "no PWM ON phase: the flow counts as missing, so PWM is idle and the continuous setpoint goes out",
    entry(218), entry(241), code(SATC, "satGetFlowTemp"))

# --------------------------------------------------------------------------- scenarios

SCENARIOS = [
    {
        "id": "SB01-support", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 3 Read-Ack with HB bit 4 = 0: the boiler allows master low-off (PWM permitted)",
        "phases": [{"duration": 120, "slave_hb": 0x00}],
        "preconditions": ["fresh boot (slave_cfg_valid is false until a valid MsgID 3 reply)"],
        "actions": [],
        "expected_replay": [
            exp("PWM permission", "REST /api/v2/sat", "slave_cfg_valid=true, slave_low_off_denied=false, pwm_veto_reason=0 within one MsgID 3 answer (<= 14 s)",
                entry(213), entry(215), test(SLAVE, "test_decoder_hook_stores_bit4_and_the_time_of_the_change"),
                test(SLAVE, "test_decoder_feeds_only_the_boilers_valid_read_ack")),
            exp("decoded slave flags", "MQTT <top>/value/<id>/slave_configuration", "00000000",
                code(CORE, "print_slavememberid")),
            REPLAY_FEED_BOUNDARY,
        ],
        "expected_wire": [],
        "does_not_prove": ["that a real boiler sends this reply", "PWM behaviour (flow is missing in replay-only mode)"],
    },
    {
        "id": "SB02-veto", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 3 HB bit 4 = 1 (master low-off not allowed): every PWM entry is refused",
        "phases": [{"duration": 120, "slave_hb": 0x10}],
        "preconditions": ["fresh boot", "SAT enabled and active (REST active=true)"],
        "actions": [{"t_s": 30, "action": "MQTT <top>/set/<id>/sat/control_mode = pwm"}],
        "expected_replay": [
            exp("PWM permission", "REST /api/v2/sat", "slave_cfg_valid=true, slave_low_off_denied=true",
                entry(213), test(SLAVE, "test_decoder_hook_stores_bit4_and_the_time_of_the_change")),
            exp("manual pwm command refused", "REST /api/v2/sat; telnet",
                "pwm_veto_reason=2, pwm_veto_path=3; telnet 'SAT: PWM refused (path 3): the boiler does not allow master low-off "
                "(MsgID 3 HB bit 4 = 1)'; MQTT sat/mode stays continuous",
                entry(213), test(SLAVE, "test_manual_pwm_command_is_blocked_without_permission"), code("src/OTGW-firmware/SATcycles.ino", "satPwmNoteRefused")),
            exp("decoded slave flags", "MQTT <top>/value/<id>/slave_configuration", "00010000", code(CORE, "print_slavememberid")),
        ],
        "expected_wire": [],
        "does_not_prove": ["the overshoot-switch and restart-restore entry paths (they need a valid flow, satsim or a wire slave)"],
    },
    {
        "id": "SB03-veto-during-pwm", "mode": "satsim+replay", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "Bit 4 turns 1 while PWM runs: PWM is left in that tick, the continuous setpoint goes out",
        "phases": [{"duration": 240, "slave_hb": 0x00}, {"duration": 240, "slave_hb": 0x10}],
        "preconditions": ["fresh boot", "SAT simulation on (SATsimulation=true): flow, return and flame come from the model",
                          "SAT enabled and active"],
        "actions": [{"t_s": 30, "action": "MQTT sat/control_mode = pwm; confirm sat/mode = pwm before t=240 s"},
                    {"t_s": 470, "action": "POST /api/v2/simulate/stop before the wrap (the file restarts with bit 4 = 0)"}],
        "expected_replay": [
            exp("veto exit", "REST /api/v2/sat; MQTT sat/mode; telnet",
                "within one control tick after the first MsgID 3 answer with bit 4 = 1 (t ~ 245 s): sat/mode continuous, "
                "pwm_veto_reason=2, pwm_veto_path=4; the next SAT-SIM trace CS is the continuous setpoint",
                entry(214), test(SLAVE, "test_bit4_rising_during_pwm_leaves_pwm_in_that_tick")),
        ],
        "expected_wire": [],
        "does_not_prove": ["boiler reaction; flame and flow are modelled, not measured"],
    },
    {
        "id": "SB04-id3-unknown", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 3 answered Unknown-DataID (no valid reply ever): no PWM",
        "phases": [{"duration": 120, "reply": {3: "unknown"}}],
        "preconditions": ["fresh boot (a valid reply from an earlier run stays until reboot)", "SAT enabled and active"],
        "actions": [{"t_s": 30, "action": "MQTT sat/control_mode = pwm"}],
        "expected_replay": [
            exp("PWM permission", "REST /api/v2/sat", "slave_cfg_valid=false; after the command pwm_veto_reason=1, pwm_veto_path=3; "
                "telnet 'SAT: PWM refused (path 3): no valid MsgID 3 reply from the boiler yet'",
                entry(215), test(SLAVE, "test_manual_pwm_command_is_blocked_without_permission"),
                test(SLAVE, "test_decoder_feeds_only_the_boilers_valid_read_ack")),
        ],
        "expected_wire": [],
        "does_not_prove": ["a boiler that answers MsgID 3 Data-Invalid or not at all after an earlier valid reply (open, see the procedure)"],
    },
    {
        "id": "SB05-id25-data-invalid", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 25 answered Data-Invalid for 120 s after 60 s of valid answers (replay boundary calibration)",
        "phases": [{"duration": 60}, {"duration": 120, "reply": {25: "data_invalid"}}, {"duration": 60}],
        "preconditions": ["SAT enabled and active", "debug flag sat on (SATTICK lines)"],
        "actions": [],
        "expected_replay": [REPLAY_FEED_BOUNDARY, NO_PWM_WITHOUT_FLOW],
        "expected_wire": [
            exp("flow feed class", "telnet SATTICK fc", "V while valid; I, then missing once the last valid sample is 60 s old; V again on the next valid answer",
                entry(218), entry(241), test(FEED, "test_flow_data_invalid_is_missing_once_the_last_valid_sample_is_60_s_old_and_recovers")),
        ],
        "does_not_prove": ["SAT's reaction to a boiler Data-Invalid: replayed answers never reach the feed; that needs a wire slave (AC1)"],
    },
    {
        "id": "SB06-id25-unknown", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 25 answered Unknown-DataID (replay boundary calibration)",
        "phases": [{"duration": 60}, {"duration": 120, "reply": {25: "unknown"}}],
        "preconditions": ["SAT enabled and active", "debug flag sat on"],
        "actions": [],
        "expected_replay": [
            REPLAY_FEED_BOUNDARY,
            exp("does-not-implement verdict", "REST /api/v2/otgw/ot-support", "MsgID 25 not marked unsupported by the replay",
                code(FEEDH, "satFeedEventForFrame"), test(FEED, "test_only_a_boiler_reply_refreshes_the_flow")),
        ],
        "expected_wire": [
            exp("flow feed class", "telnet SATTICK fc", "U (unsupported); a later Read-Ack retracts it",
                test(FEED, "test_flow_unknown_dataid_is_unsupported_and_a_later_ack_retracts_it")),
        ],
        "does_not_prove": ["the unsupported path; a replay sets no verdict"],
    },
    {
        "id": "SB07-id25-no-answer", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 25 not answered for 90 s while the other IDs are answered (stale flow)",
        "phases": [{"duration": 60}, {"duration": 90, "reply": {25: "none"}}, {"duration": 60}],
        "preconditions": ["SAT enabled and active", "debug flag sat on"],
        "actions": [],
        "expected_replay": [REPLAY_FEED_BOUNDARY,
                            exp("whole-link liveness", "MQTT sat/ot_link_lost", "stays false: other B answers keep arriving",
                                entry(79), test(LOSS, "test_a_alive_premise"))],
        "expected_wire": [
            exp("flow feed class", "telnet SATTICK fc/fage", "valid while fage < 60 s, missing from 60 s, valid on the next answer",
                entry(241), test(FEED, "test_flow_59_s_old_is_valid_and_61_s_old_is_missing"),
                test(FEED, "test_flow_valid_inside_60_s_stale_from_60_s_recovers_on_the_next_sample")),
        ],
        "does_not_prove": ["per-ID freshness on a real bus"],
    },
    {
        "id": "SB08-id28-data-invalid", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 28 answered Data-Invalid for 6 minutes (no-return path after 5 minutes)",
        "phases": [{"duration": 30}, {"duration": 360, "reply": {28: "data_invalid"}}],
        "preconditions": ["SAT enabled and active", "debug flag sat on", "no return sensor on the board (PIC .63: PR=E reads none)"],
        "actions": [],
        "expected_replay": [REPLAY_FEED_BOUNDARY],
        "expected_wire": [
            exp("return feed class", "telnet SATTICK rc/rsrc", "no return temperature (rsrc=-) once Data-Invalid has lasted 5 minutes",
                entry(217), test(FEED, "test_return_data_invalid_for_5_minutes_counts_as_no_return_temperature")),
        ],
        "does_not_prove": ["the no-return start-up (flood, MI=500): needs a wire slave and a CH ignition"],
    },
    {
        "id": "SB09-id28-unknown", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "MsgID 28 answered Unknown-DataID",
        "phases": [{"duration": 180, "reply": {28: "unknown"}}],
        "preconditions": ["SAT enabled and active", "debug flag sat on"],
        "actions": [],
        "expected_replay": [REPLAY_FEED_BOUNDARY],
        "expected_wire": [
            exp("return feed class", "telnet SATTICK rc", "U; no return temperature at once when no board sensor gives a reading",
                entry(84), test(FEED, "test_unknown_dataid_with_no_board_sensor_is_no_return_at_once_as_before"),
                test(FEED, "test_unknown_dataid_with_a_board_sensor_reading_is_not_no_return")),
        ],
        "does_not_prove": ["the boiler-side verdict (replay sets none)"],
    },
    {
        "id": "SB10-link-silence", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "Thermostat requests only, no boiler answer for 45 s, then answers again",
        "phases": [{"duration": 60}, {"duration": 45, "silent": True}, {"duration": 60}],
        "preconditions": ["SAT enabled and active", "SAT simulation OFF (the loss check is not used in simulation)", "debug flag sattrace on"],
        "actions": [{"t_s": 160, "action": "POST /api/v2/simulate/stop (the wrap would start a second loss)"}],
        "expected_replay": [
            exp("loss and hand-back", "MQTT sat/ot_link_lost; REST /api/v2/sat (ot_link_lost); telnet OTGW-SIM trace",
                "true about 30 s after the last B (t ~ 90 s); one hand-back CS=0, CH=1, MM=T held back by the replay, then no SAT "
                "command until the resume (judge from SAT's command traces only: CS=/CH=/MM= on .63, MID=1 and MID=14 on .61; "
                "the gateway's own requests also trace)",
                entry(47), entry(79), test(LOSS, "test_b_loss_hands_back_once_then_silence")),
            exp("resume", "MQTT sat/ot_link_lost", "false on the first B after t=105 s; SAT resumes by itself",
                entry(79), test(LOSS, "test_d_resume_on_first_reply")),
        ],
        "expected_wire": [],
        "does_not_prove": ["that the hand-back reaches a boiler (held back by the replay)"],
    },
    {
        "id": "SB11-slow-ignition", "mode": "wire-slave-required", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "CH request, flame appears 120 s later (slow ignition); replay variant feeds the flame edge only",
        "phases": [{"duration": 30}, {"duration": 120, "ch": True}, {"duration": 240, "ch": True, "flame": True, "mod": 30.0}],
        "preconditions": ["replay variant: SAT enabled, simulation off"],
        "actions": [],
        "expected_replay": [
            exp("flame edge decoded", "MQTT <top>/value/<id>/flamestatus", "OFF until t=150 s, then ON", code(CORE, "processOT")),
            NO_PWM_WITHOUT_FLOW,
        ],
        "expected_wire": [
            exp("PWM ON timing (reactive slave: flame 120 s after CH=1)", "telnet SAT log; wire capture",
                "the ON phase re-anchors at the ignition and lasts max(on_time, 180 s) from it; no ignition abort",
                entry(80), test(ANCH, "test_d3_slow_ignition_at_120_s_times_on_from_the_ignition"),
                test(ANCH, "test_d8_late_ignition_keeps_on_until_180_s_after_flame_on")),
        ],
        "does_not_prove": ["the PWM ignition wait: a fixed timeline cannot answer SAT's own CH/CS; this needs a reactive wire slave"],
    },
    {
        "id": "SB12-lockout-fault", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "Boiler fault bit set, no flame for 120 s (lockout), then cleared",
        "phases": [{"duration": 60}, {"duration": 120, "fault": True, "ch": False}, {"duration": 60}],
        "preconditions": ["SAT enabled and active", "debug flag sattrace on"],
        "actions": [],
        "expected_replay": [
            exp("resend pause", "telnet OTGW-SIM trace (SAT command lines only)", "no 5 s CS/MM/CH resend while the fault bit is set",
                entry(221), test(CACHE, "test_live_fault_and_dhw_bits_pause_the_resend"), code(SATC, "satCmdResend")),
            exp("control cycle skipped", "telnet (debug flag sat)",
                "'SAT: boiler fault flag detected, skipping control cycle' on each control tick while MsgID 0 slave bit 0 is set",
                code(SATC, "boiler fault flag detected")),
        ],
        "expected_wire": [
            exp("unlit ON phase (reactive slave refusing to light)", "telnet SAT log; wire capture",
                "an ON phase that never ignites ends at on_time after its start and counts once as a start",
                entry(80), test(ANCH, "test_d4_no_flame_within_180_s_does_not_abort_on"), test(ANCH, "test_e1_unlit_on_start_counts_once"),
                test(LIMIT, "test_flameless_on_restarts_count_against_the_limit")),
        ],
        "does_not_prove": ["an anti-cycle delay inside the boiler (no OT bit for it); needs a reactive wire slave"],
    },
    {
        "id": "SB13-dhw-draw", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "Hot-water draw for 120 s (DHW bit + flame), SAT in continuous mode",
        "phases": [{"duration": 60}, {"duration": 120, "dhw": True, "flame": True, "mod": 80.0}, {"duration": 330}],
        "preconditions": ["SAT enabled and active", "debug flags sat and sattrace on"],
        "actions": [],
        "expected_replay": [
            exp("MM during the draw", "telnet OTGW-SIM trace; SATTICK dhw=1",
                "MM to the maximum for the draw; on PIC (.63) the 30 s CS keepalive keeps running; no 5 s CS/MM/CH resend",
                entry(220), entry(221), entry(232),
                test(CACHE, "test_dhw_draw_sends_mm_max_keeps_the_keepalive_and_drops_the_resend")),
        ],
        "expected_wire": [],
        "does_not_prove": ["hot-water delivery; MM is held back by the replay"],
    },
    {
        "id": "SB14-dhw-during-pwm", "mode": "satsim+replay", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "Hot-water bit during a PWM run (DHW bit from the replay, flame from the model)",
        "phases": [{"duration": 300}, {"duration": 90, "dhw": True}, {"duration": 330}],
        "preconditions": ["fresh boot", "SAT simulation on", "SAT enabled and active", "MsgID 3 bit 4 = 0 (in this fixture)"],
        "actions": [{"t_s": 30, "action": "MQTT sat/control_mode = pwm"},
                    {"t_s": 715, "action": "POST /api/v2/simulate/stop"}],
        "expected_replay": [
            exp("no PWM ON start while a hot-water flame burns", "telnet SAT log", "no ON start during the draw; start after the flame is out, counted",
                entry(106), entry(234), test(ANCH, "test_h1_no_on_start_while_the_hot_water_flame_burns")),
            exp("hot-water flame not counted", "telnet SAT log (PWM start count)", "a flame during the draw adds no PWM start",
                entry(89), test(ANCH, "test_e3_hot_water_flame_inside_on_adds_no_count")),
        ],
        "expected_wire": [],
        "does_not_prove": ["a real draw: the replayed DHW bit and the modelled flame are not one boiler"],
    },
    {
        "id": "SB15-pic-restart-banner", "mode": "replay-only", "transports": ["PIC .63"],
        "title": "PIC restart banner in the stream at t=60 s: SAT sends MM and CH again",
        "phases": [{"duration": 120}],
        "events": [(60.5, "OpenTherm Gateway 6.8")],
        "preconditions": ["SAT enabled and active", "debug flag sattrace on"],
        "actions": [],
        "expected_replay": [
            exp("resend after a PIC restart", "telnet OTGW-SIM trace", "MM and CH (and CS) once more with the latest values, right after the banner",
                entry(237), entry(238), test(CACHE, "test_mm_and_ch_only_on_change_and_again_after_pic_restart"),
                test(DELIV, "test_pic_reset_resends_all_three_with_the_latest_values")),
        ],
        "expected_wire": [],
        "does_not_prove": ["a real PIC reset (the PIC was not reset)"],
    },
    {
        "id": "SB16-rest-burst", "mode": "replay-only", "transports": ["PIC .63", "OT-Direct .61"],
        "title": "REST burst above the cap of 2 while SB01 replays (deliberate, bounded fault injection)",
        "phases": None, "uses_fixture": "SB01-support",
        "preconditions": ["bench unit only; the burst deliberately exceeds the ADR-165 cap of 2 in flight"],
        "actions": [{"t_s": 30, "action": "host: 30 s of 4 concurrent GET /api/v2/sat, logged with host timestamps"}],
        "expected_replay": [
            exp("excess requests", "host burst log", "HTTP 503 for requests above the cap; no reboot", adr(165)),
            open_exp("SAT control tick timing during the burst", "MQTT otgw-firmware/stats/max_loop_gap_ms; SATTICK spacing",
                     "Robert: is a bound on SAT tick delay under a REST burst wanted, and which?"),
        ],
        "expected_wire": [],
        "does_not_prove": ["the PIC command queue (see SB17)"],
    },
    {
        "id": "SB17-pic-queue-full", "mode": "host-only", "transports": [],
        "title": "PIC command queue full: rejected command never cached as delivered",
        "phases": None,
        "preconditions": [],
        "actions": [],
        "expected_replay": [],
        "expected_wire": [
            exp("full queue", "host test only", "a rejected enqueue is not cached as delivered; a superseded command never reaches the wire",
                test(DELIV, "test_full_queue_rejects_and_the_rejected_state_is_never_cached_as_delivered"),
                test(DELIV, "test_superseded_command_never_reaches_the_wire")),
        ],
        "does_not_prove": ["anything on the bench: during a replay commands are held back before the PIC queue (TASK-1580)"],
    },
]


def scenario_rows(sc):
    if not sc.get("phases"):
        return None
    return build_timeline(sc["phases"], sc.get("events", ()))


def expected_doc(sc, fixture_name, rows, digest):
    return {
        "id": sc["id"], "title": sc["title"], "generator_version": VERSION, "mode": sc["mode"],
        "label": "board-executed controller replay" if sc["mode"] in ("replay-only", "satsim+replay") else sc["mode"],
        "transports": sc["transports"],
        "fixture": fixture_name, "fixture_sha256": digest,
        "uses_fixture": sc.get("uses_fixture"),
        "duration_s": (rows[-1][0] if rows else None),
        "phases": sc.get("phases"), "events": [list(e) for e in sc.get("events", ())],
        "preconditions": sc["preconditions"], "actions": sc["actions"],
        "expected_replay": sc["expected_replay"], "expected_wire": sc["expected_wire"],
        "does_not_prove": sc["does_not_prove"],
    }


def generate(out: Path, only=None):
    out.mkdir(parents=True, exist_ok=True)
    index = {"generator_version": VERSION, "scenarios": []}
    for sc in SCENARIOS:
        if only and sc["id"] not in only:
            continue
        rows = scenario_rows(sc)
        name, digest = None, None
        if rows is not None:
            text = render(sc, rows)
            name = "%s.otgw_simulation.log" % sc["id"]
            (out / name).write_bytes(text.encode("ascii"))
            digest = hashlib.sha256(text.encode("ascii")).hexdigest()
        doc = expected_doc(sc, name, rows, digest)
        (out / ("%s.expected.json" % sc["id"])).write_text(json.dumps(doc, indent=2) + "\n", encoding="ascii")
        index["scenarios"].append({"id": sc["id"], "mode": sc["mode"], "fixture": name, "sha256": digest})
    (out / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="ascii")
    return index


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--out", default=str(DEFAULT_OUT), help="output folder (default %(default)s)")
    p.add_argument("--only", default="", help="comma separated scenario IDs")
    a = p.parse_args(argv)
    only = {s.strip() for s in a.only.split(",") if s.strip()} or None
    index = generate(Path(a.out), only)
    for s in index["scenarios"]:
        print("%-24s %-20s %s %s" % (s["id"], s["mode"], s["fixture"] or "-", (s["sha256"] or "")[:12]))
    print("generator %s -> %s" % (VERSION, a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
