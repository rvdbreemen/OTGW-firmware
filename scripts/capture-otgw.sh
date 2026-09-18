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
if [ -z "$DEVICE_HOST" ]; then
    printf 'Gateway address (IP or hostname): '
    read -r DEVICE_HOST
fi
if [ -z "$DEVICE_HOST" ]; then
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
        # what tells a reader whether a short capture holds anything.
        if [ "${TELNET_OK:-0}" = "1" ]; then
            echo "  telnet   : $SRC_TELNET; ${LINES:-0} lines"
        else
            echo "  telnet   : $SRC_TELNET"
        fi
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

# --- 1. telnet -------------------------------------------------------------
TELNET_LOG="$OUT_DIR/telnet.log"
TELNET_OK="0"

if [ "$HAVE_DEVTCP" = "1" ]; then
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

for ep in device/info settings debug otgw/otmonitor otgw/boiler-support device/crashlog; do
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
SRC_REST="one pass over 6 endpoints at start: $REST_OK ok, $REST_FAIL failed"
log "rest     : $REST_OK ok, $REST_FAIL failed"

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
if [ "$TELNET_OK" != "1" ] && [ "$REST_OK" -eq 0 ] && [ -z "$MQTT_PID" ] && [ -z "$CRASH_PID" ]; then
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
