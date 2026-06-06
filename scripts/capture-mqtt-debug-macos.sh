#!/usr/bin/env bash
# =============================================================================
# capture-mqtt-debug-macos.sh
# OTGW MQTT + telnet diagnostic capture for macOS/Linux
#
# Single-file delivery. Bash handles prompts, process supervision, mosquitto_sub,
# cleanup, and transcript merging. An embedded Python worker handles reconnecting
# telnet capture.
# =============================================================================

set -u

DEVICE_HOST=""
BROKER_HOST=""
BROKER_PORT="1883"
TOPIC="#"
USERNAME=""
PASSWORD=""
OUTPUT_ROOT="logs/mqtt-diagnostics"
DURATION_SECONDS=""
MOSQUITTO_SUB_PATH=""
SKIP_TOOL_INSTALL=0
TELNET_CONNECT_TIMEOUT_SECONDS=6
TELNET_RECONNECT_DELAY_MILLISECONDS=500
TELNET_POST_DISCONNECT_DELAY_MILLISECONDS=1000
NO_PROMPT=0

RUN_PATH=""
SUMMARY_LOG=""
TELNET_LOG=""
MQTT_LOG=""
MQTT_ERR_LOG=""
TRANSCRIPT=""
STOP_FILE=""
MQTT_PID=""
TELNET_PID=""
CLEANED_UP=0
CAPTURE_STOP_REASON=""

show_help() {
  cat <<EOF
OTGW MQTT diagnostic capture for macOS/Linux

Usage:
  ./capture-mqtt-debug-macos.sh [options]
  ./capture-mqtt-debug-macos.sh --help

Core options:
  --device <host>                         OTGW device IP/hostname
  --broker <host>                         MQTT broker IP/hostname
  --broker-port <port>                    MQTT broker port, default 1883
  --topic <topic>                         MQTT topic, default "#"
  --user <username>                       MQTT username
  --password <password>                   MQTT password
  --duration <seconds>                    Stop automatically after fixed duration
  --output-root <path>                    Output root, default logs/mqtt-diagnostics
  --mosquitto-sub-path <path>             Explicit mosquitto_sub executable
  --skip-tool-install                     Do not try to install Mosquitto client
  --no-prompt                             Fail instead of prompting for missing values

Telnet options:
  --telnet-connect-timeout-seconds <n>    Base connect timeout, default 6, range 2-60
  --telnet-reconnect-delay-ms <n>         Delay after failed connect, default 500
  --telnet-post-disconnect-delay-ms <n>   Delay after disconnect/reboot marker, default 1000

Stop:
  Press Q to stop cleanly and create transcript.txt.
  Ctrl+C also stops cleanly.

Aliases:
  Windows-style aliases are accepted, for example -DeviceHost, -BrokerHost,
  -DurationSeconds, and -MosquittoSubPath.

Security:
  Supplying --password can expose the password in the process list while the
  script starts. Prefer entering the password at the prompt when possible.
EOF
}

fail() {
  echo "Error: $1" >&2
  exit "${2:-1}"
}

timestamp() {
  date '+%Y-%m-%dT%H:%M:%S%z'
}

require_option_value() {
  local option="$1"
  local value="${2-}"

  if [[ -z "$value" || "$value" == --* ]]; then
    fail "$option requires a value." 2
  fi
}

is_positive_integer() {
  case "$1" in
    ''|*[!0-9]*|0) return 1 ;;
    *) return 0 ;;
  esac
}

validate_range() {
  local name="$1"
  local value="$2"
  local min="$3"
  local max="$4"

  if ! is_positive_integer "$value" || (( value < min || value > max )); then
    fail "$name must be an integer from $min to $max." 2
  fi
}

write_summary() {
  local line="$1"
  echo "$line" | tee -a "$SUMMARY_LOG"
}

find_python3() {
  if command -v python3 >/dev/null 2>&1; then
    command -v python3
    return 0
  fi

  fail "python3 is required for telnet capture on macOS/Linux."
}

find_mosquitto_sub() {
  if [[ -n "$MOSQUITTO_SUB_PATH" ]]; then
    if [[ -x "$MOSQUITTO_SUB_PATH" ]]; then
      printf '%s\n' "$MOSQUITTO_SUB_PATH"
      return 0
    fi
    fail "Explicit --mosquitto-sub-path was not found or is not executable: $MOSQUITTO_SUB_PATH"
  fi

  if command -v mosquitto_sub >/dev/null 2>&1; then
    command -v mosquitto_sub
    return 0
  fi

  for path in \
    /opt/homebrew/bin/mosquitto_sub \
    /usr/local/bin/mosquitto_sub \
    /usr/bin/mosquitto_sub; do
    if [[ -x "$path" ]]; then
      printf '%s\n' "$path"
      return 0
    fi
  done

  return 1
}

ensure_mosquitto_sub() {
  local found

  if found="$(find_mosquitto_sub 2>/dev/null)"; then
    printf '%s\n' "$found"
    return 0
  fi

  if [[ "$SKIP_TOOL_INSTALL" -eq 1 ]]; then
    fail "mosquitto_sub was not found. Remove --skip-tool-install, install Mosquitto, or pass --mosquitto-sub-path."
  fi

  if command -v brew >/dev/null 2>&1; then
    write_summary "Mosquitto install: brew install started"
    if ! brew install mosquitto; then
      fail "Homebrew failed to install Mosquitto."
    fi
    write_summary "Mosquitto install: brew install completed"
  else
    echo "mosquitto_sub not found. Install Mosquitto first:" >&2
    echo "  brew install mosquitto" >&2
    echo "Or pass --mosquitto-sub-path <path>." >&2
    exit 1
  fi

  if found="$(find_mosquitto_sub 2>/dev/null)"; then
    printf '%s\n' "$found"
    return 0
  fi

  fail "Mosquitto installation completed, but mosquitto_sub still could not be found."
}

merge_transcript() {
  {
    echo "OTGW capture - merged transcript"
    echo "Generated: $(timestamp)"
    echo "Run folder: $RUN_PATH"
    echo "Upload THIS single file; it contains every capture log below."
    echo

    for item in \
      "SUMMARY (summary.txt)|$SUMMARY_LOG" \
      "OTGW TELNET DEBUG (telnet.log)|$TELNET_LOG" \
      "MQTT BROKER STREAM (mqtt.log)|$MQTT_LOG" \
      "MQTT STDERR (mqtt.stderr.log)|$MQTT_ERR_LOG"; do
      local title="${item%%|*}"
      local file="${item#*|}"
      echo "============================================================"
      echo "=== $title"
      echo "============================================================"
      if [[ -f "$file" ]]; then
        cat "$file" 2>/dev/null || echo "(could not read)"
      else
        echo "(not present)"
      fi
      echo
    done
  } > "$TRANSCRIPT"
}

stop_child() {
  local name="$1"
  local pid="$2"
  local grace_ticks="${3:-0}"
  local waited=0

  if [[ -z "$pid" ]]; then
    return
  fi

  if ! kill -0 "$pid" >/dev/null 2>&1; then
    wait "$pid" 2>/dev/null
    write_summary "$name exit code: $?"
    return
  fi

  while kill -0 "$pid" >/dev/null 2>&1 && (( waited < grace_ticks )); do
    sleep 0.1
    waited=$((waited + 1))
  done

  if ! kill -0 "$pid" >/dev/null 2>&1; then
    wait "$pid" 2>/dev/null
    write_summary "$name exit code: $?"
    return
  fi

  waited=0
  kill "$pid" >/dev/null 2>&1 || true
  while kill -0 "$pid" >/dev/null 2>&1 && (( waited < 20 )); do
    sleep 0.1
    waited=$((waited + 1))
  done

  if kill -0 "$pid" >/dev/null 2>&1; then
    kill -KILL "$pid" >/dev/null 2>&1 || true
  fi

  wait "$pid" 2>/dev/null || true
  write_summary "$name stopped by script."
}

cleanup() {
  if [[ "$CLEANED_UP" -eq 1 ]]; then
    return
  fi
  CLEANED_UP=1

  if [[ -n "$STOP_FILE" ]]; then
    : > "$STOP_FILE"
  fi

  echo
  write_summary "Capture stop reason: ${CAPTURE_STOP_REASON:-capture shutdown}"
  write_summary "Stopping: $(timestamp)"

  stop_child "mosquitto_sub" "${MQTT_PID:-}"
  stop_child "Telnet capture worker" "${TELNET_PID:-}" 20

  write_summary "Finished: $(timestamp)"

  merge_transcript

  for file in "$SUMMARY_LOG" "$TELNET_LOG" "$MQTT_LOG" "$MQTT_ERR_LOG"; do
    if [[ -f "$file" ]]; then
      rm -f "$file"
    fi
  done

  if [[ -n "$STOP_FILE" && -f "$STOP_FILE" ]]; then
    rm -f "$STOP_FILE"
  fi

  echo "Merged transcript (single upload): $TRANSCRIPT"
  echo "Diagnostic capture folder: $RUN_PATH"
}

on_signal() {
  CAPTURE_STOP_REASON="Ctrl+C/Ctrl+Break"
  cleanup
  exit 130
}

start_telnet_capture() {
  local python="$1"

  "$python" - "$DEVICE_HOST" 23 "$TELNET_LOG" "$SUMMARY_LOG" "$STOP_FILE" \
    "$TELNET_CONNECT_TIMEOUT_SECONDS" \
    "$TELNET_RECONNECT_DELAY_MILLISECONDS" \
    "$TELNET_POST_DISCONNECT_DELAY_MILLISECONDS" <<'PY' &
import os
import re
import select
import socket
import sys
import time
from datetime import datetime, timezone

host = sys.argv[1]
port = int(sys.argv[2])
telnet_log = sys.argv[3]
summary_log = sys.argv[4]
stop_file = sys.argv[5]
base_timeout = int(sys.argv[6])
reconnect_delay = int(sys.argv[7]) / 1000.0
post_disconnect_delay = int(sys.argv[8]) / 1000.0
reboot_marker = re.compile(r"ESP\.restart\(\)")
debug_state = re.compile(r"(?m)\b3\s+MQTT\s+\[(?P<state>[01])\]")

def now():
    return datetime.now(timezone.utc).astimezone().isoformat()

def stopped():
    return os.path.exists(stop_file)

def add_summary(line):
    with open(summary_log, "a", encoding="utf-8") as f:
        f.write(line + "\n")

def wait_delay(seconds):
    end = time.monotonic() + seconds
    while not stopped() and time.monotonic() < end:
        time.sleep(min(0.1, max(0.0, end - time.monotonic())))

def read_available(sock, writer):
    chunks = []
    while not stopped():
        ready, _, _ = select.select([sock], [], [], 0)
        if not ready:
            break
        data = sock.recv(4096)
        if not data:
            raise ConnectionError("remote closed connection")
        text = data.decode("ascii", "replace")
        writer.write(text)
        writer.flush()
        chunks.append(text)
    return "".join(chunks)

def effective_timeout(attempt):
    return min(base_timeout + min(max(attempt - 1, 0), 6), 20)

def connect_capture(writer, attempt):
    timeout = effective_timeout(attempt)
    add_summary(f"Telnet connect started: {host}:{port} (timeout {timeout}s)")
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.setblocking(False)
    add_summary(f"Telnet connected: {now()}")

    banner = []
    end = time.monotonic() + base_timeout
    while not stopped() and time.monotonic() < end:
        try:
            text = read_available(sock, writer)
            if text:
                banner.append(text)
                if debug_state.search("".join(banner)):
                    break
        except BlockingIOError:
            pass
        time.sleep(0.1)

    banner_text = "".join(banner)
    match = debug_state.search(banner_text)
    if match:
        if match.group("state") == "0":
            sock.sendall(b"3")
            wait_delay(0.5)
            try:
                read_available(sock, writer)
            except Exception:
                pass
            action = "sent key 3 because MQTT debug was off"
        else:
            action = "not sent because MQTT debug was already on"
    else:
        action = "not sent because MQTT debug state was not found in the telnet banner"
    add_summary(f"MQTT debug toggle action: {action}")
    return sock

sock = None
has_connected = False
attempts = 0
next_connect = 0.0
reason = None
scan = ""

with open(telnet_log, "a", encoding="utf-8", errors="replace") as writer:
    while not stopped():
        if sock is None:
            if time.monotonic() < next_connect:
                wait_delay(min(0.25, next_connect - time.monotonic()))
                continue
            try:
                attempts += 1
                if has_connected:
                    suffix = f" after {reason}" if reason else ""
                    add_summary(f"Telnet reconnect attempt #{attempts} started{suffix} (timeout {effective_timeout(attempts)}s).")
                sock = connect_capture(writer, attempts)
                scan = ""
                if has_connected:
                    add_summary(f"Telnet reconnected after {attempts} attempt(s): {now()}")
                else:
                    add_summary(f"Telnet connected: {now()}")
                has_connected = True
                attempts = 0
                reason = None
            except Exception as exc:
                next_connect = time.monotonic() + reconnect_delay
                add_summary(f"Telnet connect attempt #{attempts} failed (timeout {effective_timeout(attempts)}s): {exc} Retrying in {reconnect_delay:g}s.")
                try:
                    if sock:
                        sock.close()
                finally:
                    sock = None
                continue

        try:
            text = read_available(sock, writer)
            if text:
                scan = (scan + text)[-512:]
                if reboot_marker.search(scan):
                    reason = "ESP.restart() marker captured"
                    attempts = 0
                    next_connect = time.monotonic() + post_disconnect_delay
                    add_summary(f"Telnet captured ESP.restart(); waiting {post_disconnect_delay:g}s before reconnect attempts.")
                    sock.close()
                    sock = None
                    scan = ""
                    continue

            ready, _, _ = select.select([sock], [], [], 0)
            if ready:
                data = sock.recv(1, socket.MSG_PEEK)
                if not data:
                    reason = "connection closed"
                    attempts = 0
                    next_connect = time.monotonic() + post_disconnect_delay
                    add_summary(f"Telnet disconnected; waiting {post_disconnect_delay:g}s before reconnect attempts.")
                    sock.close()
                    sock = None
                    continue
        except BlockingIOError:
            pass
        except Exception as exc:
            reason = "read failed"
            attempts = 0
            next_connect = time.monotonic() + post_disconnect_delay
            add_summary(f"Telnet read failed: {exc}; waiting {post_disconnect_delay:g}s before reconnect attempts.")
            try:
                sock.close()
            except Exception:
                pass
            sock = None
            continue

        wait_delay(0.1)

if sock:
    try:
        sock.close()
    except Exception:
        pass
PY
  TELNET_PID=$!
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --device|-DeviceHost)
      require_option_value "$1" "${2-}"; DEVICE_HOST="$2"; shift 2 ;;
    --broker|-BrokerHost)
      require_option_value "$1" "${2-}"; BROKER_HOST="$2"; shift 2 ;;
    --broker-port|-BrokerPort)
      require_option_value "$1" "${2-}"; BROKER_PORT="$2"; shift 2 ;;
    --topic|-Topic)
      require_option_value "$1" "${2-}"; TOPIC="$2"; shift 2 ;;
    --user|--username|-Username)
      require_option_value "$1" "${2-}"; USERNAME="$2"; shift 2 ;;
    --password|-Password)
      require_option_value "$1" "${2-}"; PASSWORD="$2"; shift 2 ;;
    --duration|--duration-seconds|-DurationSeconds)
      require_option_value "$1" "${2-}"; DURATION_SECONDS="$2"; shift 2 ;;
    --output-root|-OutputRoot)
      require_option_value "$1" "${2-}"; OUTPUT_ROOT="$2"; shift 2 ;;
    --mosquitto-sub-path|-MosquittoSubPath)
      require_option_value "$1" "${2-}"; MOSQUITTO_SUB_PATH="$2"; shift 2 ;;
    --skip-tool-install|-SkipToolInstall)
      SKIP_TOOL_INSTALL=1; shift ;;
    --telnet-connect-timeout-seconds|-TelnetConnectTimeoutSeconds)
      require_option_value "$1" "${2-}"; TELNET_CONNECT_TIMEOUT_SECONDS="$2"; shift 2 ;;
    --telnet-reconnect-delay-ms|--telnet-reconnect-delay-milliseconds|-TelnetReconnectDelayMilliseconds)
      require_option_value "$1" "${2-}"; TELNET_RECONNECT_DELAY_MILLISECONDS="$2"; shift 2 ;;
    --telnet-post-disconnect-delay-ms|--telnet-post-disconnect-delay-milliseconds|-TelnetPostDisconnectDelayMilliseconds)
      require_option_value "$1" "${2-}"; TELNET_POST_DISCONNECT_DELAY_MILLISECONDS="$2"; shift 2 ;;
    --no-prompt)
      NO_PROMPT=1; shift ;;
    --help|-h|-Help|-\?)
      show_help; exit 0 ;;
    *)
      echo "Unknown argument: $1" >&2
      show_help
      exit 2 ;;
  esac
done

validate_range "--broker-port" "$BROKER_PORT" 1 65535
if [[ -n "$DURATION_SECONDS" ]]; then
  validate_range "--duration" "$DURATION_SECONDS" 1 2147483647
fi
validate_range "--telnet-connect-timeout-seconds" "$TELNET_CONNECT_TIMEOUT_SECONDS" 2 60
validate_range "--telnet-reconnect-delay-ms" "$TELNET_RECONNECT_DELAY_MILLISECONDS" 250 60000
validate_range "--telnet-post-disconnect-delay-ms" "$TELNET_POST_DISCONNECT_DELAY_MILLISECONDS" 250 60000

if [[ "$NO_PROMPT" -eq 0 ]]; then
  if [[ -z "$DEVICE_HOST" ]]; then
    read -r -p "OTGW device host: " DEVICE_HOST
  fi

  if [[ -z "$BROKER_HOST" ]]; then
    read -r -p "MQTT broker host: " BROKER_HOST
  fi

  if [[ -z "$USERNAME" ]]; then
    read -r -p "MQTT username (blank for anonymous): " USERNAME
  fi

  if [[ -n "$USERNAME" && -z "$PASSWORD" ]]; then
    read -r -s -p "MQTT password for $USERNAME: " PASSWORD
    echo
  fi
fi

if [[ -z "$DEVICE_HOST" ]]; then
  fail "Device host is required."
fi
if [[ -z "$BROKER_HOST" ]]; then
  fail "Broker host is required."
fi
if [[ -z "$USERNAME" && -n "$PASSWORD" ]]; then
  fail "Username is required when password is supplied."
fi
PYTHON="$(find_python3)"
RUN_NAME="$(date +"%Y%m%d-%H%M%S")"
RUN_PATH="$OUTPUT_ROOT/$RUN_NAME"
mkdir -p "$RUN_PATH"

SUMMARY_LOG="$RUN_PATH/summary.txt"
TELNET_LOG="$RUN_PATH/telnet.log"
MQTT_LOG="$RUN_PATH/mqtt.log"
MQTT_ERR_LOG="$RUN_PATH/mqtt.stderr.log"
TRANSCRIPT="$RUN_PATH/transcript.txt"
STOP_FILE="$RUN_PATH/.stop"

: > "$SUMMARY_LOG"
: > "$TELNET_LOG"
: > "$MQTT_LOG"
: > "$MQTT_ERR_LOG"

trap cleanup EXIT
trap on_signal INT TERM

write_summary "OTGW MQTT diagnostic capture"
write_summary "Run folder: $RUN_PATH"
write_summary "Started: $(timestamp)"
write_summary "DeviceHost: $DEVICE_HOST"
write_summary "TelnetPort: 23"
write_summary "TelnetConnectTimeoutSeconds: $TELNET_CONNECT_TIMEOUT_SECONDS"
write_summary "TelnetPostDisconnectDelayMilliseconds: $TELNET_POST_DISCONNECT_DELAY_MILLISECONDS"
write_summary "TelnetReconnectDelayMilliseconds: $TELNET_RECONNECT_DELAY_MILLISECONDS"
write_summary "BrokerHost: $BROKER_HOST"
write_summary "BrokerPort: $BROKER_PORT"
write_summary "Topic: $TOPIC"
write_summary "Username supplied: $([[ -n "$USERNAME" ]] && echo true || echo false)"
write_summary "DurationSeconds: ${DURATION_SECONDS:-until manual stop}"
write_summary "SkipToolInstall: $([[ "$SKIP_TOOL_INSTALL" -eq 1 ]] && echo true || echo false)"
write_summary "Telnet timeout strategy: adaptive (base + retry backoff, capped at 20s)"
write_summary "python3: $PYTHON"
write_summary "bash: ${BASH_VERSION:-unknown}"
if command -v sw_vers >/dev/null 2>&1; then
  write_summary "macOS: $(sw_vers -productVersion 2>/dev/null || echo unknown)"
fi

write_summary "Browser capture: not implemented in this script."

MOSQUITTO_SUB="$(ensure_mosquitto_sub)"
write_summary "mosquitto_sub: $MOSQUITTO_SUB"
write_summary "mosquitto_sub source: $([[ -n "$MOSQUITTO_SUB_PATH" ]] && echo explicit || echo PATH/common install path)"

MQTT_ARGS=(-h "$BROKER_HOST" -p "$BROKER_PORT" -t "$TOPIC" -v)
if [[ -n "$USERNAME" ]]; then
  MQTT_ARGS+=(-u "$USERNAME")
  if [[ -n "$PASSWORD" ]]; then
    MQTT_ARGS+=(-P "$PASSWORD")
  fi
fi

"$MOSQUITTO_SUB" "${MQTT_ARGS[@]}" > "$MQTT_LOG" 2> "$MQTT_ERR_LOG" &
MQTT_PID=$!
sleep 1
if ! kill -0 "$MQTT_PID" >/dev/null 2>&1; then
  wait "$MQTT_PID" 2>/dev/null || true
  write_summary "mosquitto_sub exited immediately. See mqtt.stderr.log."
  fail "MQTT capture could not start. Check $MQTT_ERR_LOG."
fi
write_summary "mosquitto_sub started: pid $MQTT_PID"

start_telnet_capture "$PYTHON"
write_summary "Telnet capture worker started: pid $TELNET_PID"
write_summary "Capture started: $(timestamp)"

echo "Capturing telnet and MQTT output in $RUN_PATH"
echo "Press Q to stop cleanly and leave transcript.txt. Ctrl+C also stops capture."

START_SECONDS="$(date +%s)"
while true; do
  if [[ -n "$DURATION_SECONDS" ]]; then
    now_seconds="$(date +%s)"
    if (( now_seconds - START_SECONDS >= DURATION_SECONDS )); then
      CAPTURE_STOP_REASON="duration elapsed"
      break
    fi
  fi

  if [[ -n "$MQTT_PID" ]] && ! kill -0 "$MQTT_PID" >/dev/null 2>&1; then
    wait "$MQTT_PID" 2>/dev/null
    mqtt_rc=$?
    write_summary "mosquitto_sub exited during capture with code $mqtt_rc."
    CAPTURE_STOP_REASON="mosquitto_sub exited"
    break
  fi

  if read -r -s -n 1 -t 0.2 key 2>/dev/null; then
    case "$key" in
      q|Q)
        CAPTURE_STOP_REASON="Q key"
        break
        ;;
    esac
  fi
done

cleanup
