#!/usr/bin/env python3
"""Decodes every stream of one gateway from the OTGW collection point (TASK-1938).

    python3 decode_all.py --file otgw-recording.bin <out-dir> --image firmware.bin     (a local download)
    python3 decode_all.py http://<nas>:8090 <superuser-email> <password> <device-uid> <out-dir> \
        --image firmware.bin [--image older-firmware.bin] [--tz Europe/Amsterdam]
    (give - as the password to be asked for it instead, so it stays out of the shell history)
    python3 decode_all.py http://<nas>:8090 <device-uid> <out-dir> --token-file <file> --image firmware.bin
    (a superuser token, e.g. a time-limited PocketBase impersonate token, instead of e-mail and password)

Reads the device's "mix" records (fmt 4) in seq order and writes:
  otframes.log  OT frames in the ADR-209 replay layout (host_time, device_time, source, frame)
  sat.log       one SATTICK line per tick: ISO time, tab, the exact SATTICK text
  events.log    one line per event (TASK-1473): ISO time, tab, type, tab, the fields as JSON
  debug.log     the telnet bytes the gateway printed, rebuilt from the address tokens; the format strings
                and function names are read from the firmware.bin matching each batch's build ID.
                Use --legacy-image if the history contains both tagged and untagged batches.
Missing records become "#" lines in otframes.log and sat.log and a "### gap" line in debug.log; so does what the
gateway itself reports as lost (SEC_LOST: items or records it could not record).
Run it on the LAN: the superuser API is not exposed through the tunnel.
"""
import argparse, json, base64, datetime, os, sys, zlib
from zoneinfo import ZoneInfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import otrec  # noqa: E402
import esp_image  # noqa: E402
import export_batches as eb  # noqa: E402


def decode_records(records, image, tz, out_dir, *, images_by_build=None):
    os.makedirs(out_dir, exist_ok=True)
    counts = {"records": 0, "frames": 0, "ticks": 0, "debug_bytes": 0, "gaps": 0}
    with open(os.path.join(out_dir, "otframes.log"), "w", newline="\n") as fo, \
            open(os.path.join(out_dir, "sat.log"), "w", newline="\n") as fs, \
            open(os.path.join(out_dir, "debug.log"), "wb") as fd, \
            open(os.path.join(out_dir, "events.log"), "w", newline="\n") as fe:
        fo.write(eb.HEADER + "\n")
        prev = None
        next_fseq = None                              # TASK-2901: the frame sequence number the next record should start with
        for r in records:
            if prev is not None and r["seq"] != prev + 1:
                note = f"gap: records {prev + 1}..{r['seq'] - 1} missing"
                fo.write(f"# {note}\n")
                fs.write(f"# {note}\n")
                fd.write(f"\r\n### {note}\r\n".encode())
                counts["gaps"] += 1
            prev = r["seq"]
            if r["fmt"] not in (otrec.FMT_MIX_V1, otrec.FMT_MIX_LZ):
                continue
            counts["records"] += 1
            sec = otrec.unpack_mix(r["t0"], otrec.record_payload(r["fmt"], base64.b64decode(r["payload"])))
            items, recs = otrec.lost_counts(sec)
            if items or recs:                         # the gateway reports what it could not record
                note = f"lost on the gateway before record {r['seq']}: {items} items, {recs} records"
                fo.write(f"# {note}\n")
                fs.write(f"# {note}\n")
                fd.write(f"\r\n### {note}\r\n".encode())
                counts["lost_items"] = counts.get("lost_items", 0) + items
                counts["lost_records"] = counts.get("lost_records", 0) + recs
            events = list(otrec.evt_decode(r["t0"], sec.get(otrec.SEC_EVT, b"")))
            for t, typ, fields in events:            # TASK-2901: session and date markers also in the OT log
                if typ == 44:
                    ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                    fo.write(f"# capture paused {ts.isoformat(timespec='milliseconds')}: {fields['reason']}\n")
                if typ in (42, 43):
                    ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                    what = (f"session start {datetime.datetime.fromtimestamp(fields['epoch_ms'] / 1000, tz).isoformat(timespec='milliseconds')}"
                            f" firmware {fields['firmware']} uid {fields['uid']} next frame seq {fields['next_frame_seq']}"
                            if typ == 42 else f"date {fields['date']}")
                    fo.write(f"# {what}\n")
                    if typ == 42:
                        next_fseq = fields["next_frame_seq"]   # a new session: numbering starts here
            frames = sec.get(otrec.SEC_OT, [])
            fseq = otrec.fseq_decode(sec.get(otrec.SEC_FSEQ))
            if fseq and frames:                       # TASK-2901: missing frames from the sequence numbers
                first, last = fseq
                if next_fseq is not None and first != next_fseq:
                    n = first - next_fseq
                    fo.write(f"# frame gap: {n} frames (seq {next_fseq}..{first - 1}) missing before record {r['seq']}\n"
                             if n > 0 else f"# frame seq restart: {next_fseq} expected, {first} in record {r['seq']}\n")
                    counts["frame_gaps"] = counts.get("frame_gaps", 0) + 1
                    counts["frames_missing"] = counts.get("frames_missing", 0) + max(n, 0)
                inside = last - first + 1 - len(frames)
                if inside:
                    fo.write(f"# frame gap: {inside} frames missing inside record {r['seq']} (seq {first}..{last})\n")
                    counts["frame_gaps"] = counts.get("frame_gaps", 0) + 1
                    counts["frames_missing"] = counts.get("frames_missing", 0) + max(inside, 0)
                next_fseq = last + 1
            for t, letter, fr in frames:
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                fo.write(f"{ts.isoformat(timespec='milliseconds')}\t{ts.strftime('%H:%M:%S.%f')}\t{letter}\t{fr:08X}\n")
                counts["frames"] += 1
            for t, _fi, text in otrec.sat_decode(r["t0"], sec.get(otrec.SEC_SAT, b"")):
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                fs.write(f"{ts.isoformat(timespec='milliseconds')}\t{text.rstrip(chr(13) + chr(10))}\n")
                counts["ticks"] += 1
            dbg = sec.get(otrec.SEC_DBG_Z)
            dbg = zlib.decompress(dbg) if dbg is not None else sec.get(otrec.SEC_DBG, b"")
            build_id = r.get("build_id")
            batch_image = images_by_build[build_id] if images_by_build is not None and build_id else image
            try:
                text = otrec.dbg_decode(r["t0"], dbg, batch_image, tz)
            except (OSError, OverflowError, ValueError, IndexError) as e:
                # TASK-2901: one record's debug section that does not decode must not stop the export of the rest
                # (the OT frames, SAT ticks and events of every record are decoded independently of it).
                text = f"\r\n### debug decode error in record seq {r.get('seq')}: {type(e).__name__}: {e}\r\n".encode()
                counts["debug_errors"] = counts.get("debug_errors", 0) + 1
            for t, typ, fields in events:
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                fe.write(f"{ts.isoformat(timespec='milliseconds')}\t{otrec.EV_NAMES[typ]}\t"
                         + json.dumps(fields, ensure_ascii=False, separators=(",", ":")) + "\n")
                counts["events"] = counts.get("events", 0) + 1
            fd.write(text)
            counts["debug_bytes"] += len(text)
    return counts


def write_days(out_dir):
    """TASK-2901: days/<YYYY-MM-DD>.otframes.log, one file per local calendar day of otframes.log, 00:00:00-24:00:00.
    The device_time column is already the local time of day; a '#' note goes to the day of the line after it."""
    src = os.path.join(out_dir, "otframes.log")
    days = os.path.join(out_dir, "days")
    os.makedirs(days, exist_ok=True)
    files, pending, header, n = {}, [], None, 0
    with open(src, encoding="utf-8") as f:
        for line in f:
            if header is None:
                header = line
                continue
            if line.startswith("#"):
                pending.append(line)
                continue
            day = line[:10]
            if day not in files:
                files[day] = open(os.path.join(days, f"{day}.otframes.log"), "w", encoding="utf-8", newline="\n")
                files[day].write(header)
            files[day].writelines(pending)
            pending = []
            files[day].write(line)
            n += 1
    for fh in files.values():
        fh.close()
    return sorted(files), n


def read_mix_file(path):
    """The records of a local download (TASK-2907, "OTGWMIX1", RecRing.h) in the form fetch_batches returns.
    Returns (records, header) with header = {"version", "firmware", "githash", "uid"}."""
    import struct
    data = open(path, "rb").read()
    if data[:8] != b"OTGWMIX1":
        raise ValueError(f"{path}: not an OTGWMIX1 recording")
    version, hlen = struct.unpack_from("<HH", data, 8)
    texts, i = [], 12
    for _ in range(3):
        n = data[i]
        texts.append(data[i + 1:i + 1 + n].decode(errors="replace"))
        i += 1 + n
    records, i = [], hlen
    while i + 20 <= len(data):
        if data[i] != 0xA5:
            raise ValueError(f"{path}: bad entry at byte {i}")
        fmt, ln, seq, t0 = struct.unpack_from("<BHQq", data, i + 1)
        if i + 20 + ln > len(data):
            break                                          # the download ended inside this record
        records.append({"seq": seq, "t0": t0, "fmt": fmt,
                        "payload": base64.b64encode(data[i + 20:i + 20 + ln]).decode()})
        i += 20 + ln
    return records, {"version": version, "firmware": texts[0], "githash": texts[1], "uid": texts[2]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base", nargs="?", help="collection point URL (not with --file)")
    ap.add_argument("admin_email", nargs="?")
    ap.add_argument("admin_password", nargs="?")
    ap.add_argument("device_uid", nargs="?", help="otgw-<12 hex>, the uid the gateway enrolled with")
    ap.add_argument("out_dir", nargs="?")
    ap.add_argument("--file", help="decode a local download (GET /api/v2/diag/recording) instead; then give only out_dir")
    ap.add_argument("--image", required=True, action="append",
                    help="firmware.bin for a recording build (repeat for histories spanning builds)")
    ap.add_argument("--legacy-image", help="firmware.bin for uploaded batches without a recorded build ID")
    ap.add_argument("--tz", default="Europe/Amsterdam")
    ap.add_argument("--days", action="store_true", help="also write days/<YYYY-MM-DD>.otframes.log, one per local day")
    ap.add_argument("--token-file", help="a superuser auth token (e.g. a PocketBase impersonate token) instead of the "
                                         "e-mail and password; then give only base device_uid out_dir")
    a = ap.parse_args()
    if a.token_file and not a.file:                        # base uid out: shift the positionals into place
        a.device_uid, a.out_dir, a.admin_email, a.admin_password = a.admin_email, a.admin_password, "-", "-"
    if a.file:
        out = a.out_dir or a.base                          # with --file the only positional is the output folder
        if not out:
            sys.exit("--file needs an output folder")
        if len(a.image) != 1 or a.legacy_image:
            ap.error("--file requires exactly one --image")
        records, hdr = read_mix_file(a.file)
        c = decode_records(records, esp_image.EspImage(a.image[0]), ZoneInfo(a.tz), out)
        print(f"{hdr['uid']} {hdr['firmware']}: {c['records']} records, {c['frames']} OT frames, {c['ticks']} SAT "
              f"ticks, {c.get('events', 0)} events, {c['debug_bytes']} debug bytes, {c['gaps']} gaps -> {out}")
        return
    if not (a.base and a.admin_email and a.admin_password and a.device_uid and a.out_dir):
        sys.exit("give base admin_email admin_password device_uid out_dir, or --file <download> out_dir")
    base = a.base.rstrip("/")
    if a.token_file:                                       # read, never printed
        with open(a.token_file, encoding="utf-8-sig") as f:
            token = f.read().strip()
    else:
        if a.admin_password == "-":                        # ask instead: keeps the password out of the shell history
            import getpass
            a.admin_password = getpass.getpass("PocketBase superuser password: ")
        token = eb.api(base, "/api/collections/_superusers/auth-with-password", None,
                       {"identity": a.admin_email, "password": a.admin_password})["token"]
    q = eb.urllib.parse.urlencode({"filter": f'uid="{a.device_uid}"', "fields": "id"})
    found = eb.api(base, f"/api/collections/devices/records?{q}", token)["items"]
    if not found:
        sys.exit(f"device {a.device_uid} not found")
    records = eb.fetch_batches(base, token, found[0]["id"], "mix")
    images = [esp_image.EspImage(path) for path in a.image]
    images_by_build = {image.build_id: image for image in images if image.build_id}
    build_ids = {r.get("build_id") for r in records if r["fmt"] in (otrec.FMT_MIX_V1, otrec.FMT_MIX_LZ)}
    missing = sorted(build_ids.difference(images_by_build, {None, ""}))
    if missing:
        ap.error("supply a matching --image for each missing build: " + ", ".join(missing))
    legacy = None
    if build_ids.intersection({None, ""}):
        if a.legacy_image:
            legacy = esp_image.EspImage(a.legacy_image)
        elif len(images) == 1 and not build_ids.intersection(images_by_build):
            # Preserve single-image export for histories entirely predating build IDs.
            legacy = images[0]
        else:
            ap.error("supply --legacy-image for batches without a recorded build ID")
    c = decode_records(records, legacy, ZoneInfo(a.tz), a.out_dir, images_by_build=images_by_build)
    print(f"{c['records']} records, {c['frames']} OT frames, {c['ticks']} SAT ticks, {c['debug_bytes']} debug bytes, "
          f"{c['gaps']} gaps, {c.get('debug_errors', 0)} debug decode errors, "
          f"lost {c.get('lost_items', 0)} items / {c.get('lost_records', 0)} records, "
          f"{c.get('frame_gaps', 0)} frame gaps ({c.get('frames_missing', 0)} frames missing by sequence number) -> {a.out_dir}")
    if a.days:
        names, n = write_days(a.out_dir)
        print(f"days: {', '.join(names)} ({n} frames) -> {os.path.join(a.out_dir, 'days')}")


if __name__ == "__main__":
    main()
