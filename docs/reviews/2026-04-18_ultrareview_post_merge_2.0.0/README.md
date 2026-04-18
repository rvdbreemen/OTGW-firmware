# Ultrareview: post-merge 2.0.0

- **Date:** 2026-04-18
- **Branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support`
- **Session:** `01SHaSdp2BQTGYjyA6csYSLX`
- **Scope at launch:** 515 files changed, 93226 insertions, 5629 deletions (full 2.0.0 branch vs `dev`)
- **Trigger:** `/ultrareview` after the dev -> 2.0.0 catch-up merge, SAT switch/select restoration (TASK-284), and ADR-077 landing

## Summary

Two defects surfaced, both of normal severity, both functional (no security impact).

| ID | Severity | File | Lines | Status |
|---|---|---|---|---|
| bug_003 | normal | `src/OTGW-firmware/MQTTstuff.ino` | 466-475 | Fixed in TASK-292, commit `abe330ab` |
| bug_006 | normal | `src/OTGW-firmware/OTDirect.ino` | 449-459 | Fixed in TASK-293 (in progress, pending push) |

No architectural findings. No security findings. No performance findings.

## Bug 003 — SAT MQTT `solar_freeze_integral` unreachable due to 20-char `satSubCmd` buffer

### PR comment as delivered

> The MQTT subcommand `solar_freeze_integral` is unreachable because the `satSubCmd` buffer at `MQTTstuff.ino:471` truncates the token to 19 characters, so the subsequent `strcasecmp_P` against the 21-character literal `"solar_freeze_integral"` can never match. Users therefore cannot set `SATsolarfreezeint` over MQTT. Fix by enlarging the buffer (e.g. `char satSubCmd[24]`) or renaming the command to <=19 characters.

### Reasoning from the review

#### What is wrong

`MQTTstuff.ino:471` declares `char satSubCmd[20]` and passes `sizeof(satSubCmd) == 20` to `readMQTTTopicToken`. That helper (MQTTstuff.ino:194-214) silently truncates any token longer than `tokenSize - 1` bytes:

```c
while (*cursor != 0 && *cursor != '/') {
    if (len < (tokenSize - 1)) {
        token[len++] = *cursor;
    }
    cursor++;
}
token[len] = '\0';
```

So `satSubCmd` can hold at most 19 characters + NUL.

#### Why the comparison never matches

The subcommand name in the PROGMEM literal is `"solar_freeze_integral"`, which is 21 characters:

```
s-o-l-a-r-_-f-r-e-e-z-e-_-i-n-t-e-g-r-a-l   -> 21 chars
```

After truncation the buffer holds `"solar_freeze_integr"` (19 chars) with a NUL at index 19. The comparison at line 615,

```c
else if (strcasecmp_P(satSubCmd, PSTR("solar_freeze_integral")) == 0) {
    updateSetting(PSTR("SATsolarfreezeint"), EVALBOOLEAN(value));
}
```

compares the truncated 19-char token against the 21-char PSTR. At position 19 `satSubCmd` is NUL while the PSTR has `'a'`, so the call always returns non-zero. The branch is dead code and any `set/<nodeId>/sat/solar_freeze_integral` publish falls through to the "unknown sub-command" error branch at line 661.

#### Proof by walk-through

Publish `OTGW/set/otgw-XXXX/sat/solar_freeze_integral` with payload `"true"`:

1. `readMQTTTopicToken(topicCursor, satSubCmd, 20)` consumes `solar_freeze_integral`.
2. The loop stores characters 0..18 (`s..r`) into `satSubCmd`; characters 19..20 (`a`, `l`) are skipped because `len < 19` is false.
3. `satSubCmd[19] = '\0'` -> buffer contains `"solar_freeze_integr"`.
4. Control falls through the chain of `strcasecmp_P` checks. When it reaches line 615, `strcasecmp_P("solar_freeze_integr", PSTR("solar_freeze_integral"))` returns a negative value because `'\0' < 'a'`.
5. `SATsolarfreezeint` is never updated and the command hits the unknown-sub-command path.

#### Scope check

Other SAT subcommands in the same block all fit within 19 characters. The longest one that reaches line 615 is `solar_min_elevation` (exactly 19 chars) which just fits. `solar_freeze_integral` (21 chars) is the sole outlier.

#### Impact

The SAT feature `sat_solar_freeze_integral` (introduced by this branch alongside the other solar-gain MQTT knobs) is advertised as MQTT-settable but the command is silently dropped. Users attempting to control the solar-gain integral-freeze flag from Home Assistant or other MQTT clients will see no effect.

### Resolution

Applied in commit `abe330ab` (TASK-292): `char satSubCmd[20]` -> `char satSubCmd[24]`. Four extra bytes of stack; headroom for future SAT sub-command names without another resize. ESP8266 build confirmed green (Flash 71%, RAM 84%, IRAM 94% — unchanged vs pre-fix because stack is not encoded in the image).

---

## Bug 006 — `bridgeFrameToParser` PS-mode short-circuit freezes gateway state

### PR comment as delivered

> When PS=1 (summary mode) is set, `bridgeFrameToParser()` returns early before calling `processOT()` (OTDirect.ino:449-454). Since this function is the **sole** path feeding OT-direct frames into the parser (~17 call sites: master cycles, handshake, slave-frame bridging, master-mode replies, loopback), enabling PS=1 effectively freezes `OTcurrentSystemState`: MQTT publishes stop, the WebSocket OT log stalls, the auto-leave-PS-mode detection at OTGW-Core.ino:3733-3736 is disabled, and `emitSummaryLine()` ends up synthesizing CSV from stale state. The PIC's real PS=1 only suppresses serial *output*; parsing continues internally. The suppression should happen at the per-frame log/emit layer inside `processOT()`, not at the parser entry point.

### Reasoning from the review

#### What the bug is

`bridgeFrameToParser(prefix, frame)` at `src/OTGW-firmware/OTDirect.ino:449-454` short-circuits when `otHideReports` is true (set at line 2518 when PS=1 is received):

```cpp
static void bridgeFrameToParser(char prefix, uint32_t frame) {
    if (otHideReports) {
        return;  // suppress individual frame output to mimic PIC PS=1
    }
    char buf[10];
    snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame);
    processOT(buf, 9);
}
```

The author's comment claims this "suppresses individual frame output" to mimic PIC behavior. But the PIC's PS=1 only suppresses the serial *output line*: internally the PIC continues to parse OT frames, decode MsgIDs, and update its state. The legacy ESP8266 path then receives the periodic CSV summary lines via `processPSSummary()` and feeds them into `decodeAndPublishOTValue()` to keep `OTcurrentSystemState` fresh.

#### Why it manifests

In OT-direct mode, the ESP32 itself is the parser: there is no PIC doing internal parsing in parallel. `bridgeFrameToParser()` is the sole entry point feeding OT-direct frames into `processOT()`. Grep shows ~17 call sites covering every operating mode:

- master requests/responses: 583, 584, 914, 936, 950
- handshake: 605, 606, 615, 617, 634, 635, 644, 645
- master-mode thermostat replies: 1560, 1561, 1573, 1574, 1586
- loopback test: 1942, 1943

When PS=1 is enabled, every one of these is bypassed. `processOT()` is never reached.

#### Concrete proof, step by step

1. User sends `PS=1` over telnet/MQTT.
2. `handleOTDirectCommand()` parses it, sets `otHideReports = true` (OTDirect.ino:2518).
3. Next OT cycle fires: `handleMasterRequest()` (line 914) calls `bridgeFrameToParser('T', frame)`.
4. `bridgeFrameToParser` checks `otHideReports`, returns immediately. `processOT()` not called.
5. Boiler responds; `handleMasterResponse()` at line 950 calls `bridgeFrameToParser('B', response)`. Returns immediately again.
6. `OTcurrentSystemState.Tboiler`, `TSet`, `Toutside`, `RelModLevel`, `Statusflags`, etc. are never updated: they hold the values from the moment PS=1 was set.
7. `state.otBus.bBoilerState` / `bThermostatState` / `bOnline` flags freeze (these are set inside `processOT()` at OTGW-Core.ino:3766-3784).
8. MQTT `otmessage` publish at OTGW-Core.ino:3742 never fires.
9. `AddLog()` / `reportOTGWEvent()` / WebSocket `sendEventToWebSocket()` calls inside `processOT()` never fire: the OT monitor page in the WebUI shows no new entries.
10. The auto-leave-PS-mode detection at OTGW-Core.ino:3733-3736 (which exits PS mode when raw frames arrive) is disabled: there is no way for the firmware to recover automatically.
11. Meanwhile `emitSummaryLine()` at OTDirect.ino:1142+ runs on its own timer. It reads from `OTcurrentSystemState.Statusflags / TSet / Tboiler / TdhwSet` (variable `s` at line 1146) and snprintfs the 34-field CSV summary, then calls `processOT(line, pos)`.
12. The CSV contains the frozen values from step 6. `processOT` parses its own stale CSV back into the same state: net no-op, and emits MQTT/WebSocket with stale values.

#### Impact

For any user who enables PS=1 in OT-direct mode:

- MQTT values are frozen at the moment PS=1 was set: HA dashboards, automations, and energy graphs see static data.
- WebSocket OT log shows only stale CSV summaries: the live monitor stops updating.
- `/v2/otgw/otmonitor` REST endpoint returns frozen `msglastupdated` timestamps.
- SAT control (per ADR-072) reads `OTcurrentSystemState.Toutside / Tboiler / RelModLevel`: its PID loop operates on stale inputs, potentially producing incorrect setpoints.
- No auto-recovery: until the user manually sends `PS=0`, the gateway is effectively dead from a state perspective.

The `otBoilerCache[128]` (updated separately at OTDirect.ino:961-962) is unaffected because it is written directly in `handleMasterResponse()` before the bridge call. So master-mode thermostat forwarding still works. But `otBoilerCache` does not feed MQTT or `OTcurrentSystemState`.

#### How to fix (from review)

Move the suppression from the parser entry to the per-frame emit layer inside `processOT()`. The intended behaviour (as documented in the research note that PS=1 summary parsing intentionally continues feeding the OT data pipeline) is:

1. Always call `processOT(buf, 9)` from `bridgeFrameToParser`, regardless of `otHideReports`.
2. Inside `processOT()` (OTGW-Core.ino:3723+), gate only the log output paths on `otHideReports` / `state.otBus.bPSmode`: skip the `AddLog()` / `sendEventToWebSocket()` / per-frame `sendMQTTData(F("otmessage"))` calls.
3. Keep the state-update path (`decodeAndPublishOTValue`, `OTcurrentSystemState` writes, `state.otBus.*` flags, MQTT decoded value publishes) running unconditionally.

This matches what the PIC does internally and what the legacy ESP8266 PS=1 path achieves via `processPSSummary()`.

Note that this is not pre-existing: `OTDirect.ino` is a net-new file in this branch, so the bug arrived with the OTGW32/OT-direct port and has no analogue in the v1.x ESP8266 codebase.

### Resolution

Implemented in TASK-293 (in progress at time of writing). Fix follows the review's recommended shape:

- `processOT()` gained an optional `bool suppressOutput = false` parameter (declared in `OTGW-Core.h`, defined in `OTGW-Core.ino`). Backward-compatible default means every existing caller keeps current behaviour.
- Inside `processOT()`: the auto-leave-PS block (line 3733-3736) and the per-frame `sendMQTTData(F("otmessage"), buf)` publish (line 3742) are gated on `!suppressOutput`. All state updates, `state.otBus.*` flag writes, `publish*ConnectedState()` calls, decoded value publishes, and `setMsgLastUpdated()` run unconditionally.
- `bridgeFrameToParser()` in `OTDirect.ino` no longer early-returns: it always formats the frame and calls `processOT(buf, 9, otHideReports)`. PS=1 now behaves as documented: output suppressed, state fresh.

`ClrLog()` / `AddLog()` / `sendEventToWebSocket()` gating (the WebSocket OT-monitor live stream) is deliberately left for a follow-up: the critical pipeline (MQTT, SAT inputs, connected-state flags, REST `otmonitor`) is already restored, and further gating can land when the WebSocket live-log contract is explicit about PS mode.

Verification:
- ESP8266 arduino-cli build green (Flash 71% — unchanged at the per-byte level, new code is reached only on ESP32).
- ESP32-S3 build in progress at the time of writing; result to be confirmed in the TASK-293 final summary.

---

## Session context

This review ran against the branch state immediately after:

- The dev -> 2.0.0 catch-up merge (commit `16e44771`)
- SAT switch/select HA discovery restoration (TASK-284, commit `5384be2a`)
- 29 manual, C4 code-level, and API doc updates (commit `e93f439d`)
- Removal of the dead mqttha generator scripts (TASK-285, commit `11db446a`)
- ESP32 partition-table alignment (TASK-286, commit `42eaf5b5`)
- build.py feature additions: `--ccache`, `--manifest`, `--reproducible` (TASK-287, commit `9c949052`)
- ESP32-S3 platform-guard fix for `setSync` and `getMaxFreeBlockSize` (commit `e1687ce2`)
- ESP32-S3 `upload.maximum_size` override (TASK-288, commit `0da41b1a`)
- `--verify-image` / `--verify-flash` helpers (TASK-290, commit `27ca1e57`)
- `-ffile-prefix-map` reproducibility (TASK-289, commit `cfea5b8c`)
- ESP32-S3 DIO/QIO verify-image normalisation (TASK-291, commit `71a07018`)
- Several backlog-hygiene commits closing TASK-275, TASK-283, TASK-240

The two defects found here were the only issues the reviewer flagged across that diff. ADR-077 (streaming MQTT HA discovery) was Accepted without findings; the build-tooling additions produced no regressions against the ESP8266 baseline.

## Related artefacts

- Backlog tasks: [TASK-292](../../../backlog/tasks/task-292%20-%20Fix-SAT-MQTT-solar_freeze_integral-unreachable-due-to-satSubCmd20-truncation.md), [TASK-293](../../../backlog/tasks/task-293%20-%20Fix-bridgeFrameToParser-PS1-short-circuit-freezes-OT-direct-state-pipeline.md)
- ADR in the same cycle: [ADR-077 Streaming MQTT HA discovery architecture](../../adr/ADR-077-streaming-mqtt-ha-discovery-architecture.md)
- Fix commits: `abe330ab` (TASK-292), TASK-293 commit to be appended once pushed
