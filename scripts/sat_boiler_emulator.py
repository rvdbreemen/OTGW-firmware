"""
sat_boiler_emulator.py - host-side synthetic boiler emulator for OTGW32 bench testing
=======================================================================================

PURPOSE
-------
Provides a minimal synthetic boiler so that the TASK-795 simulation availability gate
(satOnBoilerDetected / §4.2 of the SAT simulation contract) can be exercised on a bench
without a physical boiler wired to the OT bus.

CONNECTIVITY
------------
Connects to the OTGW32 OTDirect TCP bridge on port 25238 (default) and periodically
sends synthetic OpenTherm slave-response frames in the OTGW monitor-stream format:

    B<8 hex digits>CRLF

where the 'B' prefix denotes a boiler (slave → master) response frame, matching the
OTDirect bridge output format defined in OTDirect.ino:715:
    snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame)
followed by otDirectBridgeWriteLine() which appends CR LF.

FRAMES EMITTED
--------------
1. MsgID 3 - Slave Configuration / MemberID  (mandatory READ-ACK per OT spec §5.3.2)
   This is the key frame: otDirectBoilerPresent() in OTDirect.ino:292-295 returns
   otBoilerCacheValid[3], which is set in handleMasterResponse() when a real boiler
   ACKs MsgID 3.  The emulator sends this frame so the firmware may populate that
   cache entry and trip satOnBoilerDetected() (the §4.2 edge hook in TASK-795).

2. MsgID 0 - Status (slave status byte, all-zero = healthy boiler, no active demand)
   Sent as READ-ACK to satisfy the periodic status poll (OT spec §5.3.1: mandatory,
   must respond within the 800 ms window).

FRAME FORMAT (OT spec §4.2, verified in OpenTherm.cpp:505-516)
--------------------------------------------------------------
Bit layout of the 32-bit frame (MSB first):

  31      30-28    27-24    23-16    15-8       7-0
  ------  -------  ------   ------   ---------  ---------
  Parity  MsgType  Spare    Data-ID  Data HB    Data LB
          (3 bits) (4 bits) (8 bits) (8 bits)   (8 bits)

  MsgType READ-ACK = 0b100 = 4  (slave confirms a read request from master)

MsgID 3 frame construction (slave config, --member-id M, --slave-config F=0x00):
  data-value = (F << 8) | M
  raw_frame  = (READ_ACK << 28) | (3 << 16) | data_value
             = (4 << 28) | (3 << 16) | (F << 8) | M

  For --member-id 4, --slave-config 0x00:
    raw_frame = 0x40030004
    popcount(0x40030004) = 4 (bits 30, 17, 16, 2) = even → parity bit = 0
    final_frame = 0x40030004  →  wire line "B40030004"

MsgID 0 frame construction (status all-zero):
  data-value = 0x0000
  raw_frame  = 0x40000000
  popcount   = 1 (bit 30) → odd → parity bit = 1
  final_frame = 0xC0000000  →  wire line "BC0000000"

IMPORTANT NOTE - INPUT VS OUTPUT ASYMMETRY ON PORT 25238
---------------------------------------------------------
The OTGW32 port 25238 bridge is BIDIRECTIONAL but ASYMMETRIC:

  OUTPUT (firmware → client): the firmware emits monitor-stream lines such as
    B40030004\\r\\n  (boiler response frame with 'B' prefix)
    T00000000\\r\\n  (thermostat request frame with 'T' prefix)
    CS: 50.00\\r\\n  (PIC command echo)

  INPUT (client → firmware): the firmware expects PIC-style commands of the form
    XX=value\\r\\n  (e.g. CS=50.0, GW=1, SR=3:0004)

  A raw "B40030004" line sent as INPUT has buf[2]=='0', not '=', so
  handleOTDirectBridgeStream() → handleOTDirectCommand() will reject it as an
  invalid command.  It will NOT populate otBoilerCacheValid[3] via the TCP path.

  The correct trigger for satOnBoilerDetected() (§4.2 availability gate) is a real
  OT-bus hardware response from a boiler slave, received by the firmware's OT master
  hardware, processed in handleMasterResponse() (OTDirect.ino:1244+).  AC#2/#3 of
  TASK-795 (actual edge-trip on an OTGW32 bench) must be validated by the maintainer
  with actual OT bus hardware.

  This script emits correctly-formed monitor-stream frames and maintains the TCP
  connection so the bench operator can:
    (a) observe what the firmware OUTPUT stream looks like when a real boiler is present,
    (b) use the frames as a reference for an OT-bus slave hardware device,
    (c) drive the TCP INPUT path to test the firmware's command parsing and response
        logic without a physical boiler in loop (e.g. SR= response-override injection).

USAGE
-----
  python scripts/sat_boiler_emulator.py --dry-run [--member-id 4] [--slave-config 0x01]
  python scripts/sat_boiler_emulator.py --host 192.168.1.x [--port 25238]

PowerShell launch:
  python scripts\\sat_boiler_emulator.py --dry-run
  python scripts\\sat_boiler_emulator.py --host 192.168.1.42 --interval 2.0

Dependencies: Python 3 standard library only (socket, argparse, time, sys).
"""

import argparse
import socket
import sys
import time


# ---------------------------------------------------------------------------
# OpenTherm frame constants (OT spec §4.2, verified in OpenTherm.cpp:505-516)
# ---------------------------------------------------------------------------

#: Slave→master READ-ACK message type = 0b100 = 4 (placed in bits 30-28)
OT_READ_ACK: int = 4

#: MsgID 3: Slave Configuration / MemberID (OT spec §5.3.2)
OT_MSGID_SLAVE_CONFIG: int = 3

#: MsgID 0: Status (OT spec §5.3.1)
OT_MSGID_STATUS: int = 0


# ---------------------------------------------------------------------------
# Frame construction
# ---------------------------------------------------------------------------

def _popcount(n: int) -> int:
    """Return the number of set bits in a 32-bit unsigned integer."""
    n &= 0xFFFFFFFF
    count = 0
    while n:
        count += n & 1
        n >>= 1
    return count


def _set_ot_parity(frame: int) -> int:
    """
    Set or clear bit 31 so that the total count of set bits in the 32-bit frame is even.

    OT spec §4.2.1: the parity bit is chosen so the total number of '1' bits in
    the entire 32-bit frame is even (even parity).

    OpenTherm.cpp:464-476 parity() returns True when the count is odd - meaning
    buildResponse() sets bit 31 when parity() is True (to restore even parity).
    """
    frame &= 0x7FFFFFFF  # clear current parity bit before counting
    if _popcount(frame) % 2 == 1:
        frame |= 0x80000000  # odd data bits → set parity to make total even
    return frame


def build_ot_response(msg_type: int, msg_id: int, data_value: int) -> int:
    """
    Build a 32-bit OpenTherm response frame with correct even parity.

    Layout (OT spec §4.2 / OpenTherm.cpp:505-516):
      bit 31     : parity (even over all 32 bits)
      bits 30-28 : message type (3 bits, MSB first)
      bits 27-24 : spare (4 bits, must be 0)
      bits 23-16 : data-id (8 bits)
      bits 15-8  : data-value high byte (HB)
      bits 7-0   : data-value low byte  (LB)

    Args:
        msg_type:   Message type code (e.g. OT_READ_ACK=4).
        msg_id:     Data item identifier 0-127.
        data_value: 16-bit payload (HB<<8 | LB).

    Returns:
        32-bit frame with parity bit set correctly.
    """
    frame  = (data_value & 0xFFFF)
    frame |= ((msg_id   & 0xFF) << 16)
    frame |= ((msg_type & 0x07) << 28)
    return _set_ot_parity(frame)


def build_msgid3_frame(member_id: int, slave_config_flags: int = 0x00) -> int:
    """
    Build a MsgID 3 (Slave Configuration) READ-ACK frame.

    OT spec §5.3.2 (lines 1603-1623 of the v4.2 spec):
      HB = slave configuration flags (flag8):
           bit 0: DHW present              [0=no DHW,     1=DHW present]
           bit 1: control type             [0=modulating, 1=on/off]
           bit 2: cooling config           [0=no cooling, 1=cooling supported]
           bit 3: DHW config               [0=instantaneous, 1=storage tank]
           bit 4: master low-off & pump    [0=allowed,    1=not allowed]
           bit 5: CH2 present              [0=no CH2,     1=CH2 present]
           bit 6: remote water filling     [0=available,  1=not available]
           bit 7: heat/cool mode control   [0=by master,  1=by slave]
      LB = slave MemberID code (0-255; 0 = customer non-specific per spec note 2)

    Frame construction for member_id=4, flags=0x00:
        data_value = (0x00 << 8) | 0x04 = 0x0004
        raw        = (4 << 28) | (3 << 16) | 0x0004 = 0x40030004
        popcount(0x40030004) = 4 (bits 30, 17, 16, 2) → even → parity bit = 0
        final      = 0x40030004   →   wire line "B40030004"

    Args:
        member_id:          Slave MemberID code (0-255).
        slave_config_flags: HB capability flags (default 0x00 = no capabilities).

    Returns:
        32-bit READ-ACK frame.
    """
    data_value = ((slave_config_flags & 0xFF) << 8) | (member_id & 0xFF)
    return build_ot_response(OT_READ_ACK, OT_MSGID_SLAVE_CONFIG, data_value)


def build_status_frame(slave_status: int = 0x00) -> int:
    """
    Build a MsgID 0 (Status) READ-ACK frame.

    OT spec §5.3.1: master sends READ-DATA with master status in HB; slave
    responds READ-ACK with slave status in LB (HB mirrors master status).
    This emulator sends HB=0x00 (mirror of master status byte) and the provided
    slave_status in LB.

    slave_status LB bits:
      bit 0: fault indication  (0=no fault)
      bit 1: CH mode active
      bit 2: DHW mode active
      bit 3: flame on          (0=flame off)
      bit 4: cooling active
      bit 5: CH2 mode active
      bit 6: diagnostic indication

    Default 0x00 = healthy idle boiler (no fault, no active mode, flame off).

    Frame for slave_status=0x00:
        data_value = 0x0000
        raw        = (4 << 28) | (0 << 16) | 0x0000 = 0x40000000
        popcount   = 1 (bit 30) → odd → parity bit = 1
        final      = 0xC0000000   →   wire line "BC0000000"

    Returns:
        32-bit READ-ACK frame.
    """
    data_value = slave_status & 0xFFFF
    return build_ot_response(OT_READ_ACK, OT_MSGID_STATUS, data_value)


def frame_to_bridge_line(frame: int, prefix: str = 'B') -> bytes:
    """
    Format a 32-bit OT frame as an OTGW bridge monitor-stream line.

    Matches the format emitted by OTDirect.ino bridgeFrameToParser() + otDirectBridgeWriteLine():
        snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame)  → "B40030004"
        OTGWstream.write('\\r'); OTGWstream.write('\\n')              → CR LF

    Args:
        frame:  32-bit OT frame (only low 32 bits used).
        prefix: 'B' = boiler response (slave→master), 'T' = thermostat, 'R' = gateway
                request, 'A' = gateway answer.

    Returns:
        ASCII bytes ready to write to the TCP socket.
    """
    line = f"{prefix}{frame & 0xFFFFFFFF:08X}\r\n"
    return line.encode('ascii')


def decode_frame(frame: int) -> str:
    """Return a concise human-readable decode of a 32-bit OT frame."""
    msg_type_names = {
        0: 'READ-DATA',    1: 'WRITE-DATA', 2: 'INVALID-DATA',
        4: 'READ-ACK',     5: 'WRITE-ACK',  6: 'DATA-INVALID',
        7: 'UNKNOWN-DATAID',
    }
    parity_bit = (frame >> 31) & 1
    msg_type   = (frame >> 28) & 0x7
    spare      = (frame >> 24) & 0xF
    msg_id     = (frame >> 16) & 0xFF
    hb         = (frame >>  8) & 0xFF
    lb         =  frame        & 0xFF

    type_name  = msg_type_names.get(msg_type, f'TYPE-{msg_type}')
    total_bits = _popcount(frame)
    parity_ok  = (total_bits % 2 == 0)

    return (
        f"0x{frame:08X}  parity={'OK' if parity_ok else 'BAD'}"
        f"(bit31={parity_bit},popcount={total_bits})"
        f"  {type_name}(0b{msg_type:03b})"
        f"  spare=0b{spare:04b}"
        f"  id={msg_id}"
        f"  HB=0x{hb:02X}  LB=0x{lb:02X}"
    )


# ---------------------------------------------------------------------------
# Operational modes
# ---------------------------------------------------------------------------

def run_dry_run(args: argparse.Namespace) -> None:
    """Print the frames that would be sent and exit without opening a socket."""
    msgid3_frame = build_msgid3_frame(args.member_id, args.slave_config)
    status_frame = build_status_frame()
    msgid3_line  = frame_to_bridge_line(msgid3_frame).decode('ascii').rstrip()
    status_line  = frame_to_bridge_line(status_frame).decode('ascii').rstrip()

    print("=== sat_boiler_emulator.py - dry-run mode ===")
    print()
    print("Configuration:")
    print(f"  --host        : (not connected in dry-run)")
    print(f"  --port        : {args.port}")
    print(f"  --member-id   : {args.member_id}")
    print(f"  --slave-config: 0x{args.slave_config:02X}")
    print(f"  --interval    : {args.interval}s")
    print()
    print("MsgID 3 (Slave Configuration / MemberID) READ-ACK frame:")
    print(f"  Wire line  : {msgid3_line}")
    print(f"  Decode     : {decode_frame(msgid3_frame)}")
    print(f"  Slave config flags (HB): 0x{args.slave_config:02X}")
    print(f"  Slave MemberID     (LB): {args.member_id} (0x{args.member_id:02X})")
    print()
    print("MsgID 0 (Status) READ-ACK frame (slave_status=0x00, idle boiler):")
    print(f"  Wire line  : {status_line}")
    print(f"  Decode     : {decode_frame(status_frame)}")
    print()
    print("NOTE: port 25238 input/output asymmetry - see module docstring.")
    print("  AC#2/#3 (actual §4.2 edge-trip on OTGW32) require bench validation")
    print("  with physical OT bus hardware. See scripts/README-sat-boiler-emulator.md.")


def run_connected(args: argparse.Namespace) -> None:
    """Connect to the OTGW OTDirect bridge and send synthetic frames periodically."""
    msgid3_frame = build_msgid3_frame(args.member_id, args.slave_config)
    status_frame = build_status_frame()
    msgid3_line  = frame_to_bridge_line(msgid3_frame)
    status_line  = frame_to_bridge_line(status_frame)

    print(f"Connecting to {args.host}:{args.port} ...")
    print(f"MsgID 3 frame : {msgid3_line.decode('ascii').rstrip()}")
    print(f"  {decode_frame(msgid3_frame)}")
    print(f"MsgID 0 frame : {status_line.decode('ascii').rstrip()}")
    print(f"  {decode_frame(status_frame)}")
    print(f"Interval      : {args.interval}s  |  Ctrl-C to stop")
    print()

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((args.host, args.port))
        sock.settimeout(None)
    except (socket.error, OSError) as exc:
        print(f"ERROR: cannot connect to {args.host}:{args.port}: {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"Connected to {args.host}:{args.port}")

    iteration = 0
    try:
        while True:
            iteration += 1
            ts = time.strftime("%H:%M:%S")

            for frame_bytes, label in (
                (msgid3_line, "MsgID 3"),
                (status_line, "MsgID 0"),
            ):
                try:
                    sock.sendall(frame_bytes)
                    print(f"[{ts}] #{iteration:4d} TX {frame_bytes.decode('ascii').rstrip()}  ({label})")
                except (socket.error, OSError) as exc:
                    print(f"[{ts}] ERROR sending {label}: {exc}", file=sys.stderr)
                    return

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nCtrl-C - stopping.")
    finally:
        try:
            sock.close()
        except Exception:
            pass
        print(f"Socket closed. Sent {iteration} frame burst(s).")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Synthetic boiler emulator for OTGW32 bench testing (TASK-802). "
            "Connects to the OTDirect TCP bridge on port 25238 and periodically "
            "emits OpenTherm MsgID 3 (Slave Configuration) and MsgID 0 (Status) "
            "READ-ACK frames in OTGW monitor-stream format. "
            "Use --dry-run to print computed frames without opening a socket."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        '--host',
        default=None,
        help="OTGW32 hostname or IP address (required unless --dry-run).",
    )
    parser.add_argument(
        '--port',
        type=int,
        default=25238,
        help="TCP port of the OTDirect bridge.",
    )
    parser.add_argument(
        '--member-id',
        type=int,
        default=4,
        dest='member_id',
        metavar='N',
        help="Slave MemberID code (0-255; 0=customer non-specific, per OT spec note 2).",
    )
    parser.add_argument(
        '--slave-config',
        type=lambda x: int(x, 0),   # accept 0x01, 0b00000001, or plain decimal
        default=0x00,
        dest='slave_config',
        metavar='FLAGS',
        help=(
            "Slave configuration flags byte (HB of MsgID 3 data-value, 0-255). "
            "Accepts decimal, hex (0x..), or binary (0b..) notation. "
            "Example: 0x01 sets DHW-present bit."
        ),
    )
    parser.add_argument(
        '--interval',
        type=float,
        default=1.0,
        metavar='SEC',
        help="Seconds between frame bursts.",
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Print computed frames and exit without opening a socket.",
    )

    args = parser.parse_args()

    # Validate member-id range
    if not 0 <= args.member_id <= 255:
        parser.error("--member-id must be in range 0-255")

    if args.dry_run:
        run_dry_run(args)
        return

    if args.host is None:
        parser.error("--host is required unless --dry-run is specified")

    run_connected(args)


if __name__ == '__main__':
    main()
