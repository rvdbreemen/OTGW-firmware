#!/usr/bin/env python3
"""TASK-1202 host harness: every Home Assistant discovery payload is valid JSON.

    python test/host/test_ha_discovery_json.py                       # FIX and the positive control
    python test/host/test_ha_discovery_json.py --old-rev 8795bacc0   # plus OLD and the old-vs-fix diff

Home Assistant drops a discovery config it cannot parse ("Unable to parse JSON"), and a
button, select, switch or number without a command topic cannot act. On 8795bacc0 the SAT
Heating System select and the Reset Gateway button wrote their command topic without the
"cmd_t" key, so both payloads were invalid JSON.

The code under test comes from the revision, never copied by hand: MQTTHaDiscovery.cpp,
MQTTstuff.h and boards.h verbatim (MQTTHaDiscovery.cpp compiles as its own translation unit,
as on the device) and slices by anchor of MQTTstuff.ino, OTGW-Core.h, OTGW-Core.ino,
sensors_ext.ino and five headers (listed in test_ha_discovery_json.cpp). Only the platform
(ha_disc_shim/) and the edges (publish, MQTT connection, SAT zone state, the 1-Wire bus in
part B) are doubled. The program runs:
  A  every composer directly over its input domain: each sensor and binary-sensor row (a
     real OT id also as Thermostat), the source variants, climate, number, the override
     sensors, the SAT switches and select, the button, the GPIO/LED selects, the SAT zone
     and PV boost composer, and Dallas through sensorAutoConfigure(); both OT engines, with
     and without the full device block; an out-of-range index must publish nothing; the
     BLE sensor composer satBLEPublishHaDiscovery() and its remover satBLEUnpublishDiscovery();
  B  doAutoConfigureMsgid() for every id 0..255 under four settings permutations;
  C  clearTopologyDiscoveryForOTId() for every id: removal publishes, which must be empty.

The BLE composer formats PROGMEM strings with "%S" through snprintf_P, which is snprintf on
the ESP32 core. Both GCC and MSVC read "%S" as a wchar_t* string, so on the host it returns
-1 and the composer publishes nothing. Its result is printed as a probe, outside the verdict.

The gate, per publish: retained, strict UTF-8, strict JSON (no NaN or Infinity, nothing
after the value; also orjson.loads(), Home Assistant's parser, when orjson is installed),
an object, and the command key its component needs: cmd_t under the set/
namespace for button, select, switch and number, temp_cmd_t for climate, no command topic on
a sensor or binary_sensor. Other schema findings (discovery topic pattern, duplicate keys,
state topic, options, number range) are advisory: printed, never part of the verdict.

With --old-rev the same program is built from that revision. OLD must fail the gate in
exactly the two known composers (and in B on exactly their two topics). Every other payload
must be byte-identical between OLD and FIX, and each fixed payload must be its OLD twin with
'"cmd_t":"' inserted once, in front of the command topic. The OLD parse errors must sit
where the broker on the rig rejected them: char 291 for the select with the full device
block, char 153 for the button with the short one.

Positive control M1: a FIX copy with the number entity's cmd_t key renamed (valid JSON, no
command topic). The gate must flag that composer and nothing else.

Exit code: 0 PASS, 1 FAIL, 2 harness error (a slice, compile or run failure).
"""
import argparse
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

from test_dhw_water_meter import construct, line, mutate, read, typedef_ending
from test_ot_reserved_range import GEN, HERE, find_vcvars

try:
    import orjson    # Home Assistant's parser: json_loads_object() in homeassistant/util/json.py
except ImportError:
    orjson = None

FW = "src/OTGW-firmware/"
DISC, MQTT_H, MQTT_INO = FW + "MQTTHaDiscovery.cpp", FW + "MQTTstuff.h", FW + "MQTTstuff.ino"
BOARDS_H = "src/libraries/Platform/src/boards.h"
CORE_H, CORE_INO, FW_H, HELPER_H = FW + "OTGW-Core.h", FW + "OTGW-Core.ino", FW + "OTGW-firmware.h", FW + "helperStuff.h"
HW_H, DEV_H, SENS_H, SENS_INO = FW + "Hardwaretypes.h", FW + "Devicetypes.h", FW + "Sensorstypes.h", FW + "sensors_ext.ino"
CPP, SHIM = "test_ha_discovery_json.cpp", HERE / "ha_disc_shim"

# The two composers TASK-1202 fixes, with the config topic each one owns.
KNOWN = {"streamSatSelectDiscovery": "select/{node}/sat_heating_system/config",
         "streamButtonDiscovery": "button/{node}/resetgateway/config"}
# Where the rig's broker failed json.loads (first=1: full device block).
BENCH = {("streamSatSelectDiscovery", "first=1"): 291, ("streamButtonDiscovery", "first=0"): 153}
CMD_KEY = {"button": "cmd_t", "select": "cmd_t", "switch": "cmd_t", "number": "cmd_t", "climate": "temp_cmd_t"}
READ_ONLY = {"sensor", "binary_sensor"}
REMOVAL = "clearTopologyDiscoveryForOTId"
REMOVERS = {REMOVAL, "satBLEUnpublishDiscovery"}
# The BLE composer formats with "%S" through snprintf_P (= snprintf); what that does is up to the
# C library (MSVC here, newlib on the device), so its result is reported apart from the gate.
BLE_PROBE = "satBLEPublishHaDiscovery"
DISPATCH = "doAutoConfigureMsgid"
# homeassistant/components/mqtt/discovery.py: TOPIC_MATCHER, applied with .match() to the topic
# without "<prefix>/"; a config on a topic it does not match is dropped ("illegal discovery topic").
TOPIC_MATCHER = re.compile(r"(?P<component>\w+)/(?:(?P<node_id>[a-zA-Z0-9_-]+)/)"
                           r"?(?P<object_id>[a-zA-Z0-9_-]+)/config")
INSERT = b'"cmd_t":"'
NUMBER_CMD = ('    if (!w.writeProgmem(PSTR("\\"cmd_t\\":\\""))) return false;\n'
              '    if (!w.writeRam(ctx.mqttSubTopic)) return false;\n'
              '    if (!w.writeProgmem(PSTR("/outside\\""))) return false;\n')
M1 = [(NUMBER_CMD, NUMBER_CMD.replace("cmd_t", "cmd_x"))]


def write(out_dir, name, origin, parts):
    head = f"// GENERATED by test_ha_discovery_json.py from {origin}. Do not edit.\n"
    (out_dir / name).write_text(head + "\n\n".join(parts) + "\n", encoding="utf-8")


def override_ids(mino):
    """The ids doAutoConfigureMsgid() gives an override sensor: its one 'case a: case b: ... {' line."""
    body = construct(mino, r"^bool doAutoConfigureMsgid\(byte OTid, bool isFirst\)\s*$").splitlines()
    hits = [l for l in body if re.match(r"^\s*(case \d+:\s*)+\{\s*$", l)]
    if len(hits) != 1:
        raise SystemExit(f"the override case line must match once in doAutoConfigureMsgid(), got {len(hits)}")
    return [int(n) for n in re.findall(r"case (\d+):", hits[0])]


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    origin = f"git {rev}" if rev else "the working tree"
    for path in (DISC, MQTT_H, BOARDS_H):                 # verbatim
        (out_dir / Path(path).name).write_text(read(path, rev), encoding="utf-8")
    src = {p: read(p, rev).splitlines() for p in (MQTT_INO, CORE_H, CORE_INO, FW_H, HELPER_H, HW_H, DEV_H, SENS_H, SENS_INO)}
    ch, fwh, mino = src[CORE_H], src[FW_H], src[MQTT_INO]
    write(out_dir, "gen_types.inc", origin, [
        construct(src[HW_H], r"^enum OTGWHardwareMode\b"),
        construct(src[DEV_H], r"^struct DeviceSection\b"),
        construct(src[SENS_H], r"^struct SensorsSection\b"),
        line(src[HELPER_H], r"^#define PROGMEM_readAnything\(src, dest\)"),
        typedef_ending(ch, "OTdataStruct"),
        line(ch, r"^static OTdataStruct OTcurrentSystemState;"),
        construct(ch, r"^enum OTLibMessageID\b"),
        construct(ch, r"^\s*enum OTtype_t\b"),
        construct(ch, r"^\s*enum OTmsgcmd_t\b"),
        construct(ch, r"^\s*struct OTlookup_t\b"),
        line(ch, r"^\s*OTlookup_t OTlookupitem;"),
        construct(ch, r"^\s*const OTlookup_t OTmap\[\] PROGMEM = \{"),
        line(ch, r"^#define OT_MSGID_MAX\b"),
        line(fwh, r"^inline const char\* CSTR\(const char\* x\)"),
        line(fwh, r"^inline const char\* CSTR\(char\* x\)"),
        *[line(fwh, rf"^byte\s+{n}\s*=") for n in ("OTGWdallasdataid", "OTGWpiccontrolsid", "OTGWsatzoneid")],
        line(fwh, r"^uint32_t\s+MQTTautoConfigMap\[8\]"),
    ])
    write(out_dir, "gen_core.inc", origin, [
        construct(src[CORE_INO], r"^const char \*messageIDToString\(OTLibMessageID message_id\)\{"),
    ])
    write(out_dir, "gen_dallas.inc", origin, [
        line(src[SENS_INO], r"^const int SIM_SENSOR_COUNT\s*="),
        construct(src[SENS_INO], r"^const uint8_t DallasSimDeviceAddresses\[SIM_SENSOR_COUNT\]\[8\] = \{"),
        construct(src[SENS_INO], r"^char\* getDallasAddress\(DeviceAddress deviceAddress\)\s*$"),
    ])
    write(out_dir, "gen_mqtt.inc", origin, [
        line(mino, r"^constexpr size_t\s+MQTT_ID_MAX_LEN\b"),
        line(mino, r"^constexpr size_t\s+MQTT_NAMESPACE_MAX_LEN\b"),
        line(mino, r"^const char kPicSubtreePrefix\[\] PROGMEM = "),   # after MQTTstuff.h's extern: external linkage
        line(mino, r"^static bool mqttAutoConfigInProgress\b"),
        construct(mino, r"^struct MQTTAutoConfigSessionLock\s*\{"),
        line(mino, r"^static char\s+MQTTPubNamespace\["),
        line(mino, r"^static char\s+MQTTSubNamespace\["),
        line(mino, r"^static char\s+NodeId\["),
        construct(mino, r"^static void buildNamespace\(char \*dest, size_t destSize, const char \*base, "
                        r"const char \*segment, const char \*node\) \{"),
        construct(mino, r"^bool getMQTTConfigDone\(const uint8_t MSGid\)\s*$"),
        construct(mino, r"^void setMQTTConfigDone\(const uint8_t MSGid\)\s*$"),
        construct(mino, r"^static HaDiscoveryContext buildDiscoveryContext\(bool isFirst = false\) \{"),
        construct(mino, r"^static bool publishDiscoveryJson\(const char \*topic,\s*$"),
        construct(mino, r"^static void mqttJsonEscape\(const char\* src, char\* dst, size_t dstSize\)\s*$"),
        line(mino, r"^constexpr size_t\s+MQTT_TOPIC_MAX_LEN\b"),
        construct(mino, r"^static bool satBLEPublishOneDiscovery\(const char\* macCompact,\s*$"),
        construct(mino, r"^bool satBLEPublishHaDiscovery\(const char\* macCompact, const char\* macWithColons,\s*$"),
        construct(mino, r"^void satBLEUnpublishDiscovery\(const char\* macCompact\)\s*$"),
        construct(mino, r"^static bool buildDiscoveryDeviceBlock\(char \*dest, size_t destSize, HaDiscoveryContext &ctx\)\s*$"),
        construct(mino, r"^static bool streamSatPvBoostDiscovery\(HaDiscoveryContext &ctx\)\s*$"),
        construct(mino, r"^bool streamSatZoneDiscovery\(HaDiscoveryContext &ctx\)\s*$"),
        construct(mino, r"^static HaDevice deviceForOTId\(byte OTid\) \{"),
        construct(mino, r"^bool doAutoConfigureMsgid\(byte OTid, bool isFirst\)\s*$"),
        construct(mino, r"^void sensorAutoConfigure\(byte dataid, bool finishflag, const char \*cfgSensorId = nullptr\) \{"),
    ])
    write(out_dir, "gen_override_ids.inc", origin,
          [f"static const uint8_t kOverrideIds[] = {{ {', '.join(map(str, override_ids(mino)))} }};"])


def entry_points(rev):
    """The public composers: non-static bool/uint8_t functions defined in MQTTHaDiscovery.cpp, and
    every stream*Discovery() MQTTstuff.h declares (streamSatZoneDiscovery() lives in MQTTstuff.ino)."""
    defined = re.findall(r"^(?:bool|uint8_t)\s+(\w+)\s*\(", read(DISC, rev), re.M)
    declared = re.findall(r"^bool\s+(stream\w*Discovery)\s*\(", read(MQTT_H, rev), re.M)
    return sorted(set(defined) | set(declared))


def build_and_run(label, out_dir):
    exe = out_dir / "test_ha_discovery_json.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f'call "{find_vcvars()}" >nul 2>nul\r\n'
                   "cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DBOARD_NODOSHOP_ESP32_COMBO "
                   f'/Fo:"{fwd}/" /Fe:"{exe}" "{HERE / CPP}" "{out_dir / "MQTTHaDiscovery.cpp"}" '
                   f'/I"{out_dir}" /I"{SHIM}"\r\n', encoding="ascii", newline="")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-6000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace")
    lines = r.stdout.splitlines()
    if not lines or not lines[-1].startswith("END "):
        print(r.stdout[-3000:], r.stderr[-2000:])
        print(f"HARNESS DID NOT FINISH ({label}, exit code {r.returncode})")
        sys.exit(2)
    return parse(lines)


def parse(lines):
    ctx, calls, pubs = {}, [], []
    for l in lines:
        f = l.split(" ")
        if f[0] == "CTX":
            ctx = dict(kv.split("=", 1) for kv in f[1:])
        elif f[0] == "CALL":
            calls.append({"seq": int(f[1]), "tag": f[2], "args": f[3], "ret": int(f[4][4:]),
                          "pubs": int(f[5][5:]), "ok": f[6] == "ok=1"})
        elif f[0] == "PUB":
            pubs.append({"seq": int(f[1]), "tag": f[2], "retain": f[3] == "1",
                         "topic": b"" if f[4] == "-" else bytes.fromhex(f[4]),
                         "raw": b"" if f[5] == "-" else bytes.fromhex(f[5])})
    args = {c["seq"]: c["args"] for c in calls}
    for p in pubs:
        p["args"] = args[p["seq"]]
        p["topic"] = p["topic"].decode("utf-8", errors="replace")
    return ctx, calls, pubs


def strict_json(raw):
    """(object, error, duplicate keys). Strict UTF-8 and stdlib JSON without NaN or Infinity (its
    messages give the char offset), then Home Assistant's own rule when orjson is installed:
    orjson.loads() and a dict at the top (json_loads_object())."""
    dups = []

    def pairs(items):
        seen = set()
        for k, _ in items:
            if k in seen:
                dups.append(k)
            seen.add(k)
        return dict(items)

    def constant(name):
        raise ValueError(f"{name} is not JSON")

    try:
        obj = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
    except ValueError as e:                     # UnicodeDecodeError and JSONDecodeError included
        return None, str(e), dups
    if not isinstance(obj, dict):
        return None, f"top level is {type(obj).__name__}, not an object", dups
    if orjson is not None:
        try:
            if not isinstance(orjson.loads(raw), dict):
                return None, "orjson: top level is not an object", dups
        except orjson.JSONDecodeError as e:
            return None, f"orjson: {e}", dups
    return obj, None, dups


def component(topic, ctx):
    return topic[len(ctx["prefix"]) + 1:].split("/")[0]


def gate(p, ctx):
    """Why this publish fails the gate; an empty list passes."""
    if p["tag"] in REMOVERS:
        return [] if p["raw"] == b"" and p["retain"] else ["removal publish is not an empty retained payload"]
    bad = [] if p["retain"] else ["not retained"]
    obj, err, _ = strict_json(p["raw"])
    if err:
        return bad + [f"invalid JSON: {err}"]
    comp, sub = component(p["topic"], ctx), ctx["sub"] + "/"
    if comp in CMD_KEY:
        v = obj.get(CMD_KEY[comp])
        if not (isinstance(v, str) and v.startswith(sub) and len(v) > len(sub)):
            bad.append(f"{comp} without {CMD_KEY[comp]} under {sub} (got {v!r})")
    elif comp in READ_ONLY:
        if "cmd_t" in obj or "command_topic" in obj:
            bad.append(f"{comp} with a command topic")
    else:
        bad.append(f"component {comp!r} unknown to the gate")
    return bad


def advisory(p, ctx):
    """Schema findings outside the gate."""
    notes = []
    pfx = ctx["prefix"] + "/"
    if not (p["topic"].startswith(pfx) and TOPIC_MATCHER.match(p["topic"][len(pfx):])):
        notes.append("discovery topic outside Home Assistant's topic pattern")
    if p["tag"] in REMOVERS:
        return notes
    obj, err, dups = strict_json(p["raw"])
    if err:
        return notes
    comp = component(p["topic"], ctx)
    if dups:
        notes.append("duplicate keys " + ",".join(sorted(set(dups))))
    if not (isinstance(obj.get("uniq_id"), str) and obj["uniq_id"]):
        notes.append("no uniq_id")
    dev = obj.get("dev", obj.get("device"))
    if not (isinstance(dev, dict) and (dev.get("identifiers") or dev.get("ids"))):
        notes.append("no device identifiers")
    if comp in READ_ONLY and not obj.get("stat_t"):
        notes.append("no stat_t")
    for k, v in obj.items():
        if (k.endswith("_t") or k.endswith("_topic")) and not (isinstance(v, str) and v and "+" not in v and "#" not in v):
            notes.append(f"topic field {k} is {v!r}")
    if comp == "select":
        o = obj.get("options")
        if not (isinstance(o, list) and o and all(isinstance(x, str) for x in o)):
            notes.append("select options are not a non-empty list of strings")
    if comp == "number":
        lo, hi, st = obj.get("min"), obj.get("max"), obj.get("step")
        if not (all(isinstance(x, (int, float)) for x in (lo, hi, st)) and lo < hi and st > 0):
            notes.append(f"number range min={lo!r} max={hi!r} step={st!r}")
    return notes


def evaluate(label, run):
    """Print the per-composer table; return (failing tags, failing dispatcher topics, advisory notes)."""
    ctx, calls, pubs = run
    per = defaultdict(Counter)
    fails, adv = defaultdict(list), defaultdict(list)
    for c in calls:
        per[c["tag"]]["calls"] += 1
        per[c["tag"]]["unexpected"] += not c["ok"]
    for p in pubs:
        per[p["tag"]]["payloads"] += 1
        reasons = gate(p, ctx)
        if reasons:
            per[p["tag"]]["failed"] += 1
            fails[p["tag"]].append((p, reasons))
        for n in advisory(p, ctx):
            adv[n].append(p)
    print(f"== {label}: {len(calls)} calls, {len(pubs)} publishes ({ctx['prefix']}, {ctx['sub']}) ==")
    for tag in sorted(per):
        k = per[tag]
        state = "pass" if not (k["failed"] or k["unexpected"]) else "FAIL"
        print(f"  {tag}: {k['calls']} calls ({k['unexpected']} with an unexpected result), "
              f"{k['payloads']} payloads, {k['failed']} fail the gate -> {state}")
    for c in calls:
        if not c["ok"]:
            print(f"    unexpected: {c['tag']} {c['args']} ret={c['ret']} pubs={c['pubs']}")
    for tag in sorted(fails):
        for p, reasons in fails[tag][:2]:
            print(f"    {tag} [{p['args']}] {p['topic']}: {'; '.join(reasons)}")
        if len(fails[tag]) > 2:
            print(f"    ... and {len(fails[tag]) - 2} more {tag} payloads")
    for note in sorted(adv):
        ps = adv[note]
        tags = Counter(p["tag"] for p in ps)
        print(f"  advisory: {note}: {len(ps)} payloads ({', '.join(f'{t} {n}' for t, n in sorted(tags.items()))})")
        for t in sorted({p["topic"] for p in ps}):
            print(f"      {t}")
    probe = [c for c in calls if c["tag"] == BLE_PROBE]
    print(f"  probe {BLE_PROBE} (not in the verdict): returns {[c['ret'] for c in probe]}, "
          f"{sum(c['pubs'] for c in probe)} payloads, {len(fails.get(BLE_PROBE, []))} fail the gate")
    bad_topics = {p["topic"] for p, _ in fails.get(DISPATCH, [])}
    unexpected = {c["tag"] for c in calls if not c["ok"]}
    return (set(fails) | unexpected) - {BLE_PROBE}, bad_topics, adv


def known_topics(ctx):
    return {f"{ctx['prefix']}/{t.format(node=ctx['node'])}" for t in KNOWN.values()}


def diff_old_fix(old, fix):
    """(ok, text): FIX equals OLD byte for byte, except each payload on a known topic, which is its
    OLD twin with '"cmd_t":"' inserted once in front of the command topic."""
    (octx, _, opubs), (fctx, _, fpubs) = old, fix
    if octx != fctx or len(opubs) != len(fpubs):
        return False, f"different runs: {len(opubs)} vs {len(fpubs)} publishes"
    known, sub = known_topics(fctx), (fctx["sub"] + "/").encode()
    same = inserted = 0
    for o, f in zip(opubs, fpubs):
        if (o["tag"], o["topic"], o["retain"], o["args"]) != (f["tag"], f["topic"], f["retain"], f["args"]):
            return False, f"publish order differs at {f['tag']} [{f['args']}] {f['topic']}"
        if o["raw"] == f["raw"] and f["topic"] not in known:
            same += 1
            continue
        i = f["raw"].find(INSERT)
        if not (f["topic"] in known and f["raw"].count(INSERT) == 1 and
                f["raw"][:i] + f["raw"][i + len(INSERT):] == o["raw"] and o["raw"][i:].startswith(sub)):
            return False, f"unexpected change: {f['tag']} [{f['args']}] {f['topic']}"
        inserted += 1
    return True, f"{same} payloads identical, {inserted} on the two topics = OLD + '\"cmd_t\":\"' before {fctx['sub']}/"


def bench_offsets(old):
    """(ok, lines): OLD's parse-error offsets for the cases the rig's broker logged."""
    ok, out = True, []
    for (tag, first), want in BENCH.items():
        got = sorted({strict_json(p["raw"])[1] for p in old[2] if p["tag"] == tag and f",{first}" in "," + p["args"]})
        hit = len(got) == 1 and f"(char {want})" in got[0]
        ok &= hit
        out.append(f"  OLD {tag} ({first}): {' | '.join(got)} (rig broker: char {want})")
    return ok, out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build the code under test from this git revision (OLD)")
    args = ap.parse_args()
    root = GEN / "ha_discovery_json"
    checks = []

    export(None, root / "fix")
    fix = build_and_run("FIX (working tree)", root / "fix")
    fix_fails, fix_topics, fix_adv = evaluate("FIX (working tree)", fix)
    tags = {c["tag"] for c in fix[1]}
    missing = [e for e in entry_points(None) if e not in tags]
    print(f"  entry points: {', '.join(entry_points(None))}")
    checks.append((f"FIX calls every public composer (missing: {', '.join(missing) or 'none'})", not missing))
    checks.append(("FIX: every call returns and publishes as expected, every payload passes the gate", not fix_fails))

    if args.old_rev:
        export(args.old_rev, root / "old")
        old = build_and_run(f"OLD (git {args.old_rev})", root / "old")
        old_fails, old_topics, _ = evaluate(f"OLD (git {args.old_rev})", old)
        direct = old_fails - {DISPATCH}
        checks.append((f"OLD fails exactly {', '.join(sorted(KNOWN))} among the direct calls "
                       f"(got {', '.join(sorted(direct)) or 'none'})", direct == set(KNOWN)))
        every = all(all(r[0].startswith("invalid JSON") for r in [gate(p, old[0])])
                    for p in old[2] if p["tag"] in KNOWN)
        checks.append(("OLD: every payload of those two composers is invalid JSON", every))
        checks.append((f"OLD dispatcher fails exactly the two topics (got {sorted(old_topics)})",
                       old_topics == known_topics(old[0])))
        ok, text = diff_old_fix(old, fix)
        print(f"== OLD vs FIX: {text} ==")
        checks.append(("FIX changes nothing but the two payloads, and keeps their command topic", ok))
        ok, lines = bench_offsets(old)
        print("== OLD parse errors against the rig's broker ==")
        print("\n".join(lines))
        checks.append(("OLD fails where the rig's broker failed (select char 291, button char 153)", ok))

    export(None, root / "m1")
    mutate(root / "m1", "MQTTHaDiscovery.cpp", M1)
    m1 = build_and_run("M1 (number cmd_t key renamed)", root / "m1")
    m1_fails, m1_topics, _ = evaluate("M1: the number entity's cmd_t key renamed in a FIX copy", m1)
    m1_json = all(strict_json(p["raw"])[1] is None for p in m1[2] if p["tag"] not in REMOVERS)
    checks.append((f"M1: the gate flags exactly streamNumberDiscovery, all payloads still valid JSON "
                   f"(got {', '.join(sorted(m1_fails)) or 'none'})",
                   m1_fails == {"streamNumberDiscovery", DISPATCH} and m1_json and
                   m1_topics == {f"{m1[0]['prefix']}/number/{m1[0]['node']}/Toutside_override/config"}))

    print("== advisory findings in FIX (not part of the verdict) ==")
    print(f"  {len(fix_adv)} kinds" if fix_adv else "  none")
    print("== verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
