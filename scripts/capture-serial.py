#!/usr/bin/env python3
"""Capture the ESP32-S3 USB-serial console (boot ROM + IDF logs + panic backtrace).

The OTGW32 exposes a native USB Serial/JTAG console (VID 303A). Baud is irrelevant
on native USB-CDC, but it is honoured for plain UART bridges. Output is mirrored to
stdout and to a timestamped file so a boot/panic can be analysed after the fact.

Usage:
    python scripts/capture-serial.py [--port COM4] [--baud 115200] [--seconds 90]

Power-cycle (or reset) the device WHILE this is running to capture the full boot.
"""
import argparse
import os
import sys
import time
from datetime import datetime

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not installed: python -m pip install pyserial")


def _mac_from_port(port):
    """Derive the device uniqueid from the USB serial number / hwid MAC."""
    try:
        from serial.tools import list_ports
        for p in list_ports.comports():
            if p.device and p.device.upper() == port.upper():
                # hwid like 'USB VID:PID=303A:1001 SER=10:20:BA:21:B4:F8'
                sn = (p.serial_number or "")
                hexmac = "".join(c for c in sn if c in "0123456789ABCDEFabcdef")
                if len(hexmac) >= 12:
                    return "otgw-" + hexmac[-12:].upper()
    except Exception:
        pass
    return None


def _secrets_get(key, default=None):
    """Read a value from the shared out-of-repo store via _secrets.py (same dir)."""
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import _secrets
        return _secrets.get(key, default)
    except Exception:
        return default


def _sanitize(s):
    return "".join(c if (c.isalnum() or c in "-_.") else "_" for c in str(s)) or "unknown"


def resolve_identity(args):
    """Resolve {hostname, uniqueid, hardware} for the shared naming convention.
    Priority: explicit CLI arg -> capture-settings.json -> derived/default."""
    host = args.hostname or _secrets_get("device_host") or "OTGW"
    host = str(host).split(".")[0]  # strip .local
    uid = (args.uniqueid or _secrets_get("uniqueid")
           or _mac_from_port(args.port) or "unknown")
    hw = args.hardware or _secrets_get("hardware") or "esp32"
    return {"hostname": _sanitize(host), "uniqueid": _sanitize(uid), "hardware": _sanitize(hw)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM4")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=90.0, help="0 = until Ctrl-C")
    ap.add_argument("--outdir", default="logs/serial")
    ap.add_argument("--reset", action="store_true",
                    help="pulse DTR/RTS to reset the chip into normal boot before capturing")
    ap.add_argument("--hostname", default=None, help="device hostname for the filename")
    ap.add_argument("--hardware", default=None, help="hardware tag for the filename (e.g. otgw32)")
    ap.add_argument("--uniqueid", default=None, help="device uniqueid for the filename (default: otgw-<MAC from USB>)")
    ap.add_argument("--outfile", default=None, help="explicit output path (overrides the naming convention)")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    # Shared naming convention across both capture scripts:
    #   <kind>-<hostname>-<uniqueid>-<hardware>-<datetime>.txt
    ident = resolve_identity(args)
    if args.outfile:
        path = args.outfile
        os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    else:
        name = f"serial-{ident['hostname']}-{ident['uniqueid']}-{ident['hardware']}-{stamp}.txt"
        path = os.path.join(args.outdir, name)

    print(f"[capture-serial] opening {args.port} @ {args.baud} -> {path}")
    print(f"[capture-serial] POWER-CYCLE / RESET the device now to capture boot.")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as e:
        sys.exit(f"[capture-serial] cannot open {args.port}: {e}")

    if args.reset:
        # Reset into a NORMAL boot while our port stays open, so the boot ROM /
        # bootloader / setup() output is captured from the first byte. The ESP32-S3
        # native USB-Serial-JTAG needs esptool's dedicated USBJTAGSerialReset
        # sequence (a plain DTR/RTS pulse does NOT reset it). Fall back to a classic
        # RTS pulse for a real USB-UART bridge.
        print("[capture-serial] resetting into normal boot (esptool USB-JTAG sequence)...")
        try:
            try:
                from esptool.reset import USBJTAGSerialReset
                USBJTAGSerialReset(ser)()
            except Exception:
                from esptool.reset import ClassicReset
                ClassicReset(ser)()
            ser.reset_input_buffer()
        except Exception as e:
            print(f"[capture-serial] esptool reset unavailable ({e}); trying DTR/RTS pulse")
            try:
                ser.setDTR(False); ser.setRTS(True)
                time.sleep(0.12)
                ser.setRTS(False)
                ser.reset_input_buffer()
            except Exception as e2:
                print(f"[capture-serial] reset failed ({e2}); capture continues")

    deadline = time.monotonic() + args.seconds if args.seconds > 0 else None
    line_buf = bytearray()
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# serial capture {args.port} @ {args.baud}  started {stamp}\n")
        f.flush()
        try:
            while deadline is None or time.monotonic() < deadline:
                chunk = ser.read(256)
                if not chunk:
                    continue
                line_buf.extend(chunk)
                while b"\n" in line_buf:
                    idx = line_buf.index(b"\n")
                    raw = bytes(line_buf[: idx + 1])
                    del line_buf[: idx + 1]
                    text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    out = f"{ts}  {text}"
                    print(out)
                    f.write(out + "\n")
                    f.flush()
        except KeyboardInterrupt:
            print("\n[capture-serial] stopped by user")
        finally:
            if line_buf:
                text = bytes(line_buf).decode("utf-8", errors="replace")
                f.write(text + "\n")
            ser.close()
    print(f"[capture-serial] saved {path}")


if __name__ == "__main__":
    main()
