#!/usr/bin/env bash
# =============================================================================
# capture-mqtt-debug.sh
# OTGW MQTT + telnet + browser diagnostic capture for macOS/Linux
#
# Single-file delivery. Bash handles prompts, process supervision, mosquitto_sub,
# cleanup, and transcript merging. Embedded Python workers handle reconnecting
# telnet capture and optional Chrome/Edge DevTools capture.
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
SKIP_BROWSER_CAPTURE=0
BROWSER_URL=""
BROWSER_DEBUG_PORT=9222
BROWSER_PATH=""
NO_PROMPT=0

RUN_PATH=""
SUMMARY_LOG=""
TELNET_LOG=""
MQTT_LOG=""
MQTT_ERR_LOG=""
BROWSER_LOG=""
TRANSCRIPT=""
STOP_FILE=""
MQTT_PID=""
TELNET_PID=""
BROWSER_PID=""
BROWSER_PROFILE=""
CLEANED_UP=0
CAPTURE_STOP_REASON=""

show_help() {
  cat <<EOF
OTGW MQTT diagnostic capture for macOS/Linux

Usage:
  ./capture-mqtt-debug.sh [options]
  ./capture-mqtt-debug.sh --help

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

Browser DevTools capture:
  Enabled by default when Chrome or Edge can be found. It records console output,
  exceptions, resource failures, and network timings to browser.log.
  --skip-browser-capture                  Disable browser capture
  --browser-url <url>                     Page to load, default http://<device>/
  --browser-debug-port <port>             CDP port, default 9222; auto-bumped if busy
  --browser-path <path>                   Explicit Chrome/Edge executable path

Stop:
  Press Q to stop cleanly and create:
  transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt
  Ctrl+C also stops cleanly.

Telnet debug logging:
  On every connection, the script enables all logging toggles that are off,
  excluding simulator toggles such as SensorSim and OTGW-Sim. It then sends q
  and D so the current settings and state are captured in telnet.log.

Aliases:
  Windows-style aliases are accepted, for example -DeviceHost, -BrokerHost,
  -DurationSeconds, -MosquittoSubPath, and -SkipBrowserCapture.

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

  fail "python3 is required for telnet/browser capture on macOS/Linux."
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

port_is_free() {
  local port="$1"

  if command -v lsof >/dev/null 2>&1; then
    ! lsof -nP -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
    return
  fi

  return 0
}

select_debug_port() {
  local preferred="$1"
  local port

  for ((port = preferred; port < preferred + 12; port++)); do
    if port_is_free "$port"; then
      printf '%s\n' "$port"
      return 0
    fi
  done

  printf '%s\n' "$preferred"
}

find_browser() {
  if [[ -n "$BROWSER_PATH" ]]; then
    if [[ -x "$BROWSER_PATH" ]]; then
      printf '%s\n' "$BROWSER_PATH"
      return 0
    fi
    fail "Explicit --browser-path was not found or is not executable: $BROWSER_PATH"
  fi

  local candidate
  for candidate in \
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
    "$HOME/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
    "$HOME/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
    "/Applications/Chromium.app/Contents/MacOS/Chromium" \
    "$HOME/Applications/Chromium.app/Contents/MacOS/Chromium"; do
    if [[ -x "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  for candidate in microsoft-edge msedge google-chrome chrome chromium; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done

  return 1
}

merge_transcript() {
  local temp_transcript="$TRANSCRIPT.tmp"

  if ! {
    echo "OTGW capture - merged transcript"
    echo "Generated: $(timestamp)"
    echo "Run folder: $RUN_PATH"
    echo "Upload THIS single file; it contains every capture log below."
    echo

    for item in \
      "SUMMARY (summary.txt)|$SUMMARY_LOG" \
      "OTGW TELNET DEBUG (telnet.log)|$TELNET_LOG" \
      "MQTT BROKER STREAM (mqtt.log)|$MQTT_LOG" \
      "MQTT STDERR (mqtt.stderr.log)|$MQTT_ERR_LOG" \
      "BROWSER DEVTOOLS (browser.log)|$BROWSER_LOG"; do
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
  } > "$temp_transcript"; then
    rm -f "$temp_transcript"
    return 1
  fi

  if ! mv "$temp_transcript" "$TRANSCRIPT"; then
    rm -f "$temp_transcript"
    return 1
  fi
}

resolve_capture_metadata() {
  "$PYTHON" - "$DEVICE_HOST" "$TELNET_LOG" "$RUN_NAME" <<'PY'
import json
import re
import sys
import urllib.request

device_host, telnet_log, run_name = sys.argv[1:4]
metadata = {
    "hostname": "",
    "firmware_version": "",
    "unique_id": "",
    "pic_device_id": "",
}
sources = []

def usable(value):
    text = "" if value is None else str(value).strip()
    return text.lower() not in {"", "null", "unknown", "(not set)"}

def add(name, value, source, prefer_existing=False):
    if not usable(value) or (prefer_existing and usable(metadata[name])):
        return
    metadata[name] = str(value).strip()
    sources.append(f"{name}={source}")

def get_property(value, name):
    return value.get(name) if isinstance(value, dict) else None

def get_setting(settings, name):
    entry = get_property(settings, name)
    if isinstance(entry, dict) and "value" in entry:
        return entry["value"]
    return entry

def fetch_json(path):
    request = urllib.request.Request(f"http://{device_host}{path}")
    with urllib.request.urlopen(request, timeout=2) as response:
        return json.loads(response.read().decode("utf-8"))

try:
    patterns = {
        "hostname": re.compile(r"^\s*hostname:\s*(.+?)\s*$", re.IGNORECASE),
        "firmware_version": re.compile(r"^\s*version:\s*(.+?)\s*$", re.IGNORECASE),
        "unique_id": re.compile(r"^\s*unique_id:\s*(.+?)\s*$", re.IGNORECASE),
        "pic_device_id": re.compile(r"^\s*device_id:\s*(.+?)\s*$", re.IGNORECASE),
    }
    with open(telnet_log, "r", encoding="utf-8", errors="replace") as capture:
        for line in capture:
            for name, pattern in patterns.items():
                match = pattern.match(line)
                if match:
                    add(name, match.group(1), "telnet.log", prefer_existing=name in {"hostname", "firmware_version"})
except OSError:
    pass

needs_api = not all(usable(metadata[name]) for name in ("hostname", "firmware_version", "unique_id"))

if needs_api:
    try:
        debug = get_property(fetch_json("/api/v2/debug"), "debug")
        add("hostname", get_property(debug, "settings.hostname"), "/api/v2/debug", prefer_existing=True)
        add("firmware_version", get_property(debug, "build.version"), "/api/v2/debug", prefer_existing=True)
        add("unique_id", get_property(debug, "settings.mqtt.unique_id"), "/api/v2/debug")
    except Exception:
        pass

if needs_api and not usable(metadata["unique_id"]):
    try:
        settings = get_property(fetch_json("/api/v2/settings"), "settings")
        add("hostname", get_setting(settings, "hostname"), "/api/v2/settings", prefer_existing=True)
        add("unique_id", get_setting(settings, "mqttuniqueid"), "/api/v2/settings")
    except Exception:
        pass

if needs_api:
    try:
        device = get_property(fetch_json("/api/v2/device/info"), "device")
        add("hostname", get_property(device, "hostname"), "/api/v2/device/info", prefer_existing=True)
        add("firmware_version", get_property(device, "fwversion"), "/api/v2/device/info", prefer_existing=True)
        if not usable(metadata["unique_id"]):
            mac = re.sub(r"[^0-9A-Fa-f]", "", str(get_property(device, "macaddress") or ""))
            if mac:
                add("unique_id", f"otgw-{mac.upper()}", "/api/v2/device/info macaddress")
    except Exception:
        pass

def safe_part(value, fallback, max_length=64):
    part = str(value).strip() if usable(value) else fallback
    part = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "-", part)
    part = re.sub(r"\s+", "-", part)
    part = re.sub(r"-+", "-", part).strip(".-")
    if not part:
        part = fallback
    return part[:max_length].strip(".-") or fallback

hostname = metadata["hostname"] if usable(metadata["hostname"]) else device_host
firmware_version = metadata["firmware_version"] if usable(metadata["firmware_version"]) else "unknown-version"
device_id = metadata["unique_id"] if usable(metadata["unique_id"]) else metadata["pic_device_id"]
if not usable(device_id):
    device_id = "unknown-id"

filename = "transcript-{}-{}-{}-{}.txt".format(
    run_name,
    safe_part(firmware_version, "unknown-version"),
    safe_part(hostname, device_host),
    safe_part(device_id, "unknown-id"),
)

print(filename)
print(hostname)
print(firmware_version)
print(metadata["unique_id"] if usable(metadata["unique_id"]) else "(unknown)")
print(metadata["pic_device_id"] if usable(metadata["pic_device_id"]) else "(unknown)")
print(device_id)
print(", ".join(sources) if sources else "(none)")
PY
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
  local metadata_output
  local transcript_filename
  local resolved_hostname
  local resolved_firmware_version
  local resolved_unique_id
  local resolved_pic_device_id
  local resolved_device_id
  local metadata_sources

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
  stop_child "Browser capture worker" "${BROWSER_PID:-}" 20

  if ! metadata_output="$(resolve_capture_metadata)"; then
    metadata_output="$(printf '%s\n' \
      "transcript-$RUN_NAME-unknown-version-$DEVICE_HOST-unknown-id.txt" \
      "$DEVICE_HOST" \
      "unknown-version" \
      "(unknown)" \
      "(unknown)" \
      "unknown-id" \
      "(metadata resolution failed)")"
  fi
  transcript_filename="$(printf '%s\n' "$metadata_output" | sed -n '1p')"
  resolved_hostname="$(printf '%s\n' "$metadata_output" | sed -n '2p')"
  resolved_firmware_version="$(printf '%s\n' "$metadata_output" | sed -n '3p')"
  resolved_unique_id="$(printf '%s\n' "$metadata_output" | sed -n '4p')"
  resolved_pic_device_id="$(printf '%s\n' "$metadata_output" | sed -n '5p')"
  resolved_device_id="$(printf '%s\n' "$metadata_output" | sed -n '6p')"
  metadata_sources="$(printf '%s\n' "$metadata_output" | sed -n '7p')"
  transcript_filename="${transcript_filename:-transcript-$RUN_NAME-unknown-version-$DEVICE_HOST-unknown-id.txt}"
  resolved_hostname="${resolved_hostname:-$DEVICE_HOST}"
  resolved_firmware_version="${resolved_firmware_version:-unknown-version}"
  resolved_unique_id="${resolved_unique_id:-(unknown)}"
  resolved_pic_device_id="${resolved_pic_device_id:-(unknown)}"
  resolved_device_id="${resolved_device_id:-unknown-id}"
  metadata_sources="${metadata_sources:-(none)}"
  TRANSCRIPT="$RUN_PATH/$transcript_filename"

  write_summary "ResolvedHostname: $resolved_hostname"
  write_summary "ResolvedFirmwareVersion: $resolved_firmware_version"
  write_summary "ResolvedUniqueId: $resolved_unique_id"
  write_summary "ResolvedPicDeviceId: $resolved_pic_device_id"
  write_summary "ResolvedDeviceIdForFilename: $resolved_device_id"
  write_summary "Metadata sources: $metadata_sources"
  write_summary "Transcript filename: $transcript_filename"
  write_summary "Finished: $(timestamp)"

  if merge_transcript; then
    for file in "$SUMMARY_LOG" "$TELNET_LOG" "$MQTT_LOG" "$MQTT_ERR_LOG" "$BROWSER_LOG"; do
      if [[ -f "$file" ]]; then
        rm -f "$file"
      fi
    done
    echo "Merged transcript (single upload): $TRANSCRIPT"
  else
    echo "Could not build merged transcript; intermediate capture files were preserved." >&2
  fi

  if [[ -n "$BROWSER_PROFILE" && -d "$BROWSER_PROFILE" ]]; then
    rm -rf "$BROWSER_PROFILE"
  fi
  if [[ -n "$STOP_FILE" && -f "$STOP_FILE" ]]; then
    rm -f "$STOP_FILE"
  fi

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
banner_footer = re.compile(r"Press 'h' for command menu|\bOTGW-Sim\b", re.MULTILINE)
debug_toggle = re.compile(
    r"(?:^|\s)(?P<key>\S)\s+"
    r"(?P<label>[A-Za-z][A-Za-z0-9 /]*?)\s*"
    r"\[(?P<state>[01])\]",
    re.MULTILINE,
)
simulator_labels = {"SensorSim", "OTGW-Sim"}

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

def read_initial_banner(sock, writer):
    chunks = []
    end = time.monotonic() + base_timeout
    while not stopped() and time.monotonic() < end:
        try:
            text = read_available(sock, writer)
            if text:
                chunks.append(text)
                if banner_footer.search("".join(chunks)):
                    break
        except BlockingIOError:
            pass
        wait_delay(0.1)
    return "".join(chunks)

def enable_debug_toggles(sock, writer, banner):
    results = []
    seen = set()
    for match in debug_toggle.finditer(banner):
        key = match.group("key")
        label = match.group("label").strip()
        state = match.group("state")
        if not label or label in seen:
            continue
        seen.add(label)

        if label in simulator_labels:
            results.append(f"{label}=skipped (simulator)")
        elif state == "0":
            sock.sendall(key.encode("ascii"))
            wait_delay(0.3)
            try:
                read_available(sock, writer)
            except BlockingIOError:
                pass
            results.append(f"{label}=on (sent '{key}')")
        else:
            results.append(f"{label}=already-on")

    return "; ".join(results) if results else "no debug toggles found in banner"

def send_key_and_drain(sock, writer, key, timeout=5.0, idle_seconds=0.8):
    sock.sendall(key.encode("ascii"))
    deadline = time.monotonic() + timeout
    last_data = time.monotonic()
    while not stopped() and time.monotonic() < deadline:
        try:
            text = read_available(sock, writer)
        except BlockingIOError:
            text = ""
        if text:
            last_data = time.monotonic()
        elif time.monotonic() - last_data >= idle_seconds:
            break
        wait_delay(0.1)

def request_settings_dump(sock, writer):
    send_key_and_drain(sock, writer, "q")
    send_key_and_drain(sock, writer, "D")
    return "sent 'q' (read settings) + 'D' (dump settings/state)"

def effective_timeout(attempt):
    return min(base_timeout + min(max(attempt - 1, 0), 6), 20)

def connect_capture(writer, attempt):
    timeout = effective_timeout(attempt)
    add_summary(f"Telnet connect started: {host}:{port} (timeout {timeout}s)")
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.setblocking(False)
    add_summary(f"Telnet connected: {now()}")

    banner = read_initial_banner(sock, writer)
    add_summary(f"Debug toggle actions: {enable_debug_toggles(sock, writer, banner)}")
    add_summary(f"Settings dump: {request_settings_dump(sock, writer)}")
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

start_browser_capture() {
  local python="$1"
  local resolved_browser="$2"
  local chosen_port="$3"

  "$python" - "$resolved_browser" "$BROWSER_URL" "$chosen_port" "$BROWSER_PROFILE" "$BROWSER_LOG" "$SUMMARY_LOG" "$STOP_FILE" <<'PY' &
import base64
import json
import os
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from datetime import datetime

browser_path, device_url, debug_port, temp_profile, browser_log, summary_log, stop_file = sys.argv[1:8]
debug_port = int(debug_port)

def stopped():
    return os.path.exists(stop_file)

def log(line):
    with open(browser_log, "a", encoding="utf-8") as f:
        f.write(datetime.now().strftime("%H:%M:%S.%f")[:-3] + "  " + line + "\n")

def summary(line):
    with open(summary_log, "a", encoding="utf-8") as f:
        f.write(line + "\n")

class WebSocket:
    def __init__(self, url):
        rest = url.split("://", 1)[1]
        host_port, path = rest.split("/", 1)
        host, port = host_port, 80
        if ":" in host_port:
            host, port_text = host_port.rsplit(":", 1)
            port = int(port_text)
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(1)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (
            f"GET /{path} HTTP/1.1\r\n"
            f"Host: {host_port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(request.encode("ascii"))
        response = b""
        while b"\r\n\r\n" not in response:
            response += self.sock.recv(4096)
        if b" 101 " not in response.split(b"\r\n", 1)[0]:
            raise RuntimeError("websocket upgrade failed")

    def send_json(self, payload):
        data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        mask = os.urandom(4)
        header = bytearray([0x81])
        length = len(data)
        if length < 126:
            header.append(0x80 | length)
        elif length < 65536:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", length))
        header.extend(mask)
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
        self.sock.sendall(bytes(header) + masked)

    def recv_text(self):
        chunks = []
        while True:
            first = self._read_exact(2)
            fin = first[0] & 0x80
            opcode = first[0] & 0x0F
            masked = first[1] & 0x80
            length = first[1] & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._read_exact(8))[0]
            mask = self._read_exact(4) if masked else b""
            data = self._read_exact(length) if length else b""
            if masked:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            if opcode == 8:
                return None
            if opcode in (1, 0):
                chunks.append(data)
            if fin:
                return b"".join(chunks).decode("utf-8", "replace")

    def _read_exact(self, count):
        data = b""
        while len(data) < count:
            chunk = self.sock.recv(count - len(data))
            if not chunk:
                raise ConnectionError("websocket closed")
            data += chunk
        return data

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass

proc = None
ws = None
requests = {}
cdp_id = 0
capture_summary = "Browser capture: completed normally."

def send(method, params=None):
    global cdp_id
    cdp_id += 1
    payload = {"id": cdp_id, "method": method}
    if params:
        payload["params"] = params
    ws.send_json(payload)

try:
    log(f"browser executable: {browser_path}")
    log(f"device url: {device_url}   cdp port: {debug_port}")
    args = [
        browser_path,
        "--headless=new",
        "--disable-gpu",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-extensions",
        "--disable-background-networking",
        "--mute-audio",
        "--remote-allow-origins=*",
        f"--remote-debugging-port={debug_port}",
        f"--user-data-dir={temp_profile}",
        "about:blank",
    ]
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    ws_url = None
    deadline = time.monotonic() + 15
    while not stopped() and time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{debug_port}/json", timeout=1) as response:
                targets = json.loads(response.read().decode("utf-8"))
            for target in targets:
                if target.get("type") == "page" and target.get("webSocketDebuggerUrl"):
                    ws_url = target["webSocketDebuggerUrl"]
                    break
            if ws_url:
                break
        except Exception:
            time.sleep(0.3)
        time.sleep(0.2)
    if not ws_url:
        if stopped():
            capture_summary = "Browser capture: stopped before CDP page target was ready."
            log("stop requested before cdp page target was ready")
            raise SystemExit(0)
        raise RuntimeError(f"CDP page target not found on port {debug_port} within 15s")

    log(f"cdp websocket: {ws_url}")
    ws = WebSocket(ws_url)
    log("cdp connected")
    send("Network.enable")
    send("Runtime.enable")
    send("Log.enable")
    send("Page.enable")
    send("Page.navigate", {"url": device_url})
    log(f"domains enabled; navigating to {device_url}")

    while not stopped():
        try:
            text = ws.recv_text()
        except socket.timeout:
            continue
        if text is None:
            break
        try:
            evt = json.loads(text)
        except Exception:
            continue
        method = evt.get("method")
        params = evt.get("params", {})
        if not method:
            continue

        if method == "Runtime.consoleAPICalled":
            parts = []
            for arg in params.get("args", []):
                if "value" in arg:
                    parts.append(str(arg["value"]))
                elif arg.get("description"):
                    parts.append(str(arg["description"]))
                elif arg.get("unserializableValue"):
                    parts.append(str(arg["unserializableValue"]))
                else:
                    parts.append(f"[{arg.get('type', 'unknown')}]")
            log(f"[console.{params.get('type', 'log')}] " + " ".join(parts))
        elif method == "Runtime.exceptionThrown":
            details = params.get("exceptionDetails", {})
            exc = details.get("exception", {})
            log("[exception] " + str(exc.get("description") or details.get("text") or "unknown exception"))
        elif method == "Log.entryAdded":
            entry = params.get("entry", {})
            where = f" ({entry.get('url')})" if entry.get("url") else ""
            log(f"[log.{entry.get('level')}] {entry.get('text')}{where}")
        elif method == "Network.requestWillBeSent":
            request_id = params.get("requestId")
            req = params.get("request", {})
            if request_id:
                requests[request_id] = {"url": req.get("url", ""), "start": float(params.get("timestamp", 0)), "status": ""}
        elif method == "Network.responseReceived":
            request_id = params.get("requestId")
            if request_id in requests:
                requests[request_id]["status"] = str(params.get("response", {}).get("status", ""))
        elif method == "Network.loadingFinished":
            request_id = params.get("requestId")
            record = requests.pop(request_id, None)
            if record:
                ms = int((float(params.get("timestamp", 0)) - record["start"]) * 1000)
                log(f"[net] {ms:6d} ms  {record['status']:>3}  {record['url']}")
        elif method == "Network.loadingFailed":
            request_id = params.get("requestId")
            record = requests.pop(request_id, None)
            if record:
                log(f"[net] FAILED      {params.get('errorText')}  {record['url']}")

    for record in requests.values():
        status = record["status"] or "no-response"
        log(f"[net] PENDING     {status:>3}  {record['url']}  (started, never finished)")
    log("stop requested; closing capture")
except Exception as exc:
    capture_summary = f"Browser capture: error - {exc}"
    try:
        log(f"[worker-error] {exc}")
    except Exception:
        pass
finally:
    if ws:
        ws.close()
    if proc and proc.poll() is None:
        try:
            proc.terminate()
            proc.wait(timeout=3)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
    summary(capture_summary)
PY
  BROWSER_PID=$!
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
    --skip-browser-capture|-SkipBrowserCapture)
      SKIP_BROWSER_CAPTURE=1; shift ;;
    --browser-url|-BrowserUrl)
      require_option_value "$1" "${2-}"; BROWSER_URL="$2"; shift 2 ;;
    --browser-debug-port|-BrowserDebugPort)
      require_option_value "$1" "${2-}"; BROWSER_DEBUG_PORT="$2"; shift 2 ;;
    --browser-path|-BrowserPath)
      require_option_value "$1" "${2-}"; BROWSER_PATH="$2"; shift 2 ;;
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
validate_range "--browser-debug-port" "$BROWSER_DEBUG_PORT" 1 65535

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
if [[ -z "$BROWSER_URL" ]]; then
  BROWSER_URL="http://$DEVICE_HOST/"
fi
PYTHON="$(find_python3)"
RUN_NAME="$(date +"%Y%m%d-%H%M%S")"
RUN_PATH="$OUTPUT_ROOT/$RUN_NAME"
mkdir -p "$RUN_PATH"

SUMMARY_LOG="$RUN_PATH/summary.txt"
TELNET_LOG="$RUN_PATH/telnet.log"
MQTT_LOG="$RUN_PATH/mqtt.log"
MQTT_ERR_LOG="$RUN_PATH/mqtt.stderr.log"
BROWSER_LOG="$RUN_PATH/browser.log"
STOP_FILE="$RUN_PATH/.stop"

: > "$SUMMARY_LOG"
: > "$TELNET_LOG"
: > "$MQTT_LOG"
: > "$MQTT_ERR_LOG"
: > "$BROWSER_LOG"

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

if [[ "$SKIP_BROWSER_CAPTURE" -eq 1 ]]; then
  write_summary "Browser capture: disabled (--skip-browser-capture)."
else
  if resolved_browser="$(find_browser 2>/dev/null)"; then
    chosen_port="$(select_debug_port "$BROWSER_DEBUG_PORT")"
    BROWSER_PROFILE="${TMPDIR:-/tmp}/otgw-browser-$RUN_NAME-$$"
    mkdir -p "$BROWSER_PROFILE"
    write_summary "Browser capture: $resolved_browser"
    write_summary "Browser url: $BROWSER_URL"
    write_summary "Browser CDP port: $chosen_port"
    start_browser_capture "$PYTHON" "$resolved_browser" "$chosen_port"
    write_summary "Browser capture started (headless, writing browser.log)."
  else
    write_summary "Browser capture: skipped - no Edge/Chrome found (pass --browser-path or --skip-browser-capture)."
  fi
fi

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

echo "Capturing telnet, MQTT, and browser output in $RUN_PATH"
echo "Press Q to stop cleanly and leave a transcript timestamp-version-host-uniqueid file."
echo "Ctrl+C also stops capture."

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
