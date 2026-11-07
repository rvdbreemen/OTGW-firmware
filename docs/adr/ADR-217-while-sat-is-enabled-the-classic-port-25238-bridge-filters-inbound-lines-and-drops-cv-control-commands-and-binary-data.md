---
id: "ADR-217"
title: "While SAT is enabled, the Classic port 25238 bridge filters inbound lines and drops CV control commands and binary data"
status: "Proposed"
date: "2026-11-07"
binding: true
gate: "tests/test_bridge25238_sat_filter.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-177"
  - "ADR-179"
  - "ADR-203"
topics:
  - "port-25238"
  - "ser2net"
  - "otmonitor"
  - "sat"
  - "pic"
  - "command-filter"
aliases:
  - "25238 inbound filter"
  - "serial bridge SAT filter"
  - "OTmonitor command blocking while SAT is enabled"
  - "Net2Ser line filter"
components:
  - "Classic port 25238 inbound bridge (Net2Ser, OTGW-Core.ino)"
symbols:
  - "satOwnsControlSetpoint"
  - "satBlocksExternalCommand"
  - "enqueuePICTx"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-217 While SAT is enabled, the Classic port 25238 bridge filters inbound lines and drops CV control commands and binary data

## Status

Proposed, 2026-11-07.

This ADR (architecture decision record) is Proposed. It concerns SAT (Smart Autotune Thermostat, the firmware's own heating controller), the PIC (Peripheral Interface Controller, the Microchip microcontroller that runs Schelte Bron's gateway firmware) and the OTGW (OpenTherm Gateway) Classic board. The option it records (Option A below) was chosen by the maintainer, Robert van den Breemen, on 2026-11-07 (TASK-1695). Acceptance is a separate step and needs Robert's explicit go.

Binding per ADR-080 once Accepted. The gate is the host test `tests/test_bridge25238_sat_filter.py` (named in the front matter `gate` field). The test exists (written with the implementation under TASK-1695): it compiles the real `handlePICSerial()` and fails on the code from before this ADR (12 of 15 cases) and passes on the implementation. It must pass before this ADR is accepted (see Decision Contract, Verification).

Citations are to the working tree on 2026-11-07 on top of the `dev` branch commit `22fa148bc`, which carries uncommitted TASK-1695 changes in `OTGW-Core.ino`, `OTDirect.ino` and `OTGW-firmware.h`. Paths are relative to `src/OTGW-firmware/`. Line numbers can shift when that work is committed.

## Status History

```yaml
status_history:
  - date: 2026-11-07
    status: Proposed
    changed_by: Robert van den Breemen
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-07
    status: Proposed
    changed_by: Robert van den Breemen
    reason: Related to ADR-177
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Proposed
    changed_by: Robert van den Breemen
    reason: Related to ADR-179
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Proposed
    changed_by: Robert van den Breemen
    reason: Related to ADR-203
    changed_via: adr-kit lifecycle
```

## Context

**The rule.** On 2026-11-07 Robert set a rule (TASK-1695): while SAT is enabled, no channel other than SAT may send the CV (central heating, Dutch "centrale verwarming") control commands `CS=`, `CH=`, `MM=`, `C2=`, `H2=`, `TT=` and `TC=`. `HW=`, `SW=`, `SH=` and every other PIC command stay free. "Enabled" is `satOwnsControlSetpoint()` (`SATcontrol.ino:3823`), which returns `satIsEnabled()`, that is `settings.sat.bEnabled || state.sat.bFallbackActive` (`SATcontrol.ino:3817`). With SAT disabled, MQTT (Message Queuing Telemetry Transport), REST (Representational State Transfer, the HTTP application programming interface) and the serial bridge (Home Assistant integrations, OTmonitor) control the boiler as before, also in OT-Direct (the PIC-less OpenTherm master mode) master mode. The command set is the predicate `satBlocksExternalCommand()` (`OTGW-firmware.h:415`).

**Where the rule is already enforced.** The PIC command queue refuses an external blocked command in `addCommandToQueue()` (`OTGW-Core.ino:3883-3891`), and the OT-Direct command handler does the same in `handleOTDirectCommand()` (`OTDirect.ino:2565-2573`). ADR-179 started this for `CS=` only. The OT-Direct port 25238 bridge is already line based (128-byte buffer, `OTDirect.ino:686`) and hands each complete line to `sendPICSerial()` (`OTDirect.ino:717`), which reaches the handler, so the OT-Direct bridge is covered without this ADR.

**The gap.** On the OTGW Classic (the board with a PIC), the inbound port 25238 bridge forwards every byte to the PIC at once: it reads one byte (`OTGW-Core.ino:6388`) and calls `enqueuePICTx(&outByte, 1)` (`OTGW-Core.ino:6394`) before it knows what line the byte belongs to. The line in `sWrite` (128 bytes, `MAX_BUFFER_WRITE`, `OTGW-Core.h:48`) is assembled only for logging, for the ser2net queue override of ADR-059 and for the `GW=R`/`PS=` side effects (`OTGW-Core.ino:6396-6440`). The queue comment at `OTGW-Core.ino:3883-3886` states that these bytes "do not pass this queue and stay unfiltered". An OTmonitor user can therefore send `CS=60` while SAT is enabled and the PIC applies it; SAT only wins back within its 5 s CS (control setpoint) resend (ADR-203, kept for CS by ADR-207).

**The conflicting constraint.** On 2026-09-26 Robert decided that the client-to-PIC direction of port 25238 must be 100% byte transparent. A binary PIC upgrade from OTmonitor over 25238 contains CR (carriage return, 0x0D) bytes, and any design that treats CR specially can corrupt the stream and brick the PIC. Flashing over 25238 is discouraged but supported. ADR-177 (binding) keeps the PIC byte stream verbatim in the other direction, from the serial task to every consumer. Filtering text commands on the inbound side needs a line boundary, so the two decisions conflict. Robert was shown the conflict and chose Option A on 2026-11-07 (AskUserQuestion, recorded in the TASK-1695 implementation notes).

## Decision

While SAT is enabled, the Classic port 25238 inbound bridge buffers each line up to CR, forwards an allowed text line byte-identical, and drops a line that is a blocked CV control command or that contains any non-text byte; while SAT is disabled the bridge stays 100% byte transparent as today.

**SAT disabled (unchanged).** Every byte goes to `enqueuePICTx()` as it arrives, with no buffering and no decision on byte values. A PIC upgrade through OTmonitor works as it does today.

**SAT enabled (new).** The bridge collects bytes in a bounded line buffer (the existing 128-byte `sWrite`) and sends nothing to the PIC until CR arrives. At CR it decides once for the whole line:

1. If any byte of the line is outside printable ASCII (American Standard Code for Information Interchange, 0x20 to 0x7E), CR or LF (line feed, 0x0A), the whole line is dropped. The PIC never receives a partial binary stream.
2. Else, if `satBlocksExternalCommand()` is true for the line, the line is dropped.
3. Else, the line goes to the PIC byte-identical, CR included, and the existing logging, ser2net override (ADR-059) and `GW=R`/`PS=` handling run as today.

A line longer than the buffer is dropped whole: once it overflows, the rest of that line up to the next CR is discarded. Every dropped line is logged on the debug telnet port with its reason (blocked command, non-text byte, overflow) and counted.

**CR LF.** Clients such as pyotgw (Home Assistant) end a command with CR LF; OTmonitor sends CR only; the PIC ends a command on CR and ignores LF. An LF at a line start is therefore forwarded at once and neither starts a line nor samples the mode, so the command after a CR LF is judged in the mode that holds at its own first byte.

**Per-connection latches.** Two latches end at the disconnect (or at a new connection, which the bridge sees through the telnet library's connect callback, also when the disconnect and the reconnect fall between two loop passes). Binary-blocked: when a line is dropped as non-text while SAT is enabled, every further inbound byte of that connection is dropped (counted and logged once), because the printable fragments between the CR bytes of a binary stream would otherwise be forwarded as commands, `GW=R` included. Transparent: when a non-text byte passes while SAT is disabled, the connection stays transparent until it disconnects, also if SAT turns on meanwhile (for example through the MQTT fallback). The transparent latch is the one exception to filtering while SAT is enabled: cutting a binary upgrade midway risks a bricked PIC, which weighs more than an external command that SAT's 5 s CS resend (ADR-203) corrects.

**Queue full.** If `enqueuePICTx()` fails for a held line, the line counts as dropped and none of its side effects run (WebSocket echo, ADR-059 override, readout trigger, `GW=R`/`PS=` handling), because the PIC never received it.

**Mode change.** The mode (transparent or filtered) is sampled at a line boundary: at the first byte after a CR (an LF does not count), and at a client connect. A change of `satOwnsControlSetpoint()` in the middle of a line takes effect at the next line. This keeps a line from being half forwarded and half held.

**Scope.** This ADR covers only the inbound direction of the Classic port 25238 bridge. It does not change ADR-177 (the PIC to consumer direction stays verbatim in both modes), the OT-Direct bridge (already line based) or the command set (owned by TASK-1695 and `satBlocksExternalCommand()`).

## Decision Contract

### Must

- With `satOwnsControlSetpoint()` false, forward every inbound byte to `enqueuePICTx()` unchanged and without delay, CR bytes inside binary data included.
- With `satOwnsControlSetpoint()` true, forward nothing until CR, then forward an allowed line byte-identical, including its CR.
- Drop a whole line that contains a byte outside 0x20 to 0x7E, CR and LF while SAT is enabled.
- Drop a line for which `satBlocksExternalCommand()` is true while SAT is enabled, and do not run the ser2net queue override of ADR-059 for it, so a dropped line cannot remove SAT's own queued command.
- Drop a line longer than the bounded buffer whole, up to the next CR.
- Log each dropped line with its reason and count drops.
- Sample the mode only at a line boundary, and treat an LF at a line start as no boundary: forward it at once.
- Keep a connection transparent from its first non-text byte sent while SAT is disabled until it disconnects.
- Drop every byte of a connection after one of its lines was dropped as non-text, until it disconnects.
- Forget all per-connection state at a disconnect and at a new connection, also one that falls between two loop passes.
- Count a line as dropped, and skip its echo, override, trigger and `GW=R`/`PS=` handling, when `enqueuePICTx()` fails for it.

### Must Not

- Must not buffer, reframe or inspect byte values in the inbound path while SAT is disabled.
- Must not forward part of a line while SAT is enabled.
- Must not change the PIC to consumer direction (ADR-177).
- Must not grow the line buffer without bound or allocate on the heap per line.

### Exceptions

- `state.debug.bOTGWSimulation` keeps its current behaviour (bytes are not sent to the PIC in simulation mode).
- The OT-Direct bridge is out of scope: it is line based and its lines reach `handleOTDirectCommand()`, which already applies the rule.

### Verification

- Host test `tests/test_bridge25238_sat_filter.py` on the real Classic bridge code (not a re-implementation, per the project's reproduce-then-prove rule):
  - With SAT enabled, the line `CS=60\r` reaches the PIC transmit queue with zero bytes. The test is red on the current byte-forwarding code and green on the fix.
  - With SAT enabled, a line that contains a non-text byte (for example 0x00 or 0xFF) reaches the PIC with zero bytes.
  - With SAT enabled, an allowed line such as `HW=1\r` reaches the PIC byte-identical.
  - With SAT disabled, a binary stream with CR bytes in the middle reaches the PIC byte-identical, in order and without waiting for a line end.
  - CR LF commands: with SAT enabled after `HW=1\r\n`, `CH=0\r\n` is refused; the mirror (SAT enabled for `HW=1\r\n`, then disabled) forwards the next CR LF command.
  - A binary blob with printable fragments between CR bytes reaches the PIC with zero bytes while SAT is enabled and triggers no `GW=R`; the latch ends at a new connection.
  - Binary data sent while SAT is disabled stays byte-identical across a SAT enable; a new connection filters again.
  - A reconnect between two loop passes drops the old partial line (`X`, reconnect, SAT on, `CS=60\r` forwards no `CS=60`).
  - A failing `enqueuePICTx()` counts a drop and runs no echo, override, trigger or reset.
- `python evaluate.py --quick` reports no new failing check.

## Alternatives Considered

- **Option A: filter per line while SAT is enabled, transparent while disabled (chosen).** It enforces the TASK-1695 rule on the last open channel and keeps the 2026-09-26 transparency rule for the case it protects: a PIC upgrade, which then needs SAT disabled.
- **Option B: keep port 25238 fully transparent and let SAT overwrite external commands within its 5 s CS resend (ADR-203, ADR-207).** Rejected. It leaves the rule unenforced on one channel: an external `CS=` controls the boiler for up to 5 s, and `MM=`, `CH=`, `C2=`, `H2=`, `TT=`, `TC=` are not corrected by the 5 s resend at all (ADR-207 sends MM and CH only on change). It is the race between two writers that ADR-179 and Robert's 2026-09-22 decision ("a visible refusal beats a race") rejected for the other channels.
- **Option C: refuse all inbound port 25238 bytes while SAT is enabled.** Rejected. It also blocks `HW=`, `SW=`, `SH=`, `PR=` and every read command, which the rule explicitly leaves free, and it breaks OTmonitor as a monitoring and diagnosis tool for SAT users. It buys no extra safety over Option A, which already drops binary data.

## Consequences

**Positive:**

- The rule "while SAT is enabled only SAT sends CV control commands" holds on every channel: MQTT, REST, the OT-Direct bridge and the Classic bridge.
- A partial binary stream cannot reach the PIC while SAT is enabled: a binary line is dropped whole, never cut at a CR.
- OTmonitor text commands other than the seven blocked ones keep working while SAT is enabled.
- Behaviour with SAT disabled, including a PIC upgrade over 25238, is unchanged.
- The cost is small: the 128-byte `sWrite` buffer already exists, so the change adds one drop counter and a per-line scan of at most 128 bytes.

**Negative:**

- While SAT is enabled, OTmonitor text commands reach the PIC only after CR. This is normal for line commands, which are terminated by CR, but it is a change. *Mitigation:* documented here and in the 25238 user documentation.
- A PIC upgrade over port 25238 is impossible while SAT is enabled. This is by design and safe: the upgrade data is dropped, the PIC never receives part of it. *Mitigation:* the drop log names the reason, and the documentation says to disable SAT before an upgrade.
- A connection that sent a non-text byte while SAT was disabled stays transparent until it disconnects, so an external `CS=` or `CH=` on that same connection reaches the PIC even after SAT turns on. *Mitigation:* accepted; only a binary client triggers it, the alternative (cutting an upgrade midway) can brick the PIC, and SAT's 5 s CS resend (ADR-203) corrects a `CS=`.
- After a line is dropped as non-text, the rest of that connection is dropped, including later valid text lines. *Mitigation:* a client reconnects to recover; the drop is counted and logged once.
- A mode change applies only at the next line boundary, so a line that started while SAT was disabled goes through transparent even if SAT is enabled before its CR. *Mitigation:* accepted; SAT's 5 s CS resend (ADR-203) corrects such a single line.
- A text line longer than 128 bytes is dropped whole while SAT is enabled. No PIC command is that long: the command queue holds at most 15 characters per command (`PIC_CMD_MAX_LEN`, `OTGW-Core.h:767`). *Mitigation:* the drop is logged and counted.
- The 2026-09-26 "100% byte transparent" rule now holds only while SAT is disabled. *Mitigation:* this ADR records the narrowing and the reason, so a later reader does not see a contradiction.

## Open Questions

- None.

## Related Decisions

- ADR-177: Keep the PIC byte stream verbatim from the serial task to every consumer. Complements it: ADR-177 covers the PIC to consumer direction, this ADR the client to PIC direction, which ADR-177 leaves unfiltered.
- ADR-179: Name the owner of the OpenTherm control setpoint while SAT is enabled. This ADR extends its ownership rule to the Classic port 25238 bridge.
- ADR-059: Ser2net Queue Awareness and Serial Bus Coordination. Its queue override must not run for a dropped line.
- ADR-203: SAT sends CS MM and CH on every control update (MM is the maximum relative modulation command, CH the central heating enable command). Its 5 s CS resend is the fallback rejected as the only defence in Option B.
- ADR-207: SAT sends MM, the Immergas TP and CH on change, CS keeps the 5 s resend. Explains why MM and CH have no 5 s resend.
- ADR-080: Binding ADR rules must have a CI (continuous integration) gate. Requires the host test gate named in the Status section.

## References

- `backlog/tasks/task-1695 - While-SAT-is-enabled-refuse-external-CV-control-commands-on-every-channel-MQTT-REST-serial-port-25238.md`: the rule, acceptance criterion #4, and Robert's Option A decision of 2026-11-07 in the implementation notes.
- `src/OTGW-firmware/SATcontrol.ino:3814-3830`: `satIsEnabled()` and `satOwnsControlSetpoint()`.
- `src/OTGW-firmware/OTGW-firmware.h:410-425`: `satBlocksExternalCommand()`, the blocked command set.
- `src/OTGW-firmware/OTGW-Core.ino:3883-3891`: the queue path refusal and the comment that port 25238 bytes bypass it.
- `src/OTGW-firmware/OTGW-Core.ino:6375-6440`: the Classic inbound bridge (`enqueuePICTx(&outByte, 1)` at line 6394).
- `src/OTGW-firmware/OTGW-Core.h:48`: `MAX_BUFFER_WRITE 128`.
- `src/OTGW-firmware/OTDirect.ino:686-720`: the line-based OT-Direct bridge.
- `src/OTGW-firmware/OTDirect.ino:2565-2573`: the OT-Direct handler refusal.
- Maintainer decision 2026-09-26 (client to PIC 100% byte transparent; flashing over 25238 discouraged but supported), recorded in the project memory note `feedback_port25238_byte_transparent_pic_flash.md` with its 2026-11-07 update.

## Enforcement

The gate is the host test named in the Status section. The LLM (large language model) judge reviews changes to the Classic bridge file against the Decision Contract.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "satOwnsControlSetpoint\\(\\)",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "The Classic port 25238 bridge must switch between transparent and per-line filtered mode on satOwnsControlSetpoint() (ADR-217)."
    }
  ]
}
```
