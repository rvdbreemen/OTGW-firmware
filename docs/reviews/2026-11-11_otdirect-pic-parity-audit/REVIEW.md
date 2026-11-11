# OT-Direct against the PIC gateway firmware: behaviour parity audit

Date: 2026-11-11. Task: TASK-1795. Code at `dev` f64aba399.

Ground truth for PIC behaviour: `other-projects/otgw-6.6/gateway.asm` (Schelte Bron, PIC firmware 6.6). Implementation under audit: `src/OTGW-firmware/OTDirect.ino` with `OTDirecttypes.h`, the vendored OpenTherm library (`src/libraries/OpenTherm/src/OpenTherm.cpp`) and the callers in `OTGW-Core.ino`. Spec: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`. Client impact: pyotgw 2.2.3 (pinned by the Home Assistant `opentherm_gw` integration) and OTmonitor 6.6 under `other-projects/`.

This audit builds on [the 2026-04-27 audit](../2026-04-27_otdirect-pic-parity-audit/REVIEW.md). The items fixed since then are listed as refuted in the area reports.

## Method

Four reviewers each took one area and wrote a report with `file:line` citations on both sides. Each report holds its full tables, the bug candidates that survived a skeptic pass, the refuted claims with the reason, and what was not verified:

| Report | Area |
|---|---|
| [A-gateway-relay.md](A-gateway-relay.md) | A: gateway-mode relay and overrides (MsgID 0, 1, 8, 14, 16, 9, 100, 27, 56, 57; T/B/R/A line classification; thermostat loss) |
| [B-command-interface.md](B-command-interface.md) | B: the serial command interface (all 45 PIC commands, PR= letters, the PS=1 summary, error framing, client impact) |
| [CD-scheduling-timing.md](CD-scheduling-timing.md) | C: message scheduling and alternative slots; D: bus timing, error handling and line state |
| [E-boiler-cache.md](E-boiler-cache.md) | E: the boiler value cache and the answers the gateway generates (added after the first three, at the coordinator's request) |

The coordinator then checked the key findings against both sources: the PR=A banner (`OTDirect.ino:3271`), the `MM=-` clear (`:2976`), the mandatory IDs (`:1616-1633` against `gateway.asm:3383`) and the 50 ms slave gate (`OpenTherm.cpp:443`, `sendResponse` `:294`, the ignored return at `OTDirect.ino:2751`). The filing session spot-checked the cited lines of every high-priority task once more before filing.

Nothing was built, flashed or run on a device. Every failure scenario comes from reading code, the spec and the client code. None was reproduced on a bench or seen in a capture.

## Verdict per area

| Area | Verdict | Main findings | Tasks |
|---|---|---|---|
| A. Gateway relay and overrides | The relay path matches the PIC: one boiler frame per thermostat frame, the answer only after the boiler replied, and no own frames while a thermostat is connected. The differences sit in what the gateway rewrites in the thermostat's frames, and in the IDs it may blacklist. | MsgID 0 bits 1, 2, 4, 5 and 6 (HW=, SM=, BW=, CE=, H2=) are not applied in gateway mode, and ADR-208's premise no longer holds. OT=, SW= and SH= depend on alternative slots. The mandatory IDs 1, 3, 14, 17 and 25 can be blacklisted for good. A thermostat disconnect writes a 16 °C flow setback. | TASK-1805, TASK-1803, TASK-1801, TASK-1806, low items in TASK-1804 |
| B. Command interface | All 45 PIC commands have a branch. 13 are the same, 21 differ in low or cosmetic ways, 4 differences are recorded, and 7 carry bug candidates. External clients break on OT-Direct. | The PR=A banner and the PR=T format crash pyotgw, so the HA `opentherm_gw` integration cannot connect; the banner also names the board. `MM=-` writes 0 % and the OT= clear values write a temperature. The PS=1 summary is unusable. | TASK-1797, TASK-1799, TASK-1800, low items in TASK-1804 |
| C. Message scheduling | The 3-strike rule and the slot trigger match. The schedule design (a 100 ms floor instead of a 1 s period, a broader table) is a known design difference. | One failed MsgID 0 in master mode leaves the boiler without frames for 5 to 6 s (the spec allows 1.15 s). The mandatory-ID exemption is missing. MsgID 0 is starved for about 8 s by the slow-poll burst. The fault code is not read on a new fault. | TASK-1802, TASK-1801, low items in TASK-1804 |
| D. Bus timing and errors | The bit timing matches the PIC. The gateway's own answers can be lost, and line errors are invisible. | Answers the gateway makes itself (master mode, UI=, SR=, alternative slots) are refused by the library's 50 ms gate, and the refusal is ignored. A lost thermostat leaves the boiler silent for up to 30 s. No error lines or counters exist. The gap after the boiler's answer can be shorter than the spec's 100 ms. | TASK-1798, TASK-1802, low items in TASK-1804 |
| E. Boiler cache and answer generation | The PIC answers nothing from a value cache: its store only feeds the PS=1 summary. OT-Direct answers from its cache in one place, master mode, and that cache has no age, is not cleared on a mode switch, and also drives the boiler-present flag. | Master mode keeps answering from the cache after the boiler is gone, and the cache survives loopback, GW=R and mode switches. An empty slot gets Unknown-DataID where ADR-066 says Data-Invalid, for up to 60 s after a boot. SR= answers a WRITE with READ_ACK. A Data-Invalid does not reset the strike counter. ADR-224's cache-answered MsgID 28 slot is not in the code yet: it is TASK-1759, on a lane that becomes beta.175 today. Report E lists notes for that implementation. | TASK-1807, TASK-1808, low items in TASK-1804 |

## Filed tasks

All are on milestone 2.0.0 with the labels `otdirect` and `review-2026-11-11`. Each holds the `file:line` evidence on both sides and an acceptance criterion for a host test against the real code that is red on the old code and green on the fix.

| Task | Priority | Finding | Report |
|---|---|---|---|
| TASK-1797 | high | The PR=A banner and the PR=T format break pyotgw (HA `opentherm_gw`); the banner names the board | B: BC1, BC4 |
| TASK-1798 | high | The gateway's own answers are refused by the library's 50 ms slave gate, and the refusal is ignored | CD: Bug 3 |
| TASK-1805 | high | Gateway mode rewrites MsgID 0 bits 1, 2, 4, 5 and 6 like the PIC; a new ADR (ADR-229, reserved) supersedes ADR-208. Robert's decision: "Like the PIC: new ADR" | A: A2/B1 |
| TASK-1799 | medium | Clear values: `MM=-` writes 0 %, and `OT=99` or `OT=-` writes an outside temperature | B: BC9, BC13 |
| TASK-1800 | medium | The PS=1 summary line (separators, port 25238, cadence, the MsgID 71/77 fields) | B: BC2 |
| TASK-1801 | medium | The mandatory Data-IDs 1, 3, 14, 17 and 25 can be blacklisted for good | A: A3/B3; CD: Bug 6 |
| TASK-1802 | medium | 5 to 6 s without frames after one failed MsgID 0; up to 30 s after a thermostat loss | CD: Bug 1, Bug 9 |
| TASK-1803 | medium | OT=, SW= and SH= reach the boiler only in alternative slots | A: A4, A6 (B2) |
| TASK-1806 | medium | Remove the 16 °C flow setback on a thermostat disconnect. Robert's decision: "Like the PIC" | A: A8; CD: D9 |
| TASK-1807 | medium | The boiler cache goes stale: master mode answers from it after the boiler is gone, and it survives loopback, GW=R and mode switches (boiler-present stays true) | E: E-cand 1, E-cand 3 |
| TASK-1808 | medium | Master mode answers an empty cache slot Unknown-DataID instead of Data-Invalid (ADR-066), for up to 60 s after boot | E: E-cand 2 |
| TASK-1804 | low | Bundle of the remaining low candidates from A, B, C, D and E (E: SR= answer type, Data-Invalid and the strike counter) | all |

Deduplicated against the existing tasks and not filed again:
- TASK-1750: the MsgID 100 override flags and OH=.
- TASK-1751: TT=0 in master mode clears a room setpoint.
- TASK-1707: the content and timing of the alternative slots (TASK-1803 and TASK-1798 refer to it).
- TASK-1733: TT= and TC= are answer-only (ADR-223, Done).
- TASK-1759: the ADR-224 cache-answered MsgID 28 slot (in progress on its own lane). TASK-1807 refers to it, and report E holds notes for it.
- TASK-1138: the loopback presence flag. TASK-1807 refers to it.

## Decisions taken by Robert

Two findings needed a choice that only Robert can make. The coordinator put them to him on 2026-11-11 around 07:45:

1. **MsgID 0 bits in gateway mode** (report A, A2). He chose "Like the PIC: new ADR". The work is TASK-1805, and its ADR supersedes ADR-208.
2. **The 16 °C setback on a thermostat disconnect** (report A, A8; report CD, D9). He chose "Like the PIC": the gateway stops its own control and the boiler follows its own OpenTherm timeout. The work is TASK-1806.

Two findings from report E wait for Robert's decision before a task is filed:
- **E-cand 4.** In master mode a TSet written by the thermostat never expires, and a later TSet 0 is ignored. The ignored 0 is documented in BREAKING_CHANGES.md; the missing expiry and the stale MsgID 16/24 writes are not.
- **U1.** In master mode the thermostat's WRITEs of MsgID 56, 57, 14 and some other IDs are acknowledged and then dropped.

## Not verified

- No bench or field capture. Every scenario rests on code, the spec and client code.
- How often boilers answer Unknown-DataID to MsgID 1, 14, 16, 17 or 25. This affects the likelihood of TASK-1801 and of the MsgID 16 item in TASK-1804.
- How a real boiler reacts to 5 to 30 s of silence (TASK-1802).
- Whether the PIC's MsgID 6 write permissions make the SW=/SH= case smaller (TASK-1803).
- Whether SAT reads the outside-temperature state that `OT=` writes (TASK-1799).
