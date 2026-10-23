# OTGW-firmware host tests

Small host-compilable unit tests for pure logic that does not depend on the
Arduino runtime. They let us validate byte-manipulation code on a laptop
without flashing an ESP8266.

Full integration testing still happens on the device itself; these tests
cover isolated, deterministic functions only.

## Current tests

| File | What it covers |
| --- | --- |
| `test_dallas_address.cpp` | `getDallasAddress()` hex-string conversion for Dallas DS18B20 ROM codes |
| `test_otdirect_override.cpp` | TT/TC remote-override f8.8 round-trip, sign-extend, clamp, honour-cycle, auto-clear, plus otCmdEnqueue coalesce-by-MsgID semantics across MsgIDs 1, 14, 16, 100 |
| `test_ble_parsers.cpp` | ATC/pvvx + BTHome v2 byte-layout parsers (TASK-487 / TASK-498), encrypted-flag rejection (3A-M4), packet-id prefix skip (2A-M3), and MAC-filter strict-vs-empty paths (3A-M2) |
| `test_graph_history.cpp` | Monitor graph history (TASK-1296): compiles the real `graphHistoryStuff.h` and `jsonEmit.h` against the stubs in `tests/stubs`. Both ring sizes (126 slots of 30 s, 726 slots of 120 s): wrap, commit with gap fill (k late gives k-1 empty slots, capped), flag accumulation and reset, encoders (rounding, sentinels, NaN), the 720-slot emit cap, exact JSON for a small ring, the empty ring, and chunk-window reassembly with commits between windows (margin pinned: MARGIN identical, MARGIN+1 differs) |
| `webui/graph-history-merge.test.mjs` | `mergeGraphHistory()` from `v2.js` (node:test, real function extracted): older history kept, overlap dropped, sorted with unique `t`, scale and nulls, trim to `GHIST_MAX`, malformed payloads leave the history unchanged. Run `node --test tests/webui/graph-history-merge.test.mjs`; set `OTGW_V2_JS` to an older copy to see it red |
| `webui/graph-break.test.mjs` | `graphPath()` from `v2.js`: a series breaks where two points are more than 150 s apart (120 s history spacing stays one segment), the modulation area closes per segment, a lane does not hold its value across a gap |
| `test_capture_telnet_worker.py` | Telnet worker embedded in `scripts/capture-mqtt-debug.sh` (TASK-1299): the real heredoc is extracted and run against a fake device (telnet listener + `/api/v2/health` with controllable uptime/status). RST reconnect with gap line and marker, quiet healthy device never disconnected, uptime drop and unreachable health reconnect, 503 keeps the connection, watchdog gap starts at the last successful probe, open gap and totals at stop (SIGTERM too on POSIX). Run `python tests/test_capture_telnet_worker.py` (about 60 s); set `OTGW_CAPTURE_SH` to an older script copy to see it red |
| `test_oled_row_clear.cpp` | OLED row clearing (TASK-1151). Models the display as a character grid and reproduces the reported `Ethernet6617823` / `0.0.0.0.1.150` tails with the old positioning, proves `oledRow()` removes them, and statically asserts `OLED.ino` has no raw `oledDisplay.setRow(` left so no call site can reintroduce the hazard |

## Building and running

Any C++17 host compiler works (gcc, clang, MSVC). The tests stub
`PROGMEM` and `pgm_read_byte()` so no Arduino headers are required.

### g++ / clang++ (Linux, macOS, MSYS2, Git Bash with MinGW)

```bash
g++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out
./tests/test_dallas_address.out
echo "exit=$?"   # 0 on success, 1 on failure
```

`clang++` is a drop-in replacement:

```bash
clang++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out
```

### MSVC (Developer Command Prompt)

```bat
cl /std:c++17 /EHsc tests\test_dallas_address.cpp /Fe:tests\test_dallas_address.exe
tests\test_dallas_address.exe
```

### Expected output

```
=== Dallas Address Conversion Test ===
standard address                 expected=28FF641E8216C3A1 got=28FF641E8216C3A1 PASS
leading zero bytes               expected=0102030405060708 got=0102030405060708 PASS
all zeros                        expected=0000000000000000 got=0000000000000000 PASS
all 0xFF                         expected=FFFFFFFFFFFFFFFF got=FFFFFFFFFFFFFFFF PASS
length check                     expected=16 got=16 PASS
mixed nibbles                    expected=AABBCCDDEEFF1122 got=AABBCCDDEEFF1122 PASS
NUL terminator at index 16       PASS
=== ALL TESTS PASSED (failures=0) ===
```

Exit code is `0` on pass, `1` if any check fails.

## Artifacts

Build output (`*.out`, `*.exe`) is ignored by the existing repo `.gitignore`
patterns for compiled binaries. If you need to keep them out explicitly,
add `tests/*.out` and `tests/*.exe` to your local ignore list.

## Adding a new host test

1. Write a single self-contained `.cpp` file under `tests/`.
2. Stub any Arduino-only macros/types at the top (`PROGMEM`, `pgm_read_byte`,
   `F()`, `PSTR()`, etc.). Keep stubs minimal and explicitly marked.
3. Copy the function under test verbatim, or `#include` a header if the
   firmware source already has a clean host-compatible seam.
4. Implement a small `main()` that runs the cases, prints pass/fail per
   case, and returns non-zero if any case fails.
5. Add a row to the table above and a build command if it differs.

Do not add anything that requires `millis()`, `delay()`, `Serial`, network
stacks, or actual peripherals. Those belong on the device.

## Web UI tests (`tests/webui/`)

Node test files, run one at a time with `node --test tests/webui/<name>.test.mjs`.

- `ha-discovery-golden` needs Node alone.
- `sat-status-pill` and `sat-area-sensor-mapping` drive the shipped `v2.html`/`v2.js`
  in headless system Chrome or Edge over the DevTools protocol, against a local fake
  device. No npm install; they print SKIP and pass when no browser is found.
  `OTGW_DATA_DIR=<dir>` serves another data dir (used for old-vs-fix runs).
  - `sat-status-pill`: the pill reports burner activity, not control mode (TASK-1152),
    plus the headline precedence disabled > safety > window > summer and the
    missing-`boiler_status` fallback (TASK-1234).
  - `sat-area-sensor-mapping`: ADR-178 guard. The area pickers offer both Dallas
    addresses and BLE MACs, each tagged by source, and a chosen MAC is saved intact
    via `POST /api/v2/settings` (TASK-1234).
- `settings-error-toast` needs Node alone. It runs the shipped `bleToast()` and
  `saveSettings()` against a stub fetch: a rejected save shows the red `.toast.err`
  variant, a good save the green toast (TASK-1254). `OTGW_V2_JS` / `OTGW_V2_CSS`
  point it at older copies for old-vs-fix runs.
- The other classic-UI tests (`paced-poller`, `sat-log-filter`, `sat-settings-layout`,
  `tab-restore`, `webui_label_coverage`, `webui_settings_layout`) were removed with
  the classic UI (ADR-192, TASK-1223.12).

## Python source guards

Stdlib `unittest`, run with `python tests/<name>.py`.

- `test_settings_whitelist.py`: every key `GET /api/v2/settings` emits in
  `restAPI.ino` must be in `knownSettings[]` (else `POST` answers 400 "Unknown
  setting name"), except the commented read-only allowlist (type `"r"`, e.g.
  `ssid`). `OTGW_RESTAPI_INO=<file>` checks another copy for old-vs-fix runs
  (TASK-1254).
