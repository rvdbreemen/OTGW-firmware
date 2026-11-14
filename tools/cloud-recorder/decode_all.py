#!/usr/bin/env python3
"""Decodes every stream of one gateway from the OTGW collection point (TASK-1938).

    python3 decode_all.py http://<nas>:8090 <superuser-email> <password> <device-email> <out-dir> \
        --image firmware.bin [--tz Europe/Amsterdam]

Reads the device's "mix" records (fmt 4) in seq order and writes:
  otframes.log  OT frames in the ADR-209 replay layout (host_time, device_time, source, frame)
  sat.log       one SATTICK line per tick: ISO time, tab, the exact SATTICK text
  debug.log     the telnet bytes the gateway printed, rebuilt from the address tokens; the format strings
                and function names are read from --image, the firmware.bin (or .elf) of the build that recorded
Missing records become "#" lines in otframes.log and sat.log and a "### gap" line in debug.log.
Run it on the LAN: the superuser API is not exposed through the tunnel.
"""
import argparse, base64, datetime, os, sys, zlib
from zoneinfo import ZoneInfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import otrec  # noqa: E402
import esp_image  # noqa: E402
import export_batches as eb  # noqa: E402


def decode_records(records, image, tz, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    counts = {"records": 0, "frames": 0, "ticks": 0, "debug_bytes": 0, "gaps": 0}
    with open(os.path.join(out_dir, "otframes.log"), "w", newline="\n") as fo, \
            open(os.path.join(out_dir, "sat.log"), "w", newline="\n") as fs, \
            open(os.path.join(out_dir, "debug.log"), "wb") as fd:
        fo.write(eb.HEADER + "\n")
        prev = None
        for r in records:
            if prev is not None and r["seq"] != prev + 1:
                note = f"gap: records {prev + 1}..{r['seq'] - 1} missing"
                fo.write(f"# {note}\n")
                fs.write(f"# {note}\n")
                fd.write(f"\r\n### {note}\r\n".encode())
                counts["gaps"] += 1
            prev = r["seq"]
            if r["fmt"] != otrec.FMT_MIX_V1:
                continue
            counts["records"] += 1
            sec = otrec.unpack_mix(r["t0"], base64.b64decode(r["payload"]))
            for t, letter, fr in sec.get(otrec.SEC_OT, []):
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                fo.write(f"{ts.isoformat(timespec='milliseconds')}\t{ts.strftime('%H:%M:%S.%f')}\t{letter}\t{fr:08X}\n")
                counts["frames"] += 1
            for t, _fi, text in otrec.sat_decode(r["t0"], sec.get(otrec.SEC_SAT, b"")):
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                fs.write(f"{ts.isoformat(timespec='milliseconds')}\t{text.rstrip(chr(13) + chr(10))}\n")
                counts["ticks"] += 1
            dbg = sec.get(otrec.SEC_DBG_Z)
            dbg = zlib.decompress(dbg) if dbg is not None else sec.get(otrec.SEC_DBG, b"")
            text = otrec.dbg_decode(r["t0"], dbg, image, tz)
            fd.write(text)
            counts["debug_bytes"] += len(text)
    return counts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base")
    ap.add_argument("admin_email")
    ap.add_argument("admin_password")
    ap.add_argument("device_email")
    ap.add_argument("out_dir")
    ap.add_argument("--image", required=True)
    ap.add_argument("--tz", default="Europe/Amsterdam")
    a = ap.parse_args()
    base = a.base.rstrip("/")
    token = eb.api(base, "/api/collections/_superusers/auth-with-password", None,
                   {"identity": a.admin_email, "password": a.admin_password})["token"]
    q = eb.urllib.parse.urlencode({"filter": f'email="{a.device_email}"', "fields": "id"})
    found = eb.api(base, f"/api/collections/devices/records?{q}", token)["items"]
    if not found:
        sys.exit(f"device {a.device_email} not found")
    records = eb.fetch_batches(base, token, found[0]["id"], "mix")
    c = decode_records(records, esp_image.EspImage(a.image), ZoneInfo(a.tz), a.out_dir)
    print(f"{c['records']} records, {c['frames']} OT frames, {c['ticks']} SAT ticks, {c['debug_bytes']} debug bytes, "
          f"{c['gaps']} gaps -> {a.out_dir}")


if __name__ == "__main__":
    main()
