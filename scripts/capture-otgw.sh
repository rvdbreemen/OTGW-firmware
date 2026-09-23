#!/usr/bin/env bash
# =============================================================================
#  capture-otgw.sh - OTGW MQTT + telnet diagnostic capture (Linux, WSL, macOS)
#
#  The Linux/macOS counterpart of capture-mqtt-debug.bat, and a port of it:
#  same options, same sources, same defaults, same output. A maintainer reading
#  a report should not be able to tell from the transcript which of the two
#  produced it, apart from the "Platform" lines in the summary.
#
#  WHAT IT CAPTURES (each source degrades on its own, none aborts the run):
#    telnet.log         the debug stream on port 23. On every (re)connect the
#                       banner's debug toggles are switched on, then 'q' and 'D'
#                       dump settings and state into the log.
#    mqtt.log           the broker, via mosquitto_sub, when it is installed
#    browser.log        a headless Chrome/Chromium/Edge/Brave loading the web UI:
#                       console, exceptions and network timings, over the Chrome
#                       DevTools Protocol. Needs python3 for the CDP client.
#    crashlog.log       /api/v2/device/crashlog and /reboot_log.txt, every 30 s
#    rest-snapshot.log  the firmware's own view, read once AFTER the capture
#  All of it is merged into ONE file at the end:
#    transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt
#
#  NEEDS: bash (3.2 or newer) and curl. Optional: mosquitto_sub (MQTT),
#  python3 plus a Chromium-family browser (browser capture). A missing optional
#  tool is reported in the summary and the capture continues without it.
#
#  Usage:
#    bash capture-otgw.sh                                   interactive prompts
#    bash capture-otgw.sh -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10
#    bash capture-otgw.sh --device-host otgw.local --broker-host ha.local --username mqtt
#    bash capture-otgw.sh --help
#  Option names match the Windows script and are case-insensitive, so
#  -DeviceHost, --device-host and --devicehost are the same option.
#
#  Copyright (c) 2021-2026 Robert van den Breemen. MIT License.
# =============================================================================

# Written for bash 3.2, which is what macOS still ships: no associative arrays,
# no mapfile, no ${var^^}, no fractional `read -t`. Do not "modernise" these
# without testing on macOS.
set -u

SCRIPT_NAME="$(basename "$0")"
SCRIPT_VERSION="2.0.0"

# ---------------------------------------------------------------- defaults ---
# Same parameter set and defaults as capture-mqtt-debug.bat.
DEVICE_HOST=""
BROKER_HOST=""
BROKER_PORT="1883"
TOPIC="#"
USERNAME=""
PASSWORD=""
OUTPUT_ROOT="logs/mqtt-diagnostics"
DURATION_SECONDS="0"
MOSQUITTO_SUB_PATH=""
SKIP_TOOL_INSTALL="0"
TELNET_CONNECT_TIMEOUT_SECONDS="6"
TELNET_RECONNECT_DELAY_MS="500"
TELNET_POST_DISCONNECT_DELAY_MS="1000"
SKIP_DEBUG_TOGGLES="0"
QUIET_DEBUG_TOGGLES="0"
KEEP_DEBUG_TOGGLES=""
SKIP_BROWSER_CAPTURE="0"
SKIP_REST_SNAPSHOT="0"
BROWSER_URL=""
BROWSER_DEBUG_PORT="9222"
BROWSER_PATH=""
SKIP_CRASHLOG_CAPTURE="0"
CRASHLOG_URL=""
CRASHLOG_POLL_SECONDS="30"
TELNET_PORT="23"

# Extras this script has on top of the Windows one.
PROBE_SECONDS="0"   # >0: reachability probe into probe.log, dates an outage
SERIAL_DEV=""       # capture the USB serial link (the Windows side has
SERIAL_AUTO="0"     # capture-usb-serial.bat for this)
# 9600 because that is what the firmware configures: OTGWSerial.cpp:836 does
# HardwareSerial::begin(9600, SERIAL_8N1). That UART is the PIC link. Any other
# rate yields framing garbage that looks like a broken device (GH #684).
SERIAL_BAUD="9600"

BOUND_DEVICE_HOST="0"
BOUND_BROKER_HOST="0"
BOUND_USERNAME="0"
BOUND_PASSWORD="0"

# Simulator toggles inject fake data rather than log, and are never touched.
DEBUG_TOGGLE_SIMULATORS="SensorSim|OTGW-Sim"

show_help() {
    cat <<EOF
OTGW MQTT diagnostic capture (Linux, WSL, macOS) - $SCRIPT_NAME $SCRIPT_VERSION

Usage:
  bash $SCRIPT_NAME [-DeviceHost <host>] [-BrokerHost <host>] [-BrokerPort <port>] [-Topic <topic>]
                    [-Username <user>] [-Password <pass>] [-DurationSeconds <seconds>]
  bash $SCRIPT_NAME --help
  Option names are case-insensitive and may be written -DeviceHost, --device-host or --devicehost.

Debug logging:
  On connect (and after every reconnect/reboot) the script enables ALL telnet logging
  toggles that are off: OTmsg, REST API, MQTT, MQTTGate, Sensors, NTP. Toggles already
  on are left as-is. Simulator toggles (SensorSim, OTGW-Sim) are never touched. Toggle keys
  are parsed from the banner, so 1.x and 2.0.0/OTGW32 layouts both work.
  It then sends 'q' (read settings) and 'D' (dump settings/state) so they land in telnet.log.
  -SkipDebugToggles             Leave every toggle as the device has it. Does NOT guarantee a
                                quiet capture: a device left verbose by an earlier run stays
                                verbose.
  -QuietDebugToggles            Actively switch OFF every toggle that is on, and switch them
                                back at the end. Use for heap-leak measurement, where the extra
                                telnet output would put the instrument itself on the suspect
                                list. logHeapStats is periodic and prints regardless, so the
                                heap trend survives.
  -KeepDebugToggles <list>      Comma-separated toggle labels to keep ON while the rest is
                                silenced, switched ON when the device has them off. Restored
                                with the others at the end. Example:
                                  -QuietDebugToggles -KeepDebugToggles "REST API,NTP"
                                Blanket silencing hides the load you are hunting; keep the few
                                low-volume toggles that cover your hypothesis.

Interactive mode:
  If DeviceHost or BrokerHost is omitted, the script prompts for the OTGW device host and MQTT broker host.
  It then prompts for an optional MQTT username. Leave it blank for anonymous brokers.
  If a username is supplied without -Password, the script prompts (without echo) for the MQTT password.
  Host/broker/username answers are remembered and pre-filled next run ([default]; Enter accepts),
  in \${XDG_CONFIG_HOME:-~/.config}/otgw-capture/capture-settings.json. The MQTT password is never saved.

MQTT:
  mosquitto_sub mirrors the broker into mqtt.log. When it is missing the capture continues without
  MQTT and says so. On macOS with Homebrew the script installs it (brew install mosquitto) unless
  -SkipToolInstall is given; on Linux it prints the install command for your distribution, because
  that needs sudo. -MosquittoSubPath <path> points at a specific copy.

Browser devtools capture (console + exceptions + resource 404s + network timings):
  Enabled by default. A headless Chrome, Chromium, Edge or Brave loads the OTGW web UI over the
  Chrome DevTools Protocol and its console/network output is written to browser.log. The CDP
  client is a small python3 program embedded in this script, so python3 is needed too.
  Browser/tool stderr is captured in error.txt, which is also merged into the final transcript.
  -SkipBrowserCapture           Disable the browser capture entirely (telnet + MQTT only).
  -SkipRestSnapshot             Skip the post-capture REST snapshot of the firmware's own state.
                                The snapshot runs once after the capture stops and records device
                                info, otmonitor, boiler-support and the in-RAM value of a curated
                                set of OpenTherm message ids. Settings are never captured, because
                                they contain the MQTT broker credentials.
  -BrowserUrl <url>             Page to load (default http://<DeviceHost>/).
  -BrowserDebugPort <port>      CDP remote-debugging port (default 9222; auto-bumped if busy).
  -BrowserPath <path>           Explicit browser executable (default: auto-detect).

Crash-log capture (decoded ESP exception: exccause + epc1/excvaddr registers):
  Enabled by default. Polls the firmware REST endpoint http://<DeviceHost>/api/v2/device/crashlog
  plus the raw /reboot_log.txt ring buffer, writing both to crashlog.log (merged into the final transcript).
  Devices without the endpoint (e.g. firmware 1.2.0) simply log a 404 and the capture continues.
  -SkipCrashlogCapture          Disable the crash-log poll entirely.
  -CrashlogUrl <url>            Crash-log endpoint (default http://<DeviceHost>/api/v2/device/crashlog).
  -CrashlogPollSeconds <n>      Poll interval in seconds (default 30, range 5-3600).

Telnet:
  -TelnetConnectTimeoutSeconds <n>           Base connect timeout (default 6, range 2-60; grows on retries, max 20).
  -TelnetReconnectDelayMilliseconds <n>      Wait after a failed connect (default 500, range 250-60000).
  -TelnetPostDisconnectDelayMilliseconds <n> Wait after a disconnect or reboot (default 1000, range 250-60000).

Other:
  -OutputRoot <dir>             Where run folders go (default logs/mqtt-diagnostics).
  -ProbeSeconds <n>             Also probe /api/v2/device/info every n seconds into probe.log, which
                                dates an outage to the second. Off by default. (--probe, --reconnect
                                from earlier versions still work; reconnecting is always on now.)
  -Serial [device]              Capture the USB serial link into usb-serial.log instead of telnet, for a
                                gateway that will not come up on WiFi. Without a device it looks for one.
                                -DeviceHost and -BrokerHost are optional then. -Baud <n> (default 9600,
                                which is what the firmware configures).

Stopping capture:
  Press Q to stop cleanly. The script closes the logs and leaves
  transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt plus error.txt.
  Ctrl+C also stops the capture cleanly and still writes the transcript.
  Or pass -DurationSeconds <seconds> to stop automatically after a fixed interval.

Examples:
  bash $SCRIPT_NAME
  bash $SCRIPT_NAME -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10 -Username mqttuser
  bash $SCRIPT_NAME --help
EOF
}

# ------------------------------------------------------------------ helpers ---
warn() { printf '%s\n' "$*" >&2; }
die()  { warn "$*"; exit 2; }

# Milliseconds since the epoch. bash 5 has EPOCHREALTIME, GNU date has %3N,
# macOS has neither but always has perl. Decided once, used everywhere.
if [ -n "${EPOCHREALTIME:-}" ]; then
    now_ms() { local t="${EPOCHREALTIME/[.,]/}"; printf '%s' "${t%???}"; }
elif date +%s%3N 2>/dev/null | grep -q '^[0-9]*$'; then
    now_ms() { date +%s%3N; }
elif command -v perl >/dev/null 2>&1; then
    now_ms() { perl -MTime::HiRes=time -e 'printf("%d", time()*1000)'; }
else
    now_ms() { printf '%s000' "$(date +%s)"; }
fi

# ISO-8601 wall clock, the equivalent of PowerShell's (Get-Date).ToString('o').
iso_now() { date '+%Y-%m-%dT%H:%M:%S%z'; }

# HH:MM:SS.mmm, for the per-line stamps in crashlog.log.
clock_ms() {
    local ms
    ms="$(now_ms)"
    printf '%s.%s' "$(date '+%H:%M:%S')" "${ms: -3}"
}

is_uint() { case "$1" in ''|*[!0-9]*) return 1 ;; *) return 0 ;; esac; }

# int_opt NAME VALUE MIN MAX: validate a numeric option the way [ValidateRange]
# does on the Windows side.
int_opt() {
    is_uint "$2" || die "$1 takes a whole number, got: $2"
    if [ -n "${3:-}" ] && { [ "$2" -lt "$3" ] || [ "$2" -gt "$4" ]; }; then
        die "$1 must be between $3 and $4, got: $2"
    fi
}

json_escape() { printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'; }

# json_str KEY < json: first string value of "KEY": "...". Enough for the flat
# REST objects this script reads, and it needs neither jq nor python.
json_str() {
    local key
    key="$(printf '%s' "$1" | sed 's/[.[\*^$]/\\&/g')"
    sed -n "s/.*\"$key\"[[:space:]]*:[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p" | head -n 1
}

# ------------------------------------------------------------ argument parse ---
# Normalise every option to lower case without dashes, so the PowerShell
# spelling from the Windows docs works here unchanged.
need_value() { [ $# -ge 2 ] || die "Option $1 needs a value."; }

while [ $# -gt 0 ]; do
    raw="$1"
    opt="$(printf '%s' "$raw" | tr '[:upper:]' '[:lower:]' | sed -e 's/^-*//' -e 's/-//g')"
    case "$opt" in
        help|h|\?)                    show_help; exit 0 ;;
        devicehost|host)              need_value "$@"; DEVICE_HOST="$2"; BOUND_DEVICE_HOST="1"; shift 2 ;;
        brokerhost|broker)            need_value "$@"; BROKER_HOST="$2"; BOUND_BROKER_HOST="1"; shift 2 ;;
        brokerport|mqttport)          need_value "$@"; BROKER_PORT="$2"; shift 2 ;;
        topic|mqtttopic)              need_value "$@"; TOPIC="$2"; shift 2 ;;
        username|mqttuser)            need_value "$@"; USERNAME="$2"; BOUND_USERNAME="1"; shift 2 ;;
        password|mqttpass)            need_value "$@"; PASSWORD="$2"; BOUND_PASSWORD="1"; shift 2 ;;
        outputroot|out)               need_value "$@"; OUTPUT_ROOT="$2"; shift 2 ;;
        durationseconds)              need_value "$@"; DURATION_SECONDS="$2"; shift 2 ;;
        minutes)                      need_value "$@"; is_uint "$2" || die "--minutes takes a whole number"
                                      DURATION_SECONDS=$(( $2 * 60 )); shift 2 ;;
        mosquittosubpath)             need_value "$@"; MOSQUITTO_SUB_PATH="$2"; shift 2 ;;
        skiptoolinstall)              SKIP_TOOL_INSTALL="1"; shift ;;
        telnetconnecttimeoutseconds)  need_value "$@"; TELNET_CONNECT_TIMEOUT_SECONDS="$2"; shift 2 ;;
        telnetreconnectdelaymilliseconds)      need_value "$@"; TELNET_RECONNECT_DELAY_MS="$2"; shift 2 ;;
        telnetpostdisconnectdelaymilliseconds) need_value "$@"; TELNET_POST_DISCONNECT_DELAY_MS="$2"; shift 2 ;;
        telnetport)                   need_value "$@"; TELNET_PORT="$2"; shift 2 ;;
        skipdebugtoggles)             SKIP_DEBUG_TOGGLES="1"; shift ;;
        quietdebugtoggles)            QUIET_DEBUG_TOGGLES="1"; shift ;;
        keepdebugtoggles)             need_value "$@"; KEEP_DEBUG_TOGGLES="$2"; shift 2 ;;
        skipbrowsercapture)           SKIP_BROWSER_CAPTURE="1"; shift ;;
        skiprestsnapshot)             SKIP_REST_SNAPSHOT="1"; shift ;;
        browserurl)                   need_value "$@"; BROWSER_URL="$2"; shift 2 ;;
        browserdebugport)             need_value "$@"; BROWSER_DEBUG_PORT="$2"; shift 2 ;;
        browserpath)                  need_value "$@"; BROWSER_PATH="$2"; shift 2 ;;
        skipcrashlogcapture)          SKIP_CRASHLOG_CAPTURE="1"; shift ;;
        crashlogurl)                  need_value "$@"; CRASHLOG_URL="$2"; shift 2 ;;
        crashlogpollseconds|crashpoll) need_value "$@"; CRASHLOG_POLL_SECONDS="$2"; shift 2 ;;
        probeseconds|probe)           need_value "$@"; PROBE_SECONDS="$2"; shift 2 ;;
        reconnect|yes|y)              shift ;;   # earlier versions; reconnecting is always on, no prompt to skip
        serial)
            # Optional argument: -Serial /dev/ttyUSB0, or bare -Serial to
            # auto-detect. A following word starting with - is the next option.
            case "${2:-}" in
                ""|-*) SERIAL_AUTO="1"; shift ;;
                *)     SERIAL_DEV="$2"; shift 2 ;;
            esac ;;
        baud)                         need_value "$@"; SERIAL_BAUD="$2"; shift 2 ;;
        *) warn "Unknown option: $raw"; warn "Try --help."; exit 2 ;;
    esac
done

int_opt "-BrokerPort" "$BROKER_PORT" 1 65535
int_opt "-DurationSeconds" "$DURATION_SECONDS"
int_opt "-TelnetConnectTimeoutSeconds" "$TELNET_CONNECT_TIMEOUT_SECONDS" 2 60
int_opt "-TelnetReconnectDelayMilliseconds" "$TELNET_RECONNECT_DELAY_MS" 250 60000
int_opt "-TelnetPostDisconnectDelayMilliseconds" "$TELNET_POST_DISCONNECT_DELAY_MS" 250 60000
int_opt "-BrowserDebugPort" "$BROWSER_DEBUG_PORT" 1 65535
int_opt "-CrashlogPollSeconds" "$CRASHLOG_POLL_SECONDS" 5 3600
int_opt "-TelnetPort" "$TELNET_PORT" 1 65535
int_opt "-ProbeSeconds" "$PROBE_SECONDS"
int_opt "-Baud" "$SERIAL_BAUD"

SERIAL_MODE="0"
if [ -n "$SERIAL_DEV" ] || [ "$SERIAL_AUTO" = "1" ]; then SERIAL_MODE="1"; fi

# A wrong path the user typed themselves is fatal, not silently ignored; checked
# here, before any background source has started.
if [ -n "$MOSQUITTO_SUB_PATH" ] && [ ! -x "$MOSQUITTO_SUB_PATH" ]; then
    die "Explicit -MosquittoSubPath was not found: $MOSQUITTO_SUB_PATH"
fi

command -v curl >/dev/null 2>&1 || die "curl is required and was not found. Install it (apt install curl, dnf install curl) and re-run."

# /dev/tcp is a bash feature that some distributions build out. Check once, up
# front, so the failure is a sentence rather than a silent empty telnet.log.
if bash -c 'exec 3<>/dev/tcp/127.0.0.1/1' 2>&1 | grep -qi 'not supported\|no such file'; then
    [ "$SERIAL_MODE" = "1" ] || die "This bash has no /dev/tcp support, so the telnet stream cannot be captured."
fi

# ---------------------------------------------------------- remembered settings ---
SETTINGS_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/otgw-capture/capture-settings.json"

saved_setting() {
    [ -f "$SETTINGS_FILE" ] || return 0
    json_str "$1" < "$SETTINGS_FILE" 2>/dev/null
}

save_settings() {
    # The MQTT password is a secret and is deliberately NEVER written here.
    # Best-effort: a write failure must never abort the capture.
    local dir
    dir="$(dirname "$SETTINGS_FILE")"
    mkdir -p "$dir" 2>/dev/null || return 1
    {
        printf '{\n'
        printf '    "DeviceHost":  "%s",\n' "$(json_escape "$DEVICE_HOST")"
        printf '    "BrokerHost":  "%s",\n' "$(json_escape "$BROKER_HOST")"
        printf '    "BrokerPort":  %s,\n'   "$BROKER_PORT"
        printf '    "Topic":  "%s",\n'      "$(json_escape "$TOPIC")"
        printf '    "Username":  "%s"\n'    "$(json_escape "$USERNAME")"
        printf '}\n'
    } > "$SETTINGS_FILE" 2>/dev/null || return 1
    printf '%s' "$SETTINGS_FILE"
}

# read_with_default PROMPT DEFAULT -> answer on stdout
read_with_default() {
    local answer=""
    if [ -n "$2" ]; then
        printf '%s [%s]: ' "$1" "$2" >&2
        IFS= read -r answer || true
        [ -n "$answer" ] || answer="$2"
    else
        printf '%s: ' "$1" >&2
        IFS= read -r answer || true
    fi
    printf '%s' "$answer"
}

# ------------------------------------------------------------------- prompts ---
# Same flow as the Windows script. Serial mode is for a gateway that will not
# come up on WiFi, so there the network addresses are optional.
if [ "$SERIAL_MODE" != "1" ]; then
    [ -n "$DEVICE_HOST" ] || DEVICE_HOST="$(read_with_default "OTGW device host" "$(saved_setting DeviceHost)")"
    [ -n "$BROKER_HOST" ] || BROKER_HOST="$(read_with_default "MQTT broker host" "$(saved_setting BrokerHost)")"
    [ -n "$DEVICE_HOST" ] || die "DeviceHost is required."
    [ -n "$BROKER_HOST" ] || die "BrokerHost is required."

    if [ "$BOUND_USERNAME" != "1" ] && { [ "$BOUND_DEVICE_HOST" != "1" ] || [ "$BOUND_BROKER_HOST" != "1" ]; }; then
        USERNAME="$(read_with_default "MQTT username (blank for anonymous)" "$(saved_setting Username)")"
    fi
fi

if [ -z "$USERNAME" ]; then
    if [ "$BOUND_PASSWORD" = "1" ] && [ -n "$PASSWORD" ]; then
        die "Username is required when Password is supplied."
    fi
elif [ "$BOUND_PASSWORD" != "1" ]; then
    printf 'MQTT password for %s: ' "$USERNAME" >&2
    IFS= read -rs PASSWORD || true
    printf '\n' >&2
fi

SAVED_SETTINGS_PATH=""
if [ "$SERIAL_MODE" != "1" ]; then
    SAVED_SETTINGS_PATH="$(save_settings || true)"
fi

# ------------------------------------------------------------------ run folder ---
mkdir -p "$OUTPUT_ROOT" 2>/dev/null || die "Cannot create $OUTPUT_ROOT"
OUTPUT_ROOT_PATH="$(cd "$OUTPUT_ROOT" && pwd)"
RUN_NAME="$(date '+%Y%m%d-%H%M%S')"
RUN_PATH="$OUTPUT_ROOT_PATH/$RUN_NAME"
mkdir -p "$RUN_PATH" || die "Cannot create $RUN_PATH"

TELNET_LOG="$RUN_PATH/telnet.log"
SERIAL_LOG="$RUN_PATH/usb-serial.log"
MQTT_LOG="$RUN_PATH/mqtt.log"
MQTT_ERROR_LOG="$RUN_PATH/mqtt.stderr.log"
BROWSER_LOG="$RUN_PATH/browser.log"
BROWSER_ERROR_LOG="$RUN_PATH/browser.stderr.log"
CRASHLOG_LOG="$RUN_PATH/crashlog.log"
PROBE_LOG="$RUN_PATH/probe.log"
REST_SNAPSHOT_LOG="$RUN_PATH/rest-snapshot.log"
TOOL_ERROR_LOG="$RUN_PATH/error.txt"
SCRIPT_ERROR_LOG="$RUN_PATH/script.error.log"
SUMMARY_PATH="$RUN_PATH/summary.txt"
# Stop flag, FIFOs and worker status live outside the run folder: a FIFO cannot
# be created on every filesystem (WSL's /mnt/c drvfs among them).
STATE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/otgw-capture.XXXXXX")" || die "Cannot create a temporary directory"
STOP_FLAG="$STATE_DIR/stop"
: > "$SUMMARY_PATH"

add_summary_line() { printf '%s\n' "$*" >> "$SUMMARY_PATH"; }
write_capture_status() { add_summary_line "$*"; printf '%s\n' "$*"; }
add_tool_error_line() { printf '%s  %s\n' "$(iso_now)" "$*" >> "$SCRIPT_ERROR_LOG"; }

# Keep-list membership with the same trimming the Windows script applies.
keep_list_has() {
    local item old_ifs="$IFS"
    IFS=','
    for item in $KEEP_DEBUG_TOGGLES; do
        item="$(printf '%s' "$item" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
        if [ -n "$item" ] && [ "$item" = "$1" ]; then IFS="$old_ifs"; return 0; fi
    done
    IFS="$old_ifs"
    return 1
}
keep_list_display() {
    printf '%s' "$KEEP_DEBUG_TOGGLES" | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | grep -v '^$' | paste -sd, - | sed 's/,/, /g'
}

add_summary_line "OTGW MQTT diagnostic capture"
add_summary_line "Run folder: $RUN_PATH"
add_summary_line "Started: $(iso_now)"
add_summary_line "DeviceHost: ${DEVICE_HOST:-(none, serial-only run)}"
add_summary_line "TelnetPort: $TELNET_PORT"
add_summary_line "TelnetConnectTimeoutSeconds: $TELNET_CONNECT_TIMEOUT_SECONDS"
add_summary_line "TelnetPostDisconnectDelayMilliseconds: $TELNET_POST_DISCONNECT_DELAY_MS"
add_summary_line "TelnetReconnectDelayMilliseconds: $TELNET_RECONNECT_DELAY_MS"
add_summary_line "BrokerHost: ${BROKER_HOST:-(none)}"
add_summary_line "BrokerPort: $BROKER_PORT"
add_summary_line "Topic: $TOPIC"
if [ -n "$USERNAME" ]; then add_summary_line "Username supplied: True"; else add_summary_line "Username supplied: False"; fi
if [ "$DURATION_SECONDS" -gt 0 ]; then add_summary_line "DurationSeconds: $DURATION_SECONDS"; else add_summary_line "DurationSeconds: until manual stop"; fi
if [ "$SKIP_TOOL_INSTALL" = "1" ]; then add_summary_line "SkipToolInstall: True"; else add_summary_line "SkipToolInstall: False"; fi
add_summary_line "Tool stderr log: $TOOL_ERROR_LOG"
[ -n "$SAVED_SETTINGS_PATH" ] && add_summary_line "Settings prefill saved: $SAVED_SETTINGS_PATH"
add_summary_line "Telnet timeout strategy: adaptive (base + retry backoff, capped at 20s)"
# Record the toggle policy up front, not only on a successful connect, so a
# shared transcript always says what load the tooling itself applied.
if [ "$QUIET_DEBUG_TOGGLES" = "1" ]; then
    if [ -n "$(keep_list_display)" ]; then
        add_summary_line "Debug toggle policy: silence all except [$(keep_list_display)], restore on exit (-QuietDebugToggles -KeepDebugToggles)"
    else
        add_summary_line "Debug toggle policy: silence all that are on, restore on exit (-QuietDebugToggles)"
    fi
elif [ "$SKIP_DEBUG_TOGGLES" = "1" ]; then
    add_summary_line "Debug toggle policy: leave as-is (-SkipDebugToggles)"
else
    add_summary_line "Debug toggle policy: enable all that are off"
fi
add_summary_line "Platform: $(uname -srm 2>/dev/null || echo unknown); bash ${BASH_VERSION:-unknown}; script $SCRIPT_NAME $SCRIPT_VERSION"

# ----------------------------------------------------------------- stop handling ---
CAPTURE_STOP_REASON=""
request_capture_stop() {
    [ -n "$CAPTURE_STOP_REASON" ] || CAPTURE_STOP_REASON="$1"
    : > "$STOP_FLAG" 2>/dev/null || true
}
stop_requested() { [ -f "$STOP_FLAG" ]; }

# Ctrl+C sets the flag instead of killing the script, so the teardown still
# runs and the transcript is still written. Background workers never see the
# SIGINT: in a non-interactive shell, asynchronous commands ignore it.
trap 'request_capture_stop "Ctrl+C"' INT TERM

HAVE_TTY="0"
if [ -t 0 ] && [ -r /dev/tty ]; then HAVE_TTY="1"; fi
if [ "${BASH_VERSINFO[0]:-3}" -ge 4 ]; then KEY_WAIT="0.2"; else KEY_WAIT="1"; fi

# One pass of "wait a little, and notice Q". bash 3.2 has no fractional read
# timeout, so there the Q check waits up to a second; the capture itself runs
# in background processes and loses nothing to that.
poll_console() {
    local key=""
    if [ "$HAVE_TTY" = "1" ]; then
        if IFS= read -rsn1 -t "$KEY_WAIT" key < /dev/tty 2>/dev/null; then
            case "$key" in q|Q) request_capture_stop "Q key" ;; esac
        fi
    else
        sleep 0.2
    fi
}

# wait_capture_delay MS: sleep in slices, return early on a stop request.
wait_capture_delay() {
    local deadline=$(( $(now_ms) + $1 ))
    while ! stop_requested && [ "$(now_ms)" -lt "$deadline" ]; do
        sleep 0.1
    done
}

# ===============================================================================
#  Telnet. A background worker owns the socket: it opens /dev/tcp, copies the
#  device output into telnet.log with cat, and forwards whatever the main loop
#  writes into its FIFO to the device. That split is what lets a bash 3.2 script
#  read and write one socket at the same time. The worker exits when the device
#  closes the connection, which is how a disconnect is noticed.
# ===============================================================================
TELNET_PID=""
TELNET_FIFO=""
TELNET_CONN_SEQ=0
TELNET_CONNECTED_FLAG=""
TOGGLED_KEYS=""       # keys this run flipped, in either direction
CONNECT_OFFSET=0

log_size() { if [ -f "$1" ]; then wc -c < "$1" | tr -d ' '; else echo 0; fi; }

telnet_worker() {
    # $1 host $2 port $3 fifo $4 connected-flag $5 log
    exec 3<>"/dev/tcp/$1/$2" || exit 7
    : > "$4"
    cat "$3" >&3 &
    local writer=$!
    cat <&3 >> "$5"
    kill "$writer" 2>/dev/null
    wait "$writer" 2>/dev/null
}

telnet_alive() { [ -n "$TELNET_PID" ] && kill -0 "$TELNET_PID" 2>/dev/null; }

close_telnet() {
    if [ -n "$TELNET_PID" ]; then
        # The worker's cat children would otherwise keep the socket open.
        # SIGPIPE rather than SIGTERM: it stops cat just the same, and bash
        # does not report it, so no "Terminated" noise lands in error.txt.
        pkill -PIPE -P "$TELNET_PID" 2>/dev/null
        kill -PIPE "$TELNET_PID" 2>/dev/null
        wait "$TELNET_PID" 2>/dev/null
    fi
    exec 5>&- 2>/dev/null
    [ -n "$TELNET_FIFO" ] && rm -f "$TELNET_FIFO"
    TELNET_PID=""
    TELNET_FIFO=""
}

send_key() { printf '%s' "$1" >&5 2>/dev/null; }

# Bytes the device sent since this connection opened.
telnet_since_connect() { tail -c +"$(( CONNECT_OFFSET + 1 ))" "$TELNET_LOG" 2>/dev/null; }

# Drain: wait until telnet.log stops growing for IDLE ms, capped at CAP ms.
drain_telnet() {
    local idle_ms="${1:-800}" cap_ms="${2:-5000}" last size quiet=0
    local deadline=$(( $(now_ms) + cap_ms ))
    last="$(log_size "$TELNET_LOG")"
    while [ "$(now_ms)" -lt "$deadline" ]; do
        sleep 0.1
        size="$(log_size "$TELNET_LOG")"
        if [ "$size" != "$last" ]; then last="$size"; quiet=0
        else quiet=$(( quiet + 100 )); [ "$quiet" -ge "$idle_ms" ] && break
        fi
    done
}

# Wait for the end-of-banner footer, so every toggle line is present before
# the states are parsed. Same markers the Windows script breaks on.
read_initial_banner() {
    local deadline=$(( $(now_ms) + 6000 ))
    while [ "$(now_ms)" -lt "$deadline" ]; do
        if telnet_since_connect | grep -q "Press 'h' for command menu\|OTGW-Sim"; then
            break
        fi
        sleep 0.1
    done
    telnet_since_connect
}

# parse_toggles < banner -> lines "key<TAB>label<TAB>state". Matches the same
# "<key> <Label> [<0|1>]" triplets as the Windows regex. Status flags such as
# "[-D---W--]" never match because the bracket must hold a single 0 or 1.
parse_toggles() {
    tr -d '\r' | awk '
    {
        s = $0
        while (match(s, /(^|[ \t])[^ \t][ \t]+[A-Za-z][A-Za-z0-9 \/]*\[[01]\]/)) {
            tok = substr(s, RSTART, RLENGTH)
            s = substr(s, RSTART + RLENGTH)
            sub(/^[ \t]+/, "", tok)
            key = substr(tok, 1, 1)
            rest = substr(tok, 2)
            state = substr(rest, length(rest) - 1, 1)
            label = substr(rest, 1, index(rest, "[") - 1)
            gsub(/^[ \t]+|[ \t]+$/, "", label)
            if (label == "" || seen[label]++) continue
            printf "%s\t%s\t%s\n", key, label, state
        }
    }'
}

is_simulator() { case "|$DEBUG_TOGGLE_SIMULATORS|" in *"|$1|"*) return 0 ;; esac; return 1; }

# Enable every toggle that is off (default policy).
enable_all_debug_if_needed() {
    local key label state results=""
    while IFS="$(printf '\t')" read -r key label state; do
        [ -n "$label" ] || continue
        if is_simulator "$label"; then
            results="$results; $label=skipped (simulator)"
        elif [ "$state" = "0" ]; then
            send_key "$key"; sleep 0.3
            results="$results; $label=on (sent '$key')"
        else
            results="$results; $label=already-on"
        fi
    done <<EOF
$(printf '%s' "$1" | parse_toggles)
EOF
    results="${results#; }"
    printf '%s' "${results:-no debug toggles found in banner}"
}

# Drive every toggle to its wanted state: ON for the keep list, OFF otherwise.
# Records what it flipped so the teardown can restore exactly that.
disable_all_debug_if_needed() {
    local key label state wanted results="" flipped=""
    while IFS="$(printf '\t')" read -r key label state; do
        [ -n "$label" ] || continue
        if is_simulator "$label"; then
            results="$results; $label=skipped (simulator)"; continue
        fi
        if keep_list_has "$label"; then wanted="1"; else wanted="0"; fi
        if [ "$state" != "$wanted" ]; then
            send_key "$key"; sleep 0.3
            flipped="$flipped$key"
            if [ "$wanted" = "1" ]; then results="$results; $label=on (kept) (sent '$key')"
            else results="$results; $label=off (sent '$key')"; fi
        else
            if [ "$wanted" = "1" ]; then results="$results; $label=already-on (kept)"
            else results="$results; $label=already-off"; fi
        fi
    done <<EOF
$(printf '%s' "$1" | parse_toggles)
EOF
    # Runs inside $(...), so the flipped keys go through a file, not a variable.
    printf '%s' "$flipped" > "$STATE_DIR/toggled"
    results="${results#; }"
    printf '%s' "${results:-no debug toggles found in banner}"
}

restore_debug_toggles() {
    # Best-effort: a crashed or rebooted device leaves a dead socket, and a
    # failed restore must not mask the real outcome of the capture.
    local i n key restored=0
    n=${#TOGGLED_KEYS}
    [ "$n" -gt 0 ] || return 0
    telnet_alive || { add_summary_line "Debug toggles restore failed (device likely gone): telnet connection is closed"; return 0; }
    i=0
    while [ "$i" -lt "$n" ]; do
        key="${TOGGLED_KEYS:$i:1}"
        if send_key "$key"; then restored=$(( restored + 1 )); fi
        sleep 0.2
        i=$(( i + 1 ))
    done
    if [ "$restored" -eq "$n" ]; then
        add_summary_line "Debug toggles restored: $restored toggle(s) switched back on."
    else
        add_summary_line "Debug toggles restore: partial ($restored of $n)"
    fi
    TOGGLED_KEYS=""
}

# connect_telnet TIMEOUT_S: 0 on success, 1 on failure (reason in TELNET_ERROR).
TELNET_ERROR=""
connect_telnet() {
    local timeout_s="$1" deadline banner toggle_action
    add_summary_line "Telnet connect started: $DEVICE_HOST:$TELNET_PORT (timeout ${timeout_s}s)"
    TELNET_CONN_SEQ=$(( TELNET_CONN_SEQ + 1 ))
    TELNET_FIFO="$STATE_DIR/telnet-in.$TELNET_CONN_SEQ"
    TELNET_CONNECTED_FLAG="$STATE_DIR/telnet-connected.$TELNET_CONN_SEQ"
    rm -f "$TELNET_FIFO" "$TELNET_CONNECTED_FLAG"
    mkfifo "$TELNET_FIFO" || { TELNET_ERROR="cannot create FIFO $TELNET_FIFO"; return 1; }
    CONNECT_OFFSET="$(log_size "$TELNET_LOG")"
    telnet_worker "$DEVICE_HOST" "$TELNET_PORT" "$TELNET_FIFO" "$TELNET_CONNECTED_FLAG" "$TELNET_LOG" 2>>"$SCRIPT_ERROR_LOG" &
    TELNET_PID=$!

    deadline=$(( $(now_ms) + timeout_s * 1000 ))
    while [ ! -f "$TELNET_CONNECTED_FLAG" ]; do
        if ! telnet_alive; then
            TELNET_ERROR="connection refused or host unreachable"
            close_telnet; return 1
        fi
        if [ "$(now_ms)" -ge "$deadline" ]; then
            TELNET_ERROR="Timed out connecting to $DEVICE_HOST:$TELNET_PORT after $timeout_s seconds."
            close_telnet; return 1
        fi
        if stop_requested; then TELNET_ERROR="stop requested"; close_telnet; return 1; fi
        sleep 0.1
    done
    # Read-write open never blocks on a FIFO, and keeps it open for writes.
    exec 5<>"$TELNET_FIFO"
    add_summary_line "Telnet connected: $(iso_now)"

    banner="$(read_initial_banner)"
    if [ "$QUIET_DEBUG_TOGGLES" = "1" ]; then
        toggle_action="$(disable_all_debug_if_needed "$banner")"
        TOGGLED_KEYS="$(cat "$STATE_DIR/toggled" 2>/dev/null)"
    elif [ "$SKIP_DEBUG_TOGGLES" = "1" ]; then
        toggle_action="skipped (-SkipDebugToggles): toggles left as the device had them"
    else
        toggle_action="$(enable_all_debug_if_needed "$banner")"
    fi
    add_summary_line "Debug toggle actions: $toggle_action"

    # 'q' = force read settings, 'D' = dump settings and state (full INI dump on
    # 1.x). Unknown keys are ignored by the firmware, so both are safe.
    send_key "q"; drain_telnet 800 5000
    send_key "D"; drain_telnet 800 5000
    add_summary_line "Settings dump: sent 'q' (read settings) + 'D' (dump settings/state)"
    return 0
}

effective_connect_timeout() {
    # Grow the timeout on retries so .local/mDNS and reboot windows can recover.
    local extra=$(( $1 - 1 ))
    [ "$extra" -lt 0 ] && extra=0
    [ "$extra" -gt 6 ] && extra=6
    local t=$(( TELNET_CONNECT_TIMEOUT_SECONDS + extra ))
    [ "$t" -gt 20 ] && t=20
    printf '%s' "$t"
}

# ===============================================================================
#  Crash-log poller (background). Logs a body only when it changes, so a stable
#  device does not flood the log while a crash-looping one records each entry.
# ===============================================================================
crash_line() { printf '%s  %s\n' "$(clock_ms)" "$*" >> "$CRASHLOG_LOG"; }

# http_get URL TIMEOUT: sets HTTP_STATUS ("" on transport failure), HTTP_BODY, HTTP_ERROR.
http_get() {
    local out rc errfile
    errfile="$(mktemp "$STATE_DIR/curl.XXXXXX")"
    out="$(curl -sS -m "$2" -w '\n%{http_code}' "$1" 2>"$errfile")"
    rc=$?
    HTTP_STATUS="${out##*$'\n'}"
    HTTP_BODY="${out%$'\n'*}"
    HTTP_ERROR=""
    if [ "$rc" -ne 0 ] || [ "$HTTP_STATUS" = "000" ]; then
        # Classify the transport failure, so a poll that loses the ESP8266
        # single-loop accept race reads differently from a dead device.
        case "$rc" in
            28) HTTP_ERROR="Timeout" ;;
            7)  HTTP_ERROR="ConnectFailure" ;;
            6)  HTTP_ERROR="NameResolutionFailure" ;;
            52) HTTP_ERROR="ReceiveFailure (empty reply)" ;;
            56) HTTP_ERROR="ReceiveFailure (connection reset)" ;;
            *)  HTTP_ERROR="curl exit $rc" ;;
        esac
        HTTP_ERROR="$HTTP_ERROR - $(tr -d '\r' < "$errfile" | head -n 1)"
        HTTP_STATUS=""
        HTTP_BODY=""
    fi
    rm -f "$errfile"
}

# The device serves one HTTP client at a time, and a heavy handler can block
# the accept path for a few seconds. One retry after 1.5 s before a poll counts
# as failed; that is extra load only when a poll actually fails.
http_get_resilient() {
    http_get "$1" 10
    if [ -z "$HTTP_STATUS" ] && ! stop_requested; then
        sleep 1.5
        http_get "$1" 10
    fi
}

crashlog_worker() {
    local polls=0 last_crash="" last_reboot="" trimmed elapsed
    local reboot_url="$1"
    crash_line "crashlog endpoint: $CRASHLOG_URL"
    crash_line "reboot-log file:   $reboot_url"
    crash_line "poll interval:     ${CRASHLOG_POLL_SECONDS}s"
    while :; do
        polls=$(( polls + 1 ))
        http_get_resilient "$CRASHLOG_URL"
        if [ "$HTTP_STATUS" = "200" ] && [ -n "$HTTP_BODY" ]; then
            trimmed="$(printf '%s' "$HTTP_BODY" | tr -d '\r' | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
            if [ "$trimmed" != "$last_crash" ]; then
                crash_line "[crashlog #$polls CHANGED] $trimmed"
                last_crash="$trimmed"
            fi
        elif [ -n "$HTTP_STATUS" ]; then
            if [ "status-$HTTP_STATUS" != "$last_crash" ]; then
                crash_line "[crashlog #$polls] HTTP $HTTP_STATUS (endpoint unavailable on this firmware?)"
                last_crash="status-$HTTP_STATUS"
            fi
        else
            crash_line "[crashlog #$polls] request failed after retry: $HTTP_ERROR"
        fi

        http_get_resilient "$reboot_url"
        if [ "$HTTP_STATUS" = "200" ] && [ -n "$HTTP_BODY" ]; then
            trimmed="$(printf '%s' "$HTTP_BODY" | tr -d '\r' | sed -e '/^[[:space:]]*$/d')"
            if [ -n "$trimmed" ] && [ "$trimmed" != "$last_reboot" ]; then
                crash_line "[reboot_log.txt #$polls CHANGED]"
                printf '%s\n' "$trimmed" | while IFS= read -r line; do crash_line "  | $line"; done
                last_reboot="$trimmed"
            fi
        fi

        elapsed=0
        while [ "$elapsed" -lt $(( CRASHLOG_POLL_SECONDS * 1000 )) ]; do
            stop_requested && break
            sleep 0.2
            elapsed=$(( elapsed + 200 ))
        done
        stop_requested && break
    done
    crash_line "stop requested; closing crash-log capture ($polls polls)"
    printf 'Crash-log capture: completed normally.' > "$STATE_DIR/crashlog.status"
}

probe_worker() {
    printf '# OTGW reachability probe, one line per %ss: http=200 answering, http=000 no answer\n' "$PROBE_SECONDS" > "$PROBE_LOG"
    while ! stop_requested; do
        printf '%s http=%s\n' "$(iso_now)" "$(curl -s -o /dev/null -m 5 -w '%{http_code}' "http://$DEVICE_HOST/api/v2/device/info" 2>/dev/null || true)" >> "$PROBE_LOG"
        wait_capture_delay $(( PROBE_SECONDS * 1000 ))
    done
}

# ===============================================================================
#  Browser capture. The browser is started here; a small python3 program talks
#  the Chrome DevTools Protocol to it over a websocket, using only the standard
#  library, and writes browser.log. It stops when the stop flag appears.
# ===============================================================================
write_cdp_worker() {
    cat > "$1" <<'PYEOF'
import base64, json, os, socket, struct, sys, time, urllib.request

port, device_url, log_path, stop_path, status_path, browser_path = sys.argv[1:7]
log = open(log_path, "a", encoding="utf-8", buffering=1)

def line(text):
    t = time.time()
    log.write("%s.%03d  %s\n" % (time.strftime("%H:%M:%S", time.localtime(t)), int((t % 1) * 1000), text))

def stop():
    return os.path.exists(stop_path)

class WebSocket:
    """Minimal RFC 6455 client: text frames out (masked), frames in."""
    def __init__(self, url):
        rest = url.split("://", 1)[1]
        hostport, path = rest.split("/", 1)
        host, _, p = hostport.partition(":")
        self.sock = socket.create_connection((host, int(p or 80)), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(("GET /%s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\n"
                           "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
                           "Sec-WebSocket-Version: 13\r\n\r\n" % (path, hostport, key)).encode())
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise IOError("websocket handshake: connection closed")
            head += chunk
        status = head.split(b"\r\n", 1)[0]
        if b" 101 " not in status:
            raise IOError("websocket handshake refused: %s" % status.decode(errors="replace"))
        self.buf = head.split(b"\r\n\r\n", 1)[1]
        self.parts = []

    def send_text(self, text):
        data = text.encode()
        hdr = bytearray([0x81])
        n = len(data)
        if n < 126:
            hdr.append(0x80 | n)
        elif n < 65536:
            hdr.append(0x80 | 126); hdr += struct.pack(">H", n)
        else:
            hdr.append(0x80 | 127); hdr += struct.pack(">Q", n)
        mask = os.urandom(4)
        self.sock.sendall(bytes(hdr) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _frame(self):
        b = self.buf
        if len(b) < 2:
            return None
        n = b[1] & 0x7F
        off = 2
        if n == 126:
            if len(b) < 4: return None
            n = struct.unpack(">H", b[2:4])[0]; off = 4
        elif n == 127:
            if len(b) < 10: return None
            n = struct.unpack(">Q", b[2:10])[0]; off = 10
        if b[1] & 0x80:
            off += 4  # servers do not mask, but be tolerant
        if len(b) < off + n:
            return None
        self.buf = b[off + n:]
        return b[0] & 0x80, b[0] & 0x0F, b[off:off + n]

    def recv(self, timeout):
        """Return a complete text message, None on timeout, raise EOFError on close."""
        self.sock.settimeout(timeout)
        while True:
            fr = self._frame()
            if fr is None:
                try:
                    chunk = self.sock.recv(65536)
                except socket.timeout:
                    return None
                if not chunk:
                    raise EOFError()
                self.buf += chunk
                continue
            fin, op, payload = fr
            if op == 0x8:
                raise EOFError()
            if op == 0x9:
                self.sock.sendall(bytes([0x8A, 0x80 | len(payload)]) + b"\0\0\0\0" + payload)
                continue
            if op in (0x1, 0x2, 0x0):
                self.parts.append(payload)
                if fin:
                    msg = b"".join(self.parts); self.parts = []
                    return msg.decode("utf-8", errors="replace")

summary = "Browser capture: completed normally."
requests = {}
try:
    line("browser executable: %s" % browser_path)
    line("device url: %s   cdp port: %s" % (device_url, port))
    # Attach to the about:blank page target FIRST, then navigate, so page-load
    # console logs and resource requests are captured from the very start.
    ws_url = None
    deadline = time.time() + 15
    while time.time() < deadline and not stop():
        try:
            targets = json.loads(urllib.request.urlopen("http://127.0.0.1:%s/json" % port, timeout=2).read())
            pages = [t for t in targets if t.get("type") == "page" and t.get("webSocketDebuggerUrl")]
            if pages:
                ws_url = pages[0]["webSocketDebuggerUrl"]
                break
        except Exception:
            time.sleep(0.3)
        time.sleep(0.2)
    if not ws_url:
        raise RuntimeError("CDP page target not found on port %s within 15s" % port)
    line("cdp websocket: %s" % ws_url)
    ws = WebSocket(ws_url)
    line("cdp connected")
    msg_id = 0
    def cdp(method, params=None):
        global msg_id
        msg_id += 1
        payload = {"id": msg_id, "method": method}
        if params:
            payload["params"] = params
        ws.send_text(json.dumps(payload))
    for m in ("Network.enable", "Runtime.enable", "Log.enable", "Page.enable"):
        cdp(m)
    cdp("Page.navigate", {"url": device_url})
    line("domains enabled; navigating to %s" % device_url)

    while not stop():
        try:
            raw = ws.recv(0.75)
        except EOFError:
            break
        if raw is None:
            continue
        try:
            evt = json.loads(raw)
        except ValueError:
            continue
        method = evt.get("method")
        p = evt.get("params") or {}
        if method == "Runtime.consoleAPICalled":
            parts = []
            for a in p.get("args", []):
                if a.get("value") is not None: parts.append(str(a["value"]))
                elif a.get("description"): parts.append(str(a["description"]))
                elif a.get("unserializableValue"): parts.append(str(a["unserializableValue"]))
                else: parts.append("[%s]" % a.get("type"))
            line("[console.%s] %s" % (p.get("type"), " ".join(parts)))
        elif method == "Runtime.exceptionThrown":
            d = p.get("exceptionDetails") or {}
            text = (d.get("exception") or {}).get("description") or d.get("text")
            line("[exception] %s" % text)
        elif method == "Log.entryAdded":
            e = p.get("entry") or {}
            where = " (%s)" % e["url"] if e.get("url") else ""
            line("[log.%s] %s%s" % (e.get("level"), e.get("text"), where))
        elif method == "Network.requestWillBeSent":
            requests[p.get("requestId")] = {"url": (p.get("request") or {}).get("url"),
                                            "start": float(p.get("timestamp") or 0), "status": ""}
        elif method == "Network.responseReceived":
            r = requests.get(p.get("requestId"))
            if r is not None:
                r["status"] = str((p.get("response") or {}).get("status", ""))
        elif method == "Network.loadingFinished":
            r = requests.pop(p.get("requestId"), None)
            if r is not None:
                ms = int((float(p.get("timestamp") or 0) - r["start"]) * 1000)
                line("[net] %6d ms  %3s  %s" % (ms, r["status"], r["url"]))
        elif method == "Network.loadingFailed":
            r = requests.pop(p.get("requestId"), None)
            if r is not None:
                line("[net] FAILED      %s  %s" % (p.get("errorText"), r["url"]))

    # Requests that started but never finished are the smoking gun for the
    # single-threaded webserver stalling under load, so they are listed.
    for r in requests.values():
        line("[net] PENDING     %3s  %s  (started, never finished)" % (r["status"] or "no-response", r["url"]))
    line("stop requested; closing capture")
except Exception as exc:
    summary = "Browser capture: error - %s" % exc
    try:
        line("[worker-error] %s" % exc)
    except Exception:
        pass
finally:
    with open(status_path, "w") as f:
        f.write(summary)
PYEOF
}

find_browser() {
    local c
    if [ -n "$BROWSER_PATH" ]; then
        [ -x "$BROWSER_PATH" ] || { printf 'ERROR:Explicit -BrowserPath was not found: %s' "$BROWSER_PATH"; return; }
        printf '%s' "$BROWSER_PATH"; return
    fi
    for c in google-chrome google-chrome-stable chromium chromium-browser microsoft-edge microsoft-edge-stable brave-browser chrome; do
        if command -v "$c" >/dev/null 2>&1; then command -v "$c"; return; fi
    done
    for c in "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
             "/Applications/Chromium.app/Contents/MacOS/Chromium" \
             "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
             "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser" \
             "$HOME/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
        if [ -x "$c" ]; then printf '%s' "$c"; return; fi
    done
}

port_free() {
    # Nothing listening means a connect to it is refused at once.
    ! (exec 6<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null
}

select_debug_port() {
    local p="$1"
    while [ "$p" -lt $(( $1 + 12 )) ]; do
        if port_free "$p"; then printf '%s' "$p"; return; fi
        p=$(( p + 1 ))
    done
    printf '%s' "$1"
}

BROWSER_PID=""
CDP_PID=""
BROWSER_PROFILE=""
BROWSER_SANDBOX_NOTE=""

start_browser() {
    # $1 browser $2 port $3 extra flag (optional)
    "$1" --headless=new --disable-gpu --no-first-run --no-default-browser-check \
        --disable-extensions --disable-background-networking --mute-audio \
        --disable-logging --log-level=3 --remote-allow-origins=\* \
        --remote-debugging-port="$2" --user-data-dir="$BROWSER_PROFILE" ${3:+"$3"} \
        about:blank >/dev/null 2>>"$BROWSER_ERROR_LOG" &
    BROWSER_PID=$!
}

cdp_endpoint_up() { curl -s -m 1 -o /dev/null "http://127.0.0.1:$1/json/version"; }

start_browser_capture() {
    local browser="$1" port="$2" worker="$STATE_DIR/cdp_worker.py" i
    case "$browser" in
        /snap/*|*/snap/bin/*)
            # Snap-confined Chromium cannot write outside its own home area.
            BROWSER_PROFILE="$HOME/snap/chromium/common/otgw-capture-profile-$$" ;;
        *)  BROWSER_PROFILE="$(mktemp -d "${TMPDIR:-/tmp}/otgw-browser.XXXXXX")" ;;
    esac
    mkdir -p "$BROWSER_PROFILE"
    start_browser "$browser" "$port"
    # A browser that cannot set up its sandbox (Chrome for Testing on Ubuntu
    # 24.04's user-namespace restriction, containers) dies within a second.
    # Retry once without it, and record that in the summary.
    i=0
    while [ "$i" -lt 50 ] && ! cdp_endpoint_up "$port"; do
        if ! kill -0 "$BROWSER_PID" 2>/dev/null; then break; fi
        sleep 0.1; i=$(( i + 1 ))
    done
    if ! kill -0 "$BROWSER_PID" 2>/dev/null && grep -qi 'sandbox' "$BROWSER_ERROR_LOG" 2>/dev/null; then
        BROWSER_SANDBOX_NOTE="browser sandbox unavailable on this system, restarted with --no-sandbox"
        start_browser "$browser" "$port" "--no-sandbox"
    fi
    write_cdp_worker "$worker"
    python3 "$worker" "$port" "$BROWSER_URL" "$BROWSER_LOG" "$STOP_FLAG" "$STATE_DIR/browser.status" "$browser" \
        2>>"$BROWSER_ERROR_LOG" &
    CDP_PID=$!
}

stop_browser_capture() {
    local i=0
    if [ -n "$CDP_PID" ]; then
        while [ "$i" -lt 100 ] && kill -0 "$CDP_PID" 2>/dev/null; do sleep 0.1; i=$(( i + 1 )); done
        kill "$CDP_PID" 2>/dev/null
        wait "$CDP_PID" 2>/dev/null
    fi
    if [ -n "$BROWSER_PID" ]; then
        kill "$BROWSER_PID" 2>/dev/null
        i=0
        while [ "$i" -lt 30 ] && kill -0 "$BROWSER_PID" 2>/dev/null; do sleep 0.1; i=$(( i + 1 )); done
        kill -9 "$BROWSER_PID" 2>/dev/null
        wait "$BROWSER_PID" 2>/dev/null
    fi
    [ -n "$BROWSER_PROFILE" ] && rm -rf "$BROWSER_PROFILE"
    if [ -f "$STATE_DIR/browser.status" ]; then
        add_summary_line "$(cat "$STATE_DIR/browser.status")"
    else
        add_summary_line "Browser capture: stopped without a status (worker did not finish in time)"
    fi
}

# ===============================================================================
#  mosquitto_sub
# ===============================================================================
MOSQUITTO=""
MOSQUITTO_SOURCE=""
MOSQUITTO_INSTALLED="False"

find_mosquitto_sub() {
    local c
    if command -v mosquitto_sub >/dev/null 2>&1; then
        MOSQUITTO="$(command -v mosquitto_sub)"; MOSQUITTO_SOURCE="PATH"; return 0
    fi
    for c in /usr/bin/mosquitto_sub /usr/local/bin/mosquitto_sub /opt/homebrew/bin/mosquitto_sub \
             /opt/homebrew/opt/mosquitto/bin/mosquitto_sub /usr/local/opt/mosquitto/bin/mosquitto_sub \
             /snap/bin/mosquitto_sub /snap/bin/mosquitto.sub; do
        if [ -x "$c" ]; then MOSQUITTO="$c"; MOSQUITTO_SOURCE="common install path"; return 0; fi
    done
    return 1
}

# Install hint (Linux: needs sudo, so it is printed, not run) or a Homebrew
# install on macOS, which needs no sudo. Sets MOSQUITTO_ERROR on failure.
MOSQUITTO_ERROR=""
resolve_mosquitto_sub() {
    local hint
    if [ -n "$MOSQUITTO_SUB_PATH" ]; then
        MOSQUITTO="$MOSQUITTO_SUB_PATH"; MOSQUITTO_SOURCE="explicit"; return 0
    fi
    find_mosquitto_sub && return 0

    if [ "$(uname -s)" = "Darwin" ] && command -v brew >/dev/null 2>&1; then
        if [ "$SKIP_TOOL_INSTALL" = "1" ]; then
            MOSQUITTO_ERROR="mosquitto_sub was not found. Remove -SkipToolInstall, run 'brew install mosquitto', or pass -MosquittoSubPath."
            return 1
        fi
        add_summary_line "Mosquitto install: brew install started (mosquitto)"
        if brew install mosquitto >> "$SCRIPT_ERROR_LOG" 2>&1 && find_mosquitto_sub; then
            add_summary_line "Mosquitto install: brew install completed"
            MOSQUITTO_INSTALLED="True"
            return 0
        fi
        MOSQUITTO_ERROR="brew install mosquitto failed (see error.txt)."
        return 1
    fi

    if   command -v apt-get >/dev/null 2>&1; then hint="sudo apt install mosquitto-clients"
    elif command -v dnf     >/dev/null 2>&1; then hint="sudo dnf install mosquitto"
    elif command -v pacman  >/dev/null 2>&1; then hint="sudo pacman -S mosquitto"
    elif command -v zypper  >/dev/null 2>&1; then hint="sudo zypper install mosquitto-clients"
    elif command -v apk     >/dev/null 2>&1; then hint="sudo apk add mosquitto-clients"
    elif [ "$(uname -s)" = "Darwin" ];       then hint="install Homebrew (https://brew.sh), then: brew install mosquitto"
    else hint="install the Mosquitto clients from https://mosquitto.org/download/"; fi
    MOSQUITTO_ERROR="mosquitto_sub was not found. To capture MQTT too: $hint"
    return 1
}

# ===============================================================================
#  REST snapshot, metadata, transcript
# ===============================================================================
invoke_rest_snapshot() {
    # Runs once, AFTER the live capture has stopped, so it never adds load during
    # the measurement. /api/v2/settings is deliberately NOT captured: it carries
    # the MQTT broker credentials, and this transcript is uploaded publicly.
    local entry id name path
    {
        echo "OTGW REST snapshot - firmware state after the capture stopped"
        echo "Generated: $(iso_now)"
        echo "Device: http://$DEVICE_HOST"
        echo "Note: a message id the firmware has never seen reads 0; compare against the OT bus traffic above."
        echo ""
    } > "$REST_SNAPSHOT_LOG"
    for entry in "device info|/api/v2/device/info" "otmonitor|/api/v2/otgw/otmonitor" "boiler support|/api/v2/otgw/boiler-support" \
                 1:TSet 9:TrOverride 14:MaxRelModLevelSetting 16:TrSet 17:RelModLevel 18:CHPressure 24:Tr 25:Tboiler \
                 26:Tdhw 28:Tret 48:TdhwSetUBTdhwSetLB 56:TdhwSet 57:MaxTSet; do
        case "$entry" in
            *'|'*) name="${entry%%|*}"; path="${entry#*|}" ;;
            *)     id="${entry%%:*}"; name="msgid $id ${entry#*:}"; path="/api/v2/otgw/messages/$id" ;;
        esac
        printf -- '--- %s  [%s]\n' "$name" "$path" >> "$REST_SNAPSHOT_LOG"
        http_get "http://$DEVICE_HOST$path" 5
        if [ -n "$HTTP_STATUS" ]; then
            {
                printf 'HTTP %s\n' "$HTTP_STATUS"
                printf '%s\n' "$HTTP_BODY" | tr -d '\r' | sed -e 's/[[:space:]]*$//'
            } >> "$REST_SNAPSHOT_LOG"
        else
            printf 'ERROR: %s\n' "$HTTP_ERROR" >> "$REST_SNAPSHOT_LOG"
        fi
        echo "" >> "$REST_SNAPSHOT_LOG"
    done
}

META_HOSTNAME=""; META_VERSION=""; META_UNIQUE_ID=""; META_PIC_ID=""
META_SOURCES=""; META_ERRORS=0

usable() {
    local v
    v="$(printf '%s' "$1" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
    case "$(printf '%s' "$v" | tr '[:upper:]' '[:lower:]')" in
        ""|null|unknown|"(not set)") return 1 ;;
    esac
    return 0
}

# set_meta VAR VALUE SOURCE [prefer-existing]
set_meta() {
    usable "$2" || return 0
    if [ "${4:-}" = "prefer" ] && usable "$(eval "printf '%s' \"\$$1\"")"; then return 0; fi
    eval "$1=\$2"
    case "$1" in
        META_HOSTNAME)  META_SOURCES="$META_SOURCES, Hostname=$3" ;;
        META_VERSION)   META_SOURCES="$META_SOURCES, FirmwareVersion=$3" ;;
        META_UNIQUE_ID) META_SOURCES="$META_SOURCES, UniqueId=$3" ;;
        META_PIC_ID)    META_SOURCES="$META_SOURCES, PicDeviceId=$3" ;;
    esac
}

resolve_capture_metadata() {
    local body mac
    if [ -f "$TELNET_LOG" ]; then
        set_meta META_HOSTNAME "$(tr -d '\r' < "$TELNET_LOG" | sed -n 's/^[[:space:]]*hostname:[[:space:]]*\(.*[^[:space:]]\)[[:space:]]*$/\1/p' | head -n 1)" telnet.log prefer
        set_meta META_VERSION  "$(tr -d '\r' < "$TELNET_LOG" | sed -n 's/^[[:space:]]*version:[[:space:]]*\(.*[^[:space:]]\)[[:space:]]*$/\1/p' | head -n 1)" telnet.log prefer
        set_meta META_UNIQUE_ID "$(tr -d '\r' < "$TELNET_LOG" | sed -n 's/^[[:space:]]*unique_id:[[:space:]]*\(.*[^[:space:]]\)[[:space:]]*$/\1/p' | tail -n 1)" telnet.log
        set_meta META_PIC_ID   "$(tr -d '\r' < "$TELNET_LOG" | sed -n 's/^[[:space:]]*device_id:[[:space:]]*\(.*[^[:space:]]\)[[:space:]]*$/\1/p' | tail -n 1)" telnet.log
    fi
    if usable "$META_HOSTNAME" && usable "$META_VERSION" && usable "$META_UNIQUE_ID"; then return 0; fi
    [ -n "$DEVICE_HOST" ] || return 0

    http_get "http://$DEVICE_HOST/api/v2/debug" 2
    if [ "$HTTP_STATUS" = "200" ]; then
        set_meta META_HOSTNAME  "$(printf '%s' "$HTTP_BODY" | json_str settings.hostname)" /api/v2/debug prefer
        set_meta META_VERSION   "$(printf '%s' "$HTTP_BODY" | json_str build.version)" /api/v2/debug prefer
        set_meta META_UNIQUE_ID "$(printf '%s' "$HTTP_BODY" | json_str settings.mqtt.unique_id)" /api/v2/debug
    else
        add_tool_error_line "metadata /api/v2/debug unavailable: ${HTTP_ERROR:-HTTP $HTTP_STATUS}"; META_ERRORS=$(( META_ERRORS + 1 ))
    fi

    if ! usable "$META_UNIQUE_ID"; then
        http_get "http://$DEVICE_HOST/api/v2/settings" 2
        if [ "$HTTP_STATUS" = "200" ]; then
            # Only two fields are read; the body is not kept anywhere.
            body="$(printf '%s' "$HTTP_BODY" | tr -d '\n')"
            set_meta META_HOSTNAME  "$(printf '%s' "$body" | sed -n 's/.*"hostname"[[:space:]]*:[[:space:]]*{[^}]*"value"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')" /api/v2/settings prefer
            set_meta META_UNIQUE_ID "$(printf '%s' "$body" | sed -n 's/.*"mqttuniqueid"[[:space:]]*:[[:space:]]*{[^}]*"value"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')" /api/v2/settings
            body=""
        else
            add_tool_error_line "metadata /api/v2/settings unavailable: ${HTTP_ERROR:-HTTP $HTTP_STATUS}"; META_ERRORS=$(( META_ERRORS + 1 ))
        fi
    fi

    http_get "http://$DEVICE_HOST/api/v2/device/info" 2
    if [ "$HTTP_STATUS" = "200" ]; then
        set_meta META_HOSTNAME "$(printf '%s' "$HTTP_BODY" | json_str hostname)" /api/v2/device/info prefer
        set_meta META_VERSION  "$(printf '%s' "$HTTP_BODY" | json_str fwversion)" /api/v2/device/info prefer
        if ! usable "$META_UNIQUE_ID"; then
            mac="$(printf '%s' "$HTTP_BODY" | json_str macaddress | tr -cd '0-9A-Fa-f' | tr 'a-f' 'A-F')"
            [ -n "$mac" ] && set_meta META_UNIQUE_ID "otgw-$mac" "/api/v2/device/info macaddress"
        fi
    else
        add_tool_error_line "metadata /api/v2/device/info unavailable: ${HTTP_ERROR:-HTTP $HTTP_STATUS}"; META_ERRORS=$(( META_ERRORS + 1 ))
    fi
}

safe_name_part() {
    # $1 value $2 fallback: same rules as ConvertTo-SafeFileNamePart.
    local part="$1"
    usable "$part" || part="$2"
    part="$(printf '%s' "$part" | tr '/\\:*?"<>|' '---------' | tr -s '[:space:]' '-' | tr -s '-' | sed 's/^[.-]*//; s/[.-]*$//' | cut -c1-80 | sed 's/[.-]*$//')"
    [ -n "$part" ] || part="$2"
    printf '%s' "$part"
}

append_section() {
    # $1 output $2 title $3 file $4 empty-text (optional)
    {
        echo "============================================================"
        echo "=== $2"
        echo "============================================================"
        if [ -f "$3" ]; then
            if [ -n "${4:-}" ] && [ -z "$(tr -d '[:space:]' < "$3")" ]; then
                echo "$4"
            else
                cat "$3"
                [ -n "$(tail -c 1 "$3")" ] && echo ""
            fi
        else
            echo "(not present)"
        fi
        echo ""
    } >> "$1"
}

new_tool_error_log() {
    {
        echo "OTGW capture - tool stderr and script errors"
        echo "Generated: $(iso_now)"
        echo "Run folder: $RUN_PATH"
        echo ""
    } > "$TOOL_ERROR_LOG"
    append_section "$TOOL_ERROR_LOG" "SCRIPT / CAPTURE ERRORS (script.error.log)" "$SCRIPT_ERROR_LOG" "(empty)"
    append_section "$TOOL_ERROR_LOG" "MQTT STDERR (mqtt.stderr.log)" "$MQTT_ERROR_LOG" "(empty)"
    append_section "$TOOL_ERROR_LOG" "BROWSER STDERR (browser.stderr.log)" "$BROWSER_ERROR_LOG" "(empty)"
}

new_merged_transcript() {
    # ONE file for the tester to upload: summary first, then the raw streams.
    local out="$1"
    {
        echo "OTGW capture - merged transcript"
        echo "Generated: $(iso_now)"
        echo "Run folder: $RUN_PATH"
        echo "Upload THIS single file; it contains every capture log below."
        echo ""
    } > "$out"
    append_section "$out" "SUMMARY (summary.txt)" "$SUMMARY_PATH"
    append_section "$out" "TOOL ERRORS (error.txt)" "$TOOL_ERROR_LOG"
    if [ "$SERIAL_MODE" = "1" ]; then
        append_section "$out" "OTGW USB SERIAL (usb-serial.log)" "$SERIAL_LOG"
    fi
    append_section "$out" "OTGW TELNET DEBUG (telnet.log)" "$TELNET_LOG"
    append_section "$out" "MQTT BROKER STREAM (mqtt.log)" "$MQTT_LOG"
    append_section "$out" "BROWSER DEVTOOLS (browser.log)" "$BROWSER_LOG"
    append_section "$out" "DEVICE CRASH LOG (crashlog.log)" "$CRASHLOG_LOG"
    [ -f "$PROBE_LOG" ] && append_section "$out" "REACHABILITY PROBE (probe.log)" "$PROBE_LOG"
    append_section "$out" "REST SNAPSHOT (rest-snapshot.log)" "$REST_SNAPSHOT_LOG"
}

# ===============================================================================
#  Sources start. Order and degrade rules as in the Windows script: browser,
#  crash log, then MQTT; none of their failures stops the telnet capture.
# ===============================================================================
MQTT_PID=""
CRASHLOG_PID=""
PROBE_PID=""
SERIAL_PID=""
STOP_BROWSER="0"

if [ -z "$DEVICE_HOST" ]; then
    add_summary_line "Browser capture: skipped - no DeviceHost (serial-only run)."
elif [ "$SKIP_BROWSER_CAPTURE" = "1" ]; then
    add_summary_line "Browser capture: disabled (-SkipBrowserCapture)."
elif ! command -v python3 >/dev/null 2>&1; then
    add_summary_line "Browser capture: skipped - python3 not found (it runs the CDP client; pass -SkipBrowserCapture to silence this)."
elif [ "$(uname -s)" = "Darwin" ] && [ "$(command -v python3)" = "/usr/bin/python3" ] && ! xcode-select -p >/dev/null 2>&1; then
    # Without the Command Line Tools, /usr/bin/python3 is a stub that pops up an
    # installer dialog instead of running anything.
    add_summary_line "Browser capture: skipped - python3 is the macOS stub; install it with 'xcode-select --install' (or pass -SkipBrowserCapture)."
else
    RESOLVED_BROWSER="$(find_browser)"
    case "$RESOLVED_BROWSER" in
        "")
            if grep -qi microsoft /proc/version 2>/dev/null; then
                add_summary_line "Browser capture: skipped - no Linux Chrome/Chromium/Edge/Brave found in this WSL distribution (a Windows browser cannot be driven from WSL; pass -BrowserPath or -SkipBrowserCapture)."
            else
                add_summary_line "Browser capture: skipped - no Chrome/Chromium/Edge/Brave found (pass -BrowserPath or -SkipBrowserCapture)."
            fi ;;
        ERROR:*)
            add_summary_line "Browser capture: failed to start - ${RESOLVED_BROWSER#ERROR:}" ;;
        *)
            [ -n "$BROWSER_URL" ] || BROWSER_URL="http://$DEVICE_HOST/"
            CHOSEN_PORT="$(select_debug_port "$BROWSER_DEBUG_PORT")"
            add_summary_line "Browser capture: $RESOLVED_BROWSER"
            add_summary_line "Browser url: $BROWSER_URL"
            add_summary_line "Browser CDP port: $CHOSEN_PORT"
            start_browser_capture "$RESOLVED_BROWSER" "$CHOSEN_PORT"
            STOP_BROWSER="1"
            [ -n "$BROWSER_SANDBOX_NOTE" ] && add_summary_line "Browser capture: $BROWSER_SANDBOX_NOTE"
            add_summary_line "Browser capture started (headless, writing browser.log; tool stderr captured for error.txt)." ;;
    esac
fi

if [ -z "$DEVICE_HOST" ]; then
    add_summary_line "Crash-log capture: skipped - no DeviceHost (serial-only run)."
elif [ "$SKIP_CRASHLOG_CAPTURE" = "1" ]; then
    add_summary_line "Crash-log capture: disabled (-SkipCrashlogCapture)."
else
    [ -n "$CRASHLOG_URL" ] || CRASHLOG_URL="http://$DEVICE_HOST/api/v2/device/crashlog"
    REBOOT_LOG_URL="http://$DEVICE_HOST/reboot_log.txt"
    add_summary_line "Crash-log endpoint: $CRASHLOG_URL"
    add_summary_line "Crash-log reboot-log file: $REBOOT_LOG_URL"
    add_summary_line "Crash-log poll interval: ${CRASHLOG_POLL_SECONDS}s"
    : > "$CRASHLOG_LOG"
    crashlog_worker "$REBOOT_LOG_URL" 2>>"$SCRIPT_ERROR_LOG" &
    CRASHLOG_PID=$!
    add_summary_line "Crash-log capture started (polling, writing crashlog.log)."
fi

if [ "$PROBE_SECONDS" -gt 0 ] && [ -n "$DEVICE_HOST" ]; then
    probe_worker 2>>"$SCRIPT_ERROR_LOG" &
    PROBE_PID=$!
    add_summary_line "Reachability probe: every ${PROBE_SECONDS}s into probe.log"
fi

# The MQTT stream is a bonus; the telnet log is the reason this script exists.
# A missing mosquitto_sub degrades the run, it never aborts it.
if [ -z "$BROKER_HOST" ]; then
    add_summary_line "MQTT capture disabled: no BrokerHost given."
elif resolve_mosquitto_sub; then
    add_summary_line "mosquitto_sub: $MOSQUITTO"
    add_summary_line "mosquitto_sub source: $MOSQUITTO_SOURCE"
    add_summary_line "Mosquitto installed by script: $MOSQUITTO_INSTALLED"
    set -- -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$TOPIC" -v
    [ -n "$USERNAME" ] && set -- "$@" -u "$USERNAME"
    [ -n "$USERNAME" ] && [ -n "$PASSWORD" ] && set -- "$@" -P "$PASSWORD"
    "$MOSQUITTO" "$@" > "$MQTT_LOG" 2> "$MQTT_ERROR_LOG" &
    MQTT_PID=$!
    set --
    add_summary_line "mosquitto_sub started: pid $MQTT_PID"
else
    add_summary_line "MQTT capture disabled: $MOSQUITTO_ERROR"
    add_tool_error_line "MQTT capture disabled: $MOSQUITTO_ERROR"
    warn ""
    warn "WARNING: mosquitto_sub is unavailable, so the MQTT broker stream will NOT be captured."
    warn "  Reason: $MOSQUITTO_ERROR"
    warn "  The telnet capture continues and is usually enough to diagnose a problem."
    warn "  Or point the script at an existing copy: -MosquittoSubPath /path/to/mosquitto_sub"
    warn ""
fi

# ----------------------------------------------------------------- serial mode ---
if [ "$SERIAL_MODE" = "1" ]; then
    if [ -z "$SERIAL_DEV" ]; then
        # cu.* rather than tty.* on macOS: opening tty.* blocks until carrier detect.
        for cand in /dev/ttyUSB* /dev/ttyACM* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
            if [ -e "$cand" ]; then SERIAL_DEV="$cand"; break; fi
        done
    fi
    {
        echo "# OTGW USB serial capture"
        echo "# device  : ${SERIAL_DEV:-none found}"
        echo "# baud    : $SERIAL_BAUD 8N1 (9600 is what the firmware configures, OTGWSerial.cpp:836)"
        echo "# started : $(iso_now)"
    } > "$SERIAL_LOG"
    if [ -z "$SERIAL_DEV" ] || [ ! -e "$SERIAL_DEV" ]; then
        write_capture_status "Serial capture: FAILED - no serial device found${SERIAL_DEV:+ ($SERIAL_DEV does not exist)}. Plug the gateway in over USB, or pass -Serial /dev/ttyUSB0."
    else
        # stty takes -F on Linux and -f on BSD/macOS. clocal: do not wait for
        # carrier detect. -hupcl: do not drop DTR on close, which on many
        # adapters resets the ESP being diagnosed.
        STTY_FLAG="-F"
        stty -F "$SERIAL_DEV" -a >/dev/null 2>&1 || STTY_FLAG="-f"
        if stty "$STTY_FLAG" "$SERIAL_DEV" "$SERIAL_BAUD" cs8 -cstopb -parenb raw -echo clocal -hupcl 2>>"$SCRIPT_ERROR_LOG"; then
            cat "$SERIAL_DEV" >> "$SERIAL_LOG" 2>>"$SCRIPT_ERROR_LOG" &
            SERIAL_PID=$!
            write_capture_status "Serial capture: $SERIAL_DEV at $SERIAL_BAUD 8N1, read only"
        else
            write_capture_status "Serial capture: FAILED - stty could not configure $SERIAL_DEV (on Linux, join the dialout group)"
        fi
    fi
fi

# ===============================================================================
#  Capture loop
# ===============================================================================
if [ "$DURATION_SECONDS" -gt 0 ]; then
    DEADLINE_MS=$(( $(now_ms) + DURATION_SECONDS * 1000 ))
else
    DEADLINE_MS=0
fi

add_summary_line "Capture started: $(iso_now)"
if [ -n "$MQTT_PID" ]; then printf 'Capturing telnet and MQTT output in %s\n' "$RUN_PATH"
else printf 'Capturing telnet output (MQTT disabled) in %s\n' "$RUN_PATH"; fi
if [ "$HAVE_TTY" = "1" ]; then
    printf 'Press Q to stop cleanly and leave a transcript timestamp-version-host-uniqueid file. Run with --help for options.\n'
fi
printf 'Ctrl+C also stops the capture cleanly.\n'

: > "$TELNET_LOG"
TELNET_HAS_CONNECTED="0"
TELNET_RECONNECT_ATTEMPTS=0
TELNET_RECONNECT_REASON=""
NEXT_CONNECT_MS=0
SCAN_OFFSET=0
DO_TELNET="1"
[ -n "$DEVICE_HOST" ] || DO_TELNET="0"
[ "$SERIAL_MODE" = "1" ] && DO_TELNET="0"
LOOP_END_REASON="capture loop ended"

while :; do
    poll_console
    if stop_requested; then break; fi
    if [ "$DEADLINE_MS" -gt 0 ] && [ "$(now_ms)" -ge "$DEADLINE_MS" ]; then LOOP_END_REASON="duration elapsed"; break; fi
    if [ -n "$MQTT_PID" ] && ! kill -0 "$MQTT_PID" 2>/dev/null; then
        wait "$MQTT_PID"; MQTT_RC=$?
        add_summary_line "mosquitto_sub exited during capture with code $MQTT_RC."
        MQTT_PID=""
        LOOP_END_REASON="mosquitto_sub exited"
        break
    fi
    [ "$DO_TELNET" = "1" ] || continue

    if ! telnet_alive; then
        if [ -n "$TELNET_PID" ]; then
            close_telnet
            TELNET_RECONNECT_REASON="connection closed"
            TELNET_RECONNECT_ATTEMPTS=0
            NEXT_CONNECT_MS=$(( $(now_ms) + TELNET_POST_DISCONNECT_DELAY_MS ))
            write_capture_status "Telnet disconnected; waiting $(awk -v m="$TELNET_POST_DISCONNECT_DELAY_MS" 'BEGIN{print m/1000}')s before reconnect attempts."
        fi
        [ "$(now_ms)" -lt "$NEXT_CONNECT_MS" ] && continue

        TELNET_RECONNECT_ATTEMPTS=$(( TELNET_RECONNECT_ATTEMPTS + 1 ))
        EFFECTIVE_TIMEOUT="$(effective_connect_timeout "$TELNET_RECONNECT_ATTEMPTS")"
        if [ "$TELNET_HAS_CONNECTED" = "1" ]; then
            add_summary_line "Telnet reconnect attempt #$TELNET_RECONNECT_ATTEMPTS started${TELNET_RECONNECT_REASON:+ after $TELNET_RECONNECT_REASON} (timeout ${EFFECTIVE_TIMEOUT}s)."
        fi
        if connect_telnet "$EFFECTIVE_TIMEOUT"; then
            if [ "$TELNET_HAS_CONNECTED" = "1" ]; then
                write_capture_status "Telnet reconnected after $TELNET_RECONNECT_ATTEMPTS attempt(s): $(iso_now)"
            else
                write_capture_status "Telnet connected: $(iso_now)"
            fi
            TELNET_HAS_CONNECTED="1"
            TELNET_RECONNECT_ATTEMPTS=0
            TELNET_RECONNECT_REASON=""
            SCAN_OFFSET="$CONNECT_OFFSET"
        else
            stop_requested && break
            NEXT_CONNECT_MS=$(( $(now_ms) + TELNET_RECONNECT_DELAY_MS ))
            write_capture_status "Telnet connect attempt #$TELNET_RECONNECT_ATTEMPTS failed (timeout ${EFFECTIVE_TIMEOUT}s): $TELNET_ERROR Retrying in $(awk -v m="$TELNET_RECONNECT_DELAY_MS" 'BEGIN{print m/1000}')s."
        fi
        continue
    fi

    # Reboot marker: the device prints ESP.restart() before it goes, and the
    # socket may linger half-open afterwards, so reconnect on the marker rather
    # than wait for a close that may never arrive. Scan with 512 bytes overlap.
    SIZE_NOW="$(log_size "$TELNET_LOG")"
    if [ "$SIZE_NOW" -gt "$SCAN_OFFSET" ]; then
        FROM=$(( SCAN_OFFSET > 512 ? SCAN_OFFSET - 512 : 0 ))
        [ "$FROM" -lt "$CONNECT_OFFSET" ] && FROM="$CONNECT_OFFSET"
        if tail -c +"$(( FROM + 1 ))" "$TELNET_LOG" 2>/dev/null | head -c $(( SIZE_NOW - FROM )) | grep -q 'ESP\.restart()'; then
            close_telnet
            TELNET_RECONNECT_REASON="ESP.restart() marker captured"
            TELNET_RECONNECT_ATTEMPTS=0
            NEXT_CONNECT_MS=$(( $(now_ms) + TELNET_POST_DISCONNECT_DELAY_MS ))
            write_capture_status "Telnet captured ESP.restart(); waiting $(awk -v m="$TELNET_POST_DISCONNECT_DELAY_MS" 'BEGIN{print m/1000}')s before reconnect attempts."
        fi
        SCAN_OFFSET="$SIZE_NOW"
    fi
done

if stop_requested; then
    add_summary_line "Capture stop reason: ${CAPTURE_STOP_REASON:-Ctrl+C}"
else
    add_summary_line "Capture stop reason: $LOOP_END_REASON"
fi

# ===============================================================================
#  Teardown
# ===============================================================================
trap '' INT    # a second Ctrl+C must not cut the transcript short
add_summary_line "Stopping: $(iso_now)"
request_capture_stop "capture shutdown"

if [ -n "$MQTT_PID" ]; then
    if kill -0 "$MQTT_PID" 2>/dev/null; then
        kill "$MQTT_PID" 2>/dev/null; wait "$MQTT_PID" 2>/dev/null
        add_summary_line "mosquitto_sub stopped by script."
    else
        wait "$MQTT_PID"; add_summary_line "mosquitto_sub exit code: $?"
    fi
fi

# Restore toggles BEFORE the connection goes away.
if [ -n "$TOGGLED_KEYS" ]; then
    restore_debug_toggles
    sleep 0.3
fi
close_telnet

if [ -n "$SERIAL_PID" ]; then kill "$SERIAL_PID" 2>/dev/null; wait "$SERIAL_PID" 2>/dev/null; fi
[ "$STOP_BROWSER" = "1" ] && stop_browser_capture
if [ -n "$CRASHLOG_PID" ]; then
    WAITED=0
    while [ "$WAITED" -lt 100 ] && kill -0 "$CRASHLOG_PID" 2>/dev/null; do sleep 0.1; WAITED=$(( WAITED + 1 )); done
    kill "$CRASHLOG_PID" 2>/dev/null; wait "$CRASHLOG_PID" 2>/dev/null
    if [ -f "$STATE_DIR/crashlog.status" ]; then add_summary_line "$(cat "$STATE_DIR/crashlog.status")"
    else add_summary_line "Crash-log capture: stop error - worker did not finish within 10s"; fi
fi
if [ -n "$PROBE_PID" ]; then kill "$PROBE_PID" 2>/dev/null; wait "$PROBE_PID" 2>/dev/null; fi

# Snapshot the firmware's own view now that the live capture has stopped.
if [ -z "$DEVICE_HOST" ]; then
    add_summary_line "REST snapshot: skipped - no DeviceHost (serial-only run)."
elif [ "$SKIP_REST_SNAPSHOT" = "1" ]; then
    add_summary_line "REST snapshot: disabled (-SkipRestSnapshot)."
else
    printf 'Reading REST snapshot from the device -> rest-snapshot.log\n'
    invoke_rest_snapshot
    add_summary_line "REST snapshot: written to rest-snapshot.log"
fi

resolve_capture_metadata
if usable "$META_HOSTNAME"; then R_HOST="$META_HOSTNAME"; else R_HOST="${DEVICE_HOST:-${SERIAL_DEV##*/}}"; fi
if usable "$META_VERSION"; then R_VERSION="$META_VERSION"; else R_VERSION="unknown-version"; fi
if usable "$META_UNIQUE_ID"; then R_ID="$META_UNIQUE_ID"; elif usable "$META_PIC_ID"; then R_ID="$META_PIC_ID"; else R_ID="unknown-id"; fi
add_summary_line "ResolvedHostname: $R_HOST"
add_summary_line "ResolvedFirmwareVersion: $R_VERSION"
if usable "$META_UNIQUE_ID"; then add_summary_line "ResolvedUniqueId: $META_UNIQUE_ID"; else add_summary_line "ResolvedUniqueId: (unknown)"; fi
if usable "$META_PIC_ID"; then add_summary_line "ResolvedPicDeviceId: $META_PIC_ID"; else add_summary_line "ResolvedPicDeviceId: (unknown)"; fi
add_summary_line "ResolvedDeviceIdForFilename: $R_ID"
[ -n "$META_SOURCES" ] && add_summary_line "Metadata sources: ${META_SOURCES#, }"
[ "$META_ERRORS" -gt 0 ] && add_summary_line "Metadata lookup warnings: $META_ERRORS (see error.txt)"
add_summary_line "Finished: $(iso_now)"

new_tool_error_log
TRANSCRIPT_PATH="$RUN_PATH/transcript-$RUN_NAME-$(safe_name_part "$R_VERSION" unknown-version)-$(safe_name_part "$R_HOST" "${DEVICE_HOST:-serial}")-$(safe_name_part "$R_ID" unknown-id).txt"
add_summary_line "Transcript filename: $(basename "$TRANSCRIPT_PATH")"
new_merged_transcript "$TRANSCRIPT_PATH"

REMOVED=""
for f in summary.txt telnet.log usb-serial.log mqtt.log mqtt.stderr.log browser.log browser.stderr.log crashlog.log probe.log script.error.log rest-snapshot.log; do
    if [ -f "$RUN_PATH/$f" ]; then rm -f "$RUN_PATH/$f"; REMOVED="$REMOVED, $f"; fi
done
rm -rf "$STATE_DIR"

printf 'Merged transcript (single upload): %s\n' "$TRANSCRIPT_PATH"
printf 'Tool errors captured in: %s\n' "$TOOL_ERROR_LOG"
[ -n "$REMOVED" ] && printf 'Removed intermediate capture files: %s\n' "${REMOVED#, }"
printf 'Diagnostic capture folder: %s\n' "$RUN_PATH"
exit 0
