#!/usr/bin/env bash
# =============================================================================
#  capture-otgw.sh - OTGW diagnostic capture for Linux, WSL and macOS
#
#  The counterpart of capture-mqtt-debug.bat. That one embeds a PowerShell
#  worker, which is the right call on Windows and the wrong one here: a stock
#  WSL Ubuntu 24.04 ships neither pwsh nor mosquitto-clients nor a browser, so
#  reusing it would mean asking a reporter to install PowerShell before they can
#  capture a single line.
#
#  This script therefore uses what is already there. The telnet stream comes
#  from bash's own /dev/tcp, the REST snapshot from curl, and MQTT is optional.
#  Nothing here needs installing on a default Debian, Ubuntu, Fedora or macOS.
#
#  WHAT IT WRITES (same names as the Windows capture, so a maintainer reading
#  two reports does not have to relearn the layout):
#    telnet.log         the OpenTherm debug stream, the reason this exists
#    rest-snapshot.log  one pass over the diagnostic REST endpoints
#    crashlog.log       the crash endpoint, polled slowly
#    mqtt.log           broker mirror, only when mosquitto_sub is installed
#    summary.txt        what ran, what did not, and why
#
#  WHAT IT DELIBERATELY DOES NOT DO: drive a headless browser. The Windows
#  script can, and on a memory investigation that turned out to generate 365
#  REST requests per minute and drown the very fault being chased. A field
#  capture wants the device's own behaviour, not the tooling's.
#
#  Usage:
#    ./capture-otgw.sh                                  interactive prompts
#    ./capture-otgw.sh --host 192.168.1.50              non-interactive
#    ./capture-otgw.sh --host otgw.local --minutes 30
#    ./capture-otgw.sh --host 10.0.0.5 --broker 10.0.0.2 --mqtt-user ha
#    ./capture-otgw.sh --help
#
#  Copyright (c) 2021-2026 Robert van den Breemen. MIT License.
# =============================================================================

# Written for bash 3.2, which is what macOS still ships. No associative arrays,
# no mapfile, no ${var^^}. Do not "modernise" these without testing on macOS.
set -u

SCRIPT_NAME="$(basename "$0")"
SCRIPT_VERSION="1.0.0"

# ---------------------------------------------------------------- defaults ---
DEVICE_HOST=""
BROKER_HOST=""
MQTT_PORT="1883"
MQTT_USER=""
MQTT_PASS=""
MQTT_TOPIC="#"
DURATION_MIN="10"
OUT_DIR=""
TELNET_PORT="23"
CRASH_POLL_SEC="3600"   # slow on purpose: catches a reboot without perturbing
ASSUME_YES="0"
RECONNECT="0"           # keep watching across dropouts instead of stopping at the first
PROBE_SEC="10"          # REST reachability probe interval while reconnecting
SERIAL_DEV=""           # capture the USB serial link instead of telnet
SERIAL_AUTO="0"         # --serial given with no device: auto-detect one
# 9600 because that is what the firmware configures: OTGWSerial.cpp:836 does
# HardwareSerial::begin(9600, SERIAL_8N1). That UART is the PIC link, and the
# PIC runs at 9600, so the ESP matches it. Capturing at any other rate yields
# framing garbage that looks like a broken device. The Windows script defaulted
# to 115200 and cost a reporter a round trip on GH #684 for exactly this.
SERIAL_BAUD="9600"

# Recorded for summary.txt so a shared capture can be judged without
# reverse-engineering it. Set as each source is decided.
SRC_TELNET="not attempted"
SRC_REST="not attempted"
SRC_MQTT="not attempted"
SRC_CRASH="not attempted"
EXIT_REASON="unknown"

usage() {
    cat <<EOF
$SCRIPT_NAME $SCRIPT_VERSION - OTGW diagnostic capture (Linux, WSL, macOS)

  --host HOST          gateway address or hostname (prompted when omitted)
  --minutes N          capture duration in minutes (default: $DURATION_MIN)
  --out DIR            output directory (default: ./otgw-capture-<host>-<stamp>)
  --broker HOST        MQTT broker; enables the broker mirror
  --mqtt-port N        broker port (default: $MQTT_PORT)
  --mqtt-user USER     broker username
  --mqtt-pass PASS     broker password (prompted when a user is given without one)
  --mqtt-topic TOPIC   topic filter (default: $MQTT_TOPIC)
  --telnet-port N      debug port (default: $TELNET_PORT)
  --crash-poll N       crash endpoint poll interval in seconds (default: $CRASH_POLL_SEC)
  --reconnect          for intermittent dropouts: keep reconnecting across
                       outages instead of stopping at the first one, and probe
                       reachability every --probe seconds so the log shows the
                       moment it went away and the moment it came back
  --probe N            reachability probe interval with --reconnect (default: ${PROBE_SEC}s)
  --serial [DEV]       capture the USB serial link instead of telnet, for a
                       gateway that will not come up on WiFi. Without DEV it
                       looks for one. --host is then optional.
  --baud N             serial rate (default: $SERIAL_BAUD, which is what the
                       firmware configures; see OTGWSerial.cpp:836)
  --yes                skip the confirmation prompt
  --help               this text

Stop early with Ctrl+C: the capture finishes cleanly and still writes summary.txt.

What you need installed: bash and curl. That is all. mosquitto_sub is used when
present and skipped with a note when it is not, so a missing broker client never
costs you the telnet log.
EOF
}

# ------------------------------------------------------------------ helpers ---
log()  { printf '%s\n' "$*"; }
warn() { printf '%s\n' "$*" >&2; }

# Milliseconds since epoch. GNU date understands %3N, macOS date does not, and
# perl is present on every macOS and on nearly every Linux.
now_ms() {
    local v
    v="$(date +%s%3N 2>/dev/null)"
    case "$v" in
        *N*|"") perl -e 'print int(time()*1000)' 2>/dev/null || echo "$(date +%s)000" ;;
        *)      printf '%s' "$v" ;;
    esac
}

timestamp() { date '+%Y-%m-%d %H:%M:%S'; }
stamp_compact() { date '+%Y%m%d-%H%M%S'; }

# curl with a wget fallback, mirroring what flash_otgw.sh already does in this
# repo so a reporter needs the same tools for both.
http_get() {
    local url="$1" timeout="${2:-10}"
    if command -v curl >/dev/null 2>&1; then
        curl -sS -m "$timeout" "$url" 2>&1
    elif command -v wget >/dev/null 2>&1; then
        wget -qO- --timeout="$timeout" "$url" 2>&1
    else
        echo "(neither curl nor wget is installed)"
        return 1
    fi
}

# ------------------------------------------------------------- argument parse ---
while [ $# -gt 0 ]; do
    case "$1" in
        --host)        DEVICE_HOST="${2:-}"; shift 2 ;;
        --minutes)     DURATION_MIN="${2:-}"; shift 2 ;;
        --out)         OUT_DIR="${2:-}"; shift 2 ;;
        --broker)      BROKER_HOST="${2:-}"; shift 2 ;;
        --mqtt-port)   MQTT_PORT="${2:-}"; shift 2 ;;
        --mqtt-user)   MQTT_USER="${2:-}"; shift 2 ;;
        --mqtt-pass)   MQTT_PASS="${2:-}"; shift 2 ;;
        --mqtt-topic)  MQTT_TOPIC="${2:-}"; shift 2 ;;
        --telnet-port) TELNET_PORT="${2:-}"; shift 2 ;;
        --crash-poll)  CRASH_POLL_SEC="${2:-}"; shift 2 ;;
        --reconnect)   RECONNECT="1"; shift ;;
        --probe)       PROBE_SEC="${2:-}"; shift 2 ;;
        --serial)
            # Optional argument: --serial /dev/ttyUSB0, or bare --serial to
            # auto-detect. A following word starting with - is the next flag.
            case "${2:-}" in
                ""|-*) SERIAL_AUTO="1"; shift ;;
                *)     SERIAL_DEV="$2"; shift 2 ;;
            esac
            ;;
        --baud)        SERIAL_BAUD="${2:-}"; shift 2 ;;
        --yes|-y)      ASSUME_YES="1"; shift ;;
        --help|-h)     usage; exit 0 ;;
        *) warn "Unknown option: $1"; warn "Try --help."; exit 2 ;;
    esac
done

# ------------------------------------------------------------ sanity on bash ---
# /dev/tcp is a bash feature and some distributions build it out. Check once,
# up front, so the failure is a sentence rather than a confusing silence later.
HAVE_DEVTCP="0"
if (exec 3<>/dev/null) 2>/dev/null; then :; fi
if bash -c 'exec 3<>/dev/tcp/127.0.0.1/1' 2>&1 | grep -qi 'not supported\|no such file'; then
    HAVE_DEVTCP="0"
else
    HAVE_DEVTCP="1"
fi

# ------------------------------------------------------------------- prompts ---
# Serial mode is for a gateway that will not come up on WiFi, so an address is
# optional there: without one the network sources simply stay off.
SERIAL_MODE="0"
if [ -n "$SERIAL_DEV" ] || [ "$SERIAL_AUTO" = "1" ]; then SERIAL_MODE="1"; fi

if [ -z "$DEVICE_HOST" ] && [ "$SERIAL_MODE" != "1" ]; then
    printf 'Gateway address (IP or hostname): '
    read -r DEVICE_HOST
fi
if [ -z "$DEVICE_HOST" ] && [ "$SERIAL_MODE" != "1" ]; then
    warn "No gateway address given. Nothing to capture."
    exit 2
fi

if [ -n "$MQTT_USER" ] && [ -z "$MQTT_PASS" ]; then
    printf 'MQTT password for %s (leave empty for none): ' "$MQTT_USER"
    stty -echo 2>/dev/null; read -r MQTT_PASS; stty echo 2>/dev/null; printf '\n'
fi

case "$DURATION_MIN" in
    ''|*[!0-9]*) warn "--minutes takes a whole number, got: $DURATION_MIN"; exit 2 ;;
esac
[ "$DURATION_MIN" -gt 0 ] || { warn "--minutes must be at least 1."; exit 2; }

if [ -z "$OUT_DIR" ]; then
    OUT_DIR="./otgw-capture-$(printf '%s' "$DEVICE_HOST" | tr -c 'A-Za-z0-9._-' '-')-$(stamp_compact)"
fi

# ------------------------------------------------------------------ summary ---
# Written by the EXIT trap so it exists even when the run dies early. This is
# the line that lets someone judge a shared capture without counting requests
# in the log to work out what the tooling was doing.
write_summary() {
    {
        echo "OTGW capture summary"
        echo "===================="
        echo "script        : $SCRIPT_NAME $SCRIPT_VERSION"
        echo "finished      : $(timestamp)"
        echo "host          : $DEVICE_HOST"
        echo "duration asked: ${DURATION_MIN} min"
        echo "output dir    : $OUT_DIR"
        echo "exit reason   : $EXIT_REASON"
        echo
        echo "Sources, and the load each applied to the device:"
        # Read the counter live rather than relying on the post-loop append: an
        # interrupted run never reaches that line, and the line count is exactly
        # what tells a reader whether a short capture holds anything. Reconnect
        # mode already puts the count in SRC_TELNET, so it is skipped here to
        # avoid printing the number twice.
        if [ "${TELNET_OK:-0}" = "1" ] && [ "${RECONNECT:-0}" != "1" ]; then
            echo "  telnet   : $SRC_TELNET; ${LINES:-0} lines"
        else
            echo "  telnet   : $SRC_TELNET"
        fi
        echo "  serial   : $SRC_SERIAL"
        echo "  rest     : $SRC_REST"
        echo "  crashlog : $SRC_CRASH"
        echo "  mqtt     : $SRC_MQTT"
        echo "  browser  : not used by this script, by design (see header)"
        echo
        echo "Platform:"
        echo "  uname     : $(uname -srm 2>/dev/null || echo unknown)"
        echo "  bash      : ${BASH_VERSION:-unknown}"
        echo "  curl      : $(command -v curl >/dev/null 2>&1 && curl --version 2>/dev/null | head -1 || echo 'not installed')"
        echo "  mosquitto : $(command -v mosquitto_sub >/dev/null 2>&1 && echo present || echo 'not installed')"
        echo
        echo "Send the whole directory. telnet.log is the one that usually answers the question."
    } > "$OUT_DIR/summary.txt" 2>/dev/null
}

cleanup() {
    # Stop children first so their logs are flushed before the summary claims
    # what ran. Killing a pid that already exited is not an error here.
    if [ -n "${MQTT_PID:-}" ]; then kill "$MQTT_PID" 2>/dev/null || true; fi
    if [ -n "${CRASH_PID:-}" ]; then kill "$CRASH_PID" 2>/dev/null || true; fi
    if [ -n "${PROBE_PID:-}" ]; then kill "$PROBE_PID" 2>/dev/null || true; fi
    write_summary
    log ""
    log "Capture written to: $OUT_DIR"
    log "Send the whole directory."
}

on_interrupt() {
    EXIT_REASON="stopped by the operator (Ctrl+C)"
    log ""
    log "Stopping early, finishing the files..."
    exit 0
}

mkdir -p "$OUT_DIR" || { warn "Cannot create $OUT_DIR"; exit 1; }
trap cleanup EXIT
trap on_interrupt INT TERM

# ------------------------------------------------------------------- banner ---
log "OTGW capture"
log "  host     : $DEVICE_HOST"
log "  duration : ${DURATION_MIN} min"
log "  output   : $OUT_DIR"
log ""
log "This capture measures the device as it is. Leave the web interface CLOSED"
log "while it runs: one open dashboard tab is enough to change what you measure."
log "Home Assistant talking over MQTT is fine, that is not the same load."
log ""

if [ "$ASSUME_YES" != "1" ]; then
    printf 'Start? [Y/n] '
    read -r reply
    case "$reply" in
        [Nn]*) EXIT_REASON="declined at the prompt"; exit 0 ;;
    esac
fi

# =============================================================================
#  Source order matters. Telnet is what this script exists for, so it is set up
#  FIRST and everything optional comes after it. The Windows script once set up
#  the browser, the crash poller and mosquitto_sub before telnet, so a reporter
#  without Mosquitto got a transcript in which the telnet log had never been
#  written. Optional sources go last, and their failures are caught here at the
#  call site rather than inside each helper.
# =============================================================================

# --- 0. serial (only when asked; replaces telnet as the primary source) ------
SERIAL_LOG="$OUT_DIR/usb-serial.log"
SERIAL_OK="0"
SRC_SERIAL="not attempted"

if [ "$SERIAL_MODE" = "1" ]; then
    # Auto-detect covers the usual adapters. cu.* rather than tty.* on macOS:
    # opening tty.* blocks until carrier detect, cu.* does not.
    if [ -z "$SERIAL_DEV" ]; then
        for _cand in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
            if [ -e "$_cand" ]; then SERIAL_DEV="$_cand"; break; fi
        done
    fi

    if [ -z "$SERIAL_DEV" ]; then
        SRC_SERIAL="FAILED: no serial device found (looked for ttyUSB*, ttyACM*, cu.usbserial*)"
        warn "serial   : no device found. Plug the gateway in over USB, or pass --serial /dev/ttyUSB0"
    elif [ ! -e "$SERIAL_DEV" ]; then
        # An explicitly named path that does not exist is a typo, and a typo
        # deserves a hard failure rather than a silent fallback.
        warn "serial   : $SERIAL_DEV does not exist."
        EXIT_REASON="the serial device given with --serial does not exist: $SERIAL_DEV"
        SRC_SERIAL="FAILED: $SERIAL_DEV does not exist"
        exit 2
    else
        # stty takes -F on Linux and -f on BSD/macOS. Try the one that works.
        STTY_FLAG="-F"
        if ! stty -F "$SERIAL_DEV" -a >/dev/null 2>&1; then
            if stty -f "$SERIAL_DEV" -a >/dev/null 2>&1; then STTY_FLAG="-f"; fi
        fi
        # clocal: do not wait for carrier detect, which never arrives on a plain
        # USB-serial adapter. -hupcl: do not drop DTR on close, which on many
        # adapters resets the ESP and would reboot the device being diagnosed.
        if stty "$STTY_FLAG" "$SERIAL_DEV" "$SERIAL_BAUD" cs8 -cstopb -parenb raw -echo clocal -hupcl 2>/dev/null; then
            if exec 4<"$SERIAL_DEV" 2>/dev/null; then
                SERIAL_OK="1"
                SRC_SERIAL="$SERIAL_DEV at ${SERIAL_BAUD} 8N1, read only"
                log "serial   : $SERIAL_DEV at ${SERIAL_BAUD} baud"
            else
                SRC_SERIAL="FAILED: cannot open $SERIAL_DEV (permissions? try the dialout group)"
                warn "serial   : cannot open $SERIAL_DEV. On Linux you usually need to be in the dialout group."
            fi
        else
            SRC_SERIAL="FAILED: stty could not configure $SERIAL_DEV at ${SERIAL_BAUD}"
            warn "serial   : stty could not configure $SERIAL_DEV"
        fi
    fi

    {
        echo "# OTGW USB serial capture"
        echo "# device  : ${SERIAL_DEV:-none found}"
        echo "# baud    : $SERIAL_BAUD 8N1"
        echo "# started : $(timestamp)"
        echo "# note    : 9600 is what the firmware configures (OTGWSerial.cpp:836)."
        echo "#           A capture at any other rate produces framing garbage that"
        echo "#           looks like a broken device but is only a wrong baud."
        echo "#"
    } > "$SERIAL_LOG"
fi

# --- 1. telnet -------------------------------------------------------------
TELNET_LOG="$OUT_DIR/telnet.log"
TELNET_OK="0"

if [ -z "$DEVICE_HOST" ]; then
    # Serial-only run: no address was given, so there is no network side.
    SRC_TELNET="skipped: no --host given (serial-only run)"
elif [ "$HAVE_DEVTCP" = "1" ]; then
    if exec 3<>"/dev/tcp/$DEVICE_HOST/$TELNET_PORT" 2>/dev/null; then
        TELNET_OK="1"
        SRC_TELNET="connected via /dev/tcp, passive read only (no debug toggles sent)"
        log "telnet   : connected to $DEVICE_HOST:$TELNET_PORT"
    else
        SRC_TELNET="FAILED to connect to $DEVICE_HOST:$TELNET_PORT"
        warn "telnet   : could not connect to $DEVICE_HOST:$TELNET_PORT"
    fi
elif command -v nc >/dev/null 2>&1; then
    SRC_TELNET="using nc (this bash has no /dev/tcp)"
    log "telnet   : /dev/tcp unavailable, falling back to nc"
else
    SRC_TELNET="unavailable: no /dev/tcp in this bash and nc is not installed"
    warn "telnet   : no /dev/tcp and no nc. The main log cannot be captured."
fi

{
    echo "# OTGW telnet capture"
    echo "# host    : $DEVICE_HOST:$TELNET_PORT"
    echo "# started : $(timestamp)"
    echo "# note    : passive read. This script sends no debug toggles, so what"
    echo "#           you see is what the device prints on its own."
    echo "#"
} > "$TELNET_LOG"

# --- 2. REST snapshot ------------------------------------------------------
REST_LOG="$OUT_DIR/rest-snapshot.log"
REST_BASE="http://$DEVICE_HOST/api/v2"
REST_OK=0
REST_FAIL=0

{
    echo "# OTGW REST snapshot"
    echo "# taken   : $(timestamp)"
    echo "#"
} > "$REST_LOG"

for ep in $( [ -n "$DEVICE_HOST" ] && echo device/info settings debug otgw/otmonitor otgw/boiler-support device/crashlog ); do
    {
        echo ""
        echo "===== GET /api/v2/$ep ====="
    } >> "$REST_LOG"
    if http_get "$REST_BASE/$ep" 10 >> "$REST_LOG" 2>&1; then
        REST_OK=$((REST_OK + 1))
    else
        REST_FAIL=$((REST_FAIL + 1))
    fi
done
if [ -n "$DEVICE_HOST" ]; then
    SRC_REST="one pass over 6 endpoints at start: $REST_OK ok, $REST_FAIL failed"
    log "rest     : $REST_OK ok, $REST_FAIL failed"
else
    SRC_REST="skipped: no --host given (serial-only run)"
fi

# --- 3. crash endpoint poller (optional, slow) -----------------------------
CRASH_LOG="$OUT_DIR/crashlog.log"
CRASH_PID=""
if [ "$CRASH_POLL_SEC" -gt 0 ] 2>/dev/null; then
    (
        while :; do
            {
                echo ""
                echo "===== $(timestamp) GET /api/v2/device/crashlog ====="
                http_get "$REST_BASE/device/crashlog" 10 2>&1
            } >> "$CRASH_LOG"
            sleep "$CRASH_POLL_SEC"
        done
    ) &
    CRASH_PID=$!
    SRC_CRASH="polled every ${CRASH_POLL_SEC}s (slow on purpose: catches a reboot without perturbing the measurement)"
    log "crashlog : polling every ${CRASH_POLL_SEC}s"
else
    SRC_CRASH="disabled by --crash-poll 0"
fi

# --- 4. MQTT mirror (optional, last, degrades) -----------------------------
MQTT_LOG="$OUT_DIR/mqtt.log"
MQTT_PID=""
if [ -z "$BROKER_HOST" ]; then
    SRC_MQTT="skipped: no --broker given"
elif ! command -v mosquitto_sub >/dev/null 2>&1; then
    SRC_MQTT="skipped: mosquitto_sub is not installed (install mosquitto-clients to include it)"
    warn "mqtt     : mosquitto_sub not installed, continuing without it"
else
    set -- -h "$BROKER_HOST" -p "$MQTT_PORT" -t "$MQTT_TOPIC" -v
    [ -n "$MQTT_USER" ] && set -- "$@" -u "$MQTT_USER"
    [ -n "$MQTT_PASS" ] && set -- "$@" -P "$MQTT_PASS"
    mosquitto_sub "$@" >> "$MQTT_LOG" 2>&1 &
    MQTT_PID=$!
    # A broker that refuses auth exits immediately; say so now rather than
    # letting the reporter discover an empty file later.
    sleep 1
    if kill -0 "$MQTT_PID" 2>/dev/null; then
        SRC_MQTT="mirroring $BROKER_HOST:$MQTT_PORT topic '$MQTT_TOPIC'"
        log "mqtt     : mirroring $BROKER_HOST:$MQTT_PORT"
    else
        MQTT_PID=""
        SRC_MQTT="FAILED: mosquitto_sub exited at once, see mqtt.log (usually auth or a wrong host)"
        warn "mqtt     : mosquitto_sub exited immediately, see mqtt.log"
    fi
fi

# =============================================================================
#  Capture loop
# =============================================================================
# Nothing reachable and nothing running: waiting out the window would cost the
# reporter minutes for an empty directory. Say what failed and stop now. The
# files written so far are still there, and summary.txt still names the cause,
# which is what makes the aborted run diagnosable.
# Not in --reconnect mode: there, a device that is unreachable right now is the
# normal starting state, because waiting for it to come back is the whole job.
if [ "$SERIAL_MODE" != "1" ] && [ "$RECONNECT" != "1" ] && [ "$TELNET_OK" != "1" ] && [ "$REST_OK" -eq 0 ] && [ -z "$MQTT_PID" ] && [ -z "$CRASH_PID" ]; then
    EXIT_REASON="nothing was reachable: telnet failed and all $REST_FAIL REST calls failed, so there was nothing to wait for"
    warn ""
    warn "Nothing on $DEVICE_HOST answered: neither port $TELNET_PORT nor the REST API."
    warn "Check the address, and that the gateway is on the network."
    warn "Not waiting out the ${DURATION_MIN} minute window, there is nothing to capture."
    exit 1
fi

DEADLINE=$(( $(date +%s) + DURATION_MIN * 60 ))
log ""
log "Capturing until $(date -d "@$DEADLINE" '+%H:%M:%S' 2>/dev/null || date -r "$DEADLINE" '+%H:%M:%S' 2>/dev/null || echo "+${DURATION_MIN}min"). Ctrl+C stops early and still writes the files."

LINES=0
FAST_FAILS=0

# --- serial capture --------------------------------------------------------
# Reads whatever the ESP puts on its UART, which on this firmware is the PIC
# conversation. Line-oriented in normal operation, so read -r works; the -t
# guard means a stream with no newlines (the signature of a wrong baud) still
# honours the deadline instead of blocking forever.
if [ "$SERIAL_MODE" = "1" ]; then
    if [ "$SERIAL_OK" != "1" ]; then
        warn "serial   : nothing to read. See summary.txt."
        EXIT_REASON="the serial device could not be opened"
        exit 1
    fi
    log ""
    log "Capturing serial for ${DURATION_MIN} min. Ctrl+C stops early and still writes the files."
    NONPRINT=0
    while [ "$(date +%s)" -lt "$DEADLINE" ]; do
        if IFS= read -r -t 10 line <&4; then
            # OTGW lines are CRLF-terminated. read strips the LF and leaves the
            # CR, which is both noise in the log and, because 0x0D is outside
            # printable ASCII, enough to make every healthy line look like a
            # baud mismatch. Strip it before anything else looks at the line.
            line="${line%$'\r'}"
            # A blank line after that strip is the tail of a CRLF pair, not
            # data. Dropping it keeps the line count honest.
            [ -z "$line" ] && continue
            printf '%s %s\n' "$(timestamp)" "$line" >> "$SERIAL_LOG"
            LINES=$(( LINES + 1 ))
            # A wrong baud shows up as bytes outside printable ASCII. Counting
            # them lets the summary say "this looks like a baud mismatch"
            # instead of leaving the reporter to puzzle over mojibake.
            case "$line" in
                *[!\ -~]*) NONPRINT=$(( NONPRINT + 1 )) ;;
            esac
            if [ $((LINES % 100)) -eq 0 ]; then
                printf '\r  %s lines' "$LINES"
            fi
        fi
    done
    exec 4<&- 2>/dev/null || true
    printf '\r'
    log "serial   : $LINES lines captured"
    SRC_SERIAL="$SRC_SERIAL; $LINES lines"
    if [ "$LINES" -gt 0 ] && [ "$NONPRINT" -ge $(( LINES / 2 )) ]; then
        SRC_SERIAL="$SRC_SERIAL; WARNING $NONPRINT of $LINES lines contain non-printable bytes, which usually means the baud rate is wrong (this firmware uses 9600)"
        warn ""
        warn "serial   : $NONPRINT of $LINES lines look like garbage."
        warn "           That is normally a baud mismatch. This firmware uses 9600."
    fi
    EXIT_REASON="completed the requested ${DURATION_MIN} minute serial capture"
    exit 0
fi

# --reconnect: for a device that goes away and comes back. Stopping at the first
# dropout would throw away the recovery, which is usually the half that says
# what happened. A reachability probe runs alongside so probe.log carries a line
# per interval whether or not the device answers: that is what dates the outage
# and lets it be lined up against telnet.log.
if [ "$RECONNECT" = "1" ]; then
    PROBE_LOG="$OUT_DIR/probe.log"
    {
        echo "# OTGW reachability probe"
        echo "# one line per ${PROBE_SEC}s, whether or not the device answers"
        echo "# http=200 answering, http=000 no answer at all"
        echo "#"
    } > "$PROBE_LOG"

    (
        while :; do
            _code="$(curl -s -o /dev/null -m 5 -w '%{http_code}' "$REST_BASE/device/info" 2>/dev/null)"
            printf '%s http=%s\n' "$(timestamp)" "${_code:-000}" >> "$PROBE_LOG"
            sleep "$PROBE_SEC"
        done
    ) &
    PROBE_PID=$!
    SRC_REST="$SRC_REST; plus a reachability probe every ${PROBE_SEC}s (see probe.log)"

    log "mode     : reconnect, watching across dropouts"
    log "probe    : every ${PROBE_SEC}s into probe.log"

    # Starts at 1 when the first connection already succeeded during setup, so
    # the summary counts connections and not just the reconnections after them.
    if [ "$TELNET_OK" = "1" ]; then CONNECTIONS=1; else CONNECTIONS=0; fi
    while [ "$(date +%s)" -lt "$DEADLINE" ]; do
        if [ "$TELNET_OK" != "1" ]; then
            printf '%s --- reconnecting ---\n' "$(timestamp)" >> "$TELNET_LOG"
            if exec 3<>"/dev/tcp/$DEVICE_HOST/$TELNET_PORT" 2>/dev/null; then
                TELNET_OK="1"
                CONNECTIONS=$(( CONNECTIONS + 1 ))
                printf '%s --- connected ---\n' "$(timestamp)" >> "$TELNET_LOG"
            else
                sleep 10
                continue
            fi
        fi

        _t_before="$(date +%s)"
        if IFS= read -r -t 30 line <&3; then
            printf '%s %s\n' "$(timestamp)" "$line" >> "$TELNET_LOG"
            LINES=$(( LINES + 1 ))
            FAST_FAILS=0
            if [ $((LINES % 200)) -eq 0 ]; then
                printf '\r  %s lines, %s connection(s)' "$LINES" "$CONNECTIONS"
            fi
        else
            if [ $(( $(date +%s) - _t_before )) -lt 2 ]; then
                FAST_FAILS=$(( FAST_FAILS + 1 ))
            else
                FAST_FAILS=0
            fi
            if [ "$FAST_FAILS" -ge 3 ]; then
                printf '%s --- connection lost ---\n' "$(timestamp)" >> "$TELNET_LOG"
                exec 3<&- 2>/dev/null || true
                TELNET_OK="0"
                FAST_FAILS=0
            fi
        fi
    done
    exec 3<&- 2>/dev/null || true
    kill "$PROBE_PID" 2>/dev/null || true
    printf '\r'
    log "telnet   : $LINES lines across $CONNECTIONS connection(s)"
    SRC_TELNET="reconnect mode; $LINES lines across $CONNECTIONS connection(s), $(( CONNECTIONS > 0 ? CONNECTIONS - 1 : 0 )) dropout(s) survived"
    EXIT_REASON="completed the requested ${DURATION_MIN} minute window in reconnect mode"
    exit 0
fi

if [ "$TELNET_OK" = "1" ]; then
    while [ "$(date +%s)" -lt "$DEADLINE" ]; do
        # read -t returns non-zero on timeout as well as on EOF, so the loop
        # condition is the deadline and not read's exit status.
        _t_before="$(date +%s)"
        if IFS= read -r -t 5 line <&3; then
            printf '%s\n' "$line" >> "$TELNET_LOG"
            LINES=$((LINES + 1))
            if [ $((LINES % 200)) -eq 0 ]; then
                printf '\r  %s lines captured' "$LINES"
            fi
        else
            # Telling a quiet bus from a closed socket, without /proc: a read on
            # a closed fd fails at once, a read on a quiet one blocks for the
            # full -t 5. So time the failure instead of inspecting the fd, which
            # works the same on macOS, in containers, and anywhere /proc is not
            # mounted. Several fast failures in a row mean the far end is gone;
            # one is just a race.
            _t_after="$(date +%s)"
            if [ $(( _t_after - _t_before )) -lt 2 ]; then
                FAST_FAILS=$(( FAST_FAILS + 1 ))
            else
                FAST_FAILS=0
            fi
            if [ "$FAST_FAILS" -ge 3 ]; then
                warn ""
                warn "telnet   : connection closed by the device"
                SRC_TELNET="$SRC_TELNET; closed by the device before the deadline"
                # Do not let the tail of this script claim the window completed:
                # a capture that ended because the device hung up is a different
                # fact from one that ran its course, and the reader needs it.
                TELNET_CLOSED_EARLY="1"
                break
            fi
        fi
    done
    exec 3<&- 2>/dev/null || true
    printf '\r'
    log "telnet   : $LINES lines captured"
elif command -v nc >/dev/null 2>&1 && [ "$HAVE_DEVTCP" != "1" ]; then
    nc "$DEVICE_HOST" "$TELNET_PORT" >> "$TELNET_LOG" 2>&1 &
    NC_PID=$!
    sleep $(( DURATION_MIN * 60 ))
    kill "$NC_PID" 2>/dev/null || true
    LINES=$(wc -l < "$TELNET_LOG" 2>/dev/null || echo 0)
    SRC_TELNET="$SRC_TELNET; $LINES lines"
else
    # No telnet at all. Still honour the duration so the MQTT and crash logs
    # cover the window the reporter was asked for.
    warn "telnet   : not captured. Waiting out the window for the other sources."
    sleep $(( DURATION_MIN * 60 ))
fi

if [ "${TELNET_CLOSED_EARLY:-0}" = "1" ]; then
    EXIT_REASON="the device closed the telnet connection before the ${DURATION_MIN} minute window was up"
else
    EXIT_REASON="completed the requested ${DURATION_MIN} minute window"
fi
exit 0
