---
id: "ADR-201"
title: "Combo board detection persists only a positive PIC verdict and re-probes every boot (supersedes ADR-164)"
status: "Proposed"
date: "2026-10-25"
binding: false
gate: null
documents_shipped: true
verified_in:
  - "commit:2a820ac11"
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "board-detection"
  - "combo"
  - "pic"
  - "otdirect"
  - "boot"
  - "board-mode"
aliases:
  - "combo board detection"
  - "board mode persistence"
  - "iBoardMode auto detection"
  - "PIC probe at boot"
  - "asymmetric persist"
components:
  - "Combo hardware-mode resolution"
symbols:
  - "detectPIC"
  - "isPICEnabled"
  - "initOTDirect"
  - "applyResolvedComboPins"
  - "comboActivePinMap"
  - "activePicRst"
  - "iBoardMode"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-201 Combo board detection persists only a positive PIC verdict and re-probes every boot (supersedes ADR-164)

## Status

Proposed, 2026-10-25.

This ADR (architecture decision record) is the proposed successor to ADR-164 ("Combo AUTO board detection persists its PIC verdict once (supersedes ADR-160)", Accepted). ADR-164 stays Accepted and its file is untouched while this ADR is Proposed. The `supersedes` list in the front matter is therefore empty: adr-kit writes both sides of a supersession at once (`adr supersede ADR-164 --by ADR-201`) and that runs on acceptance, which needs the maintainer's explicit OK (TASK-1400 AC #2).

Guideline-level per ADR-080: no `evaluate.py` gate and no host test checks this rule (a search of `tests/` and `evaluate.py` for `iBoardMode`, `picFound` and `TASK-1031` found nothing on 2026-10-25). It is verified by code review and by the on-device boot-detect record (see Decision Contract, Verification).

It documents behaviour that already ships: commit `2a820ac11` (TASK-1031, 2026-07-09) changed the code, and ADR-164 was never updated. Citations below are at `dev` HEAD `03b808e7a`; `OTGW-firmware.ino` means `src/OTGW-firmware/OTGW-firmware.ino`.

## Status History

```yaml
status_history:
  - date: 2026-10-25
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal; records the asymmetric persist rule shipped by TASK-1031 (2a820ac11) as the proposed successor to ADR-164 (TASK-1400)
    changed_via: adr-kit
```

## Context

The combo firmware image (ADR-127) runs on two kinds of board: an OTGW Classic carrier with a PIC (a LOLIN S3 Mini or S3 Mini Pro in the Classic socket, ADR-158) and an OTGW32, which has no PIC and talks OpenTherm itself (OT-Direct). The setting `settings.iBoardMode` holds the board mode: `0` auto, `1` Classic on S3 Mini, `2` OT-Direct, `3` Classic on S3 Mini Pro (`settingStuff.ino:667`, range-checked at `settingStuff.ino:867`). The PIC probe `detectPIC()` resets the PIC and waits for the bootloader ETX (end-of-text, 0x03) byte (`OTGW-Core.ino:1035-1059`). The reset is both a pulse on the reset pin and a `GW=R` command on the PIC UART (`OTGWSerial.cpp:931-946`); the PIC firmware executes a `reset` instruction on `GW=R` (`other-projects/otgw-6.6/gateway.asm:4734-4738`). A found ETX sets `HW_MODE_PIC`, a miss sets `HW_MODE_DEGRADED`.

**What ADR-164 decided.** AUTO (`iBoardMode == 0`) detects on first boot with a 3x `detectPIC()` retry and persists any verdict once: `1`/`3` for Classic, `2` for OT-Direct. Later boots take the forced path for that mode and do not re-run AUTO (ADR-164, Decision items 1 and 2).

**Why that broke.** The forced OT-Direct path did not probe the PIC. A transient first-boot miss on a real Classic board was persisted as `2`, and the board stayed in OT-Direct until the user set the mode back to Auto by hand. A tester reported exactly this on an S3 Mini Pro combo (TASK-1031, crashevans, Discord #alpha-testing, 2026-07-09). Commit `2a820ac11` fixed it with an asymmetric rule, which is what the code does today.

**What the code does at HEAD** (read in this session, `#if HAS_RUNTIME_HW_DETECT` block, `OTGW-firmware.ino:375-466`):

- **Auto (`iBoardMode == 0`).** `detectPIC()` runs up to 3 times, with `delay(60)` before each retry: `for (uint8_t attempt = 0; attempt < 3 && !picFound; attempt++)` (`OTGW-firmware.ino:425-429`). A found PIC sets `settings.iBoardMode = state.hw.bClassicPro ? 3 : 1;` (`:431`). A miss closes the PIC UART (`OTGWSerial.end();`, `:444`) and calls `initOTDirect()` for this boot only (`:445`). The write is `if (settings.iBoardMode != 0) writeSettings(false);` (`:450`), so a miss leaves `iBoardMode` at `0` and the next boot probes again.
- **Forced Classic (`1` or `3`).** One `detectPIC()` per boot, no retry (`:409-411`); a dead PIC gives `HW_MODE_DEGRADED` (comment at `:411`, set in `OTGW-Core.ino:1056`). The mode is not changed.
- **Forced OT-Direct (`2`).** One recovery probe per boot, no retry (`:387-390`). A found PIC corrects and persists Classic: `settings.iBoardMode = state.hw.bClassicPro ? 3 : 1;` then `writeSettings(false);` (`:392-393`). No PIC: `OTGWSerial.end()` then `initOTDirect()` (`:397-398`).
- **Second positive-only persist site.** When a PIC boot banner arrives at run time while the PIC was marked unavailable and OT-Direct is not active (`OTGW-Core.ino:5637`), the firmware re-enables the PIC and, only if `settings.iBoardMode == 0`, persists `3` or `1` (`OTGW-Core.ino:5647-5650`). It never persists `2`.
- After any of these, `applyResolvedComboPins()` re-pins I2C, the LEDs and the OLED to the resolved pin map (`OTGW-firmware.ino:456`), and the result is logged to the USB console (`log_e("[combo] detect: ...")`, `:461-463`) and to `/bootdetect.log` (`appendBootDetectLog()`, `:466`).

**Why the asymmetry fits the hardware.** A PIC is a socketed chip on the Classic carrier; a PIC that answered once is very likely still there. A miss, by contrast, can be transient: the code itself retries "to let the PIC settle after the prior reset / IMU-probe transient" (`OTGW-firmware.ino:426`), because the early `probeProImu()` drives the S3 Mini Pro I2C pins 11/12 (`OTGW-firmware.ino:220-239`, called at `:280`), and GPIO (general-purpose input/output) 12 is the PIC reset line on a plain S3 Mini (`boards.h:264`, `PIN_PIC_RST 12`). A false negative must therefore heal on the next boot. A false positive (a stray 0x03 byte read as ETX on a board without a PIC) has not been observed, but it is not ruled out either: `find(ETX)` accepts any 0x03 byte within its timeout. Its cost is listed under Consequences. On a genuine OTGW32 the probe drives only the PIC pins, which are re-initialised afterwards (ADR-125, "Safety of the probe"), and the UART is closed before OT-Direct starts (`OTGW-firmware.ino:397`, `:444`), so the repeated probe is harmless there.

**Maintainer endorsement.** On 2026-10-25, in session, Robert van den Breemen said about this asymmetric design: "Dit vind ik een goed idee" ("I think this is a good idea"). This ADR records that endorsement as the reason to propose it; acceptance still needs his explicit OK.

**Field observation, 2026-10-25 (TASK-1399, open).** A bench S3 board with a PIC on its carrier ran the combo image (alpha.499 to alpha.505) all day as OT-Direct. Under the rule above, every boot of that unit ran at least one PIC probe (three if `iBoardMode` was `0`, one if it was `2`) and every probe missed. That points to a persistent cause (seating, PIC power, or a detection defect), not to a stale persisted verdict. TASK-1399 has not named the cause yet.

## Decision

On the combo board, boot detection persists only a positive PIC verdict, and every boot that is not already a persisted Classic board probes for a PIC again.

1. **Auto (`0`)** probes up to 3 times per boot. A found PIC is persisted as `1` (S3 Mini) or `3` (S3 Mini Pro). A miss runs OT-Direct for that boot only and is never written to `iBoardMode`.
2. **Forced Classic (`1`/`3`)** probes once per boot and stays Classic; a dead PIC runs degraded, it does not fall back to OT-Direct.
3. **Forced OT-Direct (`2`)** probes once per boot as a recovery check. A PIC that answers means OT-Direct is wrong for that board, so the mode is corrected and persisted to `1`/`3`.
4. **The only automatic writes of `iBoardMode`** are the positive Classic verdicts in items 1 and 3 and the banner safety net (`OTGW-Core.ino:5647-5650`). Nothing in the firmware writes `2` on its own; `2` comes only from the user.

This replaces ADR-164 Decision item 2 ("persist once", `2` included, no re-detection). It keeps ADR-164 item 1 (3x retry in auto) and item 3 (manual override honoured; `iBoardMode` is a soft, user-visible setting), and it keeps ADR-158's `iBoardMode` values and ADR-127's single combo binary. Fixed `esp32` and `esp32-classic` builds do no runtime detection and are out of scope (`#else` branch at `OTGW-firmware.ino:467-469`).

## Decision Contract

### Must

- Persist a detection verdict into `settings.iBoardMode` only when a PIC was found, and then only as `1` or `3` (`OTGW-firmware.ino:431,450`, `:392-393`, `OTGW-Core.ino:5647-5650`).
- Run the PIC probe on every boot in auto mode and in forced OT-Direct mode.
- Close the PIC UART (`OTGWSerial.end()`) before `initOTDirect()` whenever no PIC was found (`OTGW-firmware.ino:397,444`).

### Must Not

- Must not write `iBoardMode = 2` from any detection path.
- Must not skip the PIC probe on a board whose `iBoardMode` is `2`.

### Exceptions

- None. A user can still set any mode by hand; this ADR governs only what the firmware writes on its own.

### Verification

- Code review of the `#if HAS_RUNTIME_HW_DETECT` block at `OTGW-firmware.ino:375-466` and of `OTGW-Core.ino:5647-5650` against the Must and Must Not lists.
- On device: the `[combo] detect: eMode=... picEnabled=... boardMode=...` line on the USB console (`OTGW-firmware.ino:461-463`) and the same record in `/bootdetect.log` (`:466`). Expected: a Classic board shows `boardMode` 1 or 3 after its first PIC hit; an OTGW32 in auto shows `boardMode=0` on every boot.
- Bench, 2026-07-09 (TASK-1031 notes, alpha.341, the maintainer's S3 Mini Pro combo): `boardmode=2` forced on a board with a live PIC corrected itself to `boardmode=3`, PIC mode, on the next boot. That exercised the forced OT-Direct recovery path on a Pro; the auto no-persist path was checked by code review. External field confirmation by the original reporter is not recorded.
- No automated gate exists (guideline-level per ADR-080).

## Alternatives Considered

- **Never persist, re-probe every boot (ADR-160).** Auto would never write `iBoardMode`. Rejected by history: it was implemented (TASK-947, `c3afa5f4f`), the follow-up for a plain S3 Mini (TASK-949, `7f69f6b91`) moved settings loading to the top of `setup()` and hung the boot on-device, and it was reverted (`d7a34f4ad`), as ADR-164 records. Without a persisted positive verdict, a Classic board also starts every boot on the auto pin map before detection.
- **Persist once, the negative verdict included (ADR-164).** Deterministic after the first boot and no probe cost later. Rejected: a single first-boot miss stranded a live-PIC board in OT-Direct, because the forced OT-Direct path did not probe (TASK-1031 field report on an S3 Mini Pro).
- **Re-detect while running and switch the command path.** Keep probing after boot and move between the PIC and OT-Direct paths live. Not chosen now: with no PIC found the UART is closed so a floating PIC RX pin cannot cause an interrupt storm (`OTGW-firmware.ino:435-436`, ADR-125), so a live probe would have to reopen it under OT-Direct, and the OpenTherm command path would change under a running gateway. The narrow banner safety net (`OTGW-Core.ino:5637-5650`) already covers a PIC that shows up after a degraded boot, and only when OT-Direct is not running.
- **Do nothing (keep ADR-164 as the record).** Rejected: the Accepted record would contradict the shipped code, and a reader following ADR-164 would reintroduce the TASK-1031 stranding.

## Consequences

**Positive:**

- A transient PIC miss heals by itself on the next boot; no manual switch back to Auto is needed (TASK-1031).
- A board persisted as OT-Direct by an old firmware or by mistake corrects itself as soon as its PIC answers (`OTGW-firmware.ino:391-395`). Bench-verified on an S3 Mini Pro (TASK-1031); on a Pro this relies on the `GW=R` command, see the reset-pin item below.
- A found Classic verdict is persisted, so later Classic boots start peripherals on the right pin map before detection (`comboActivePinMap()`, `OTGW-firmware.h:766-770`).

**Negative:**

- A genuine OTGW32 left in auto pays the full 3-try probe on every boot. Estimate from reading the code, not measured: each try holds the PIC reset low for 100 ms (`OTGWSerial.cpp:943`) and `find(ETX)` waits up to the Stream timeout (Arduino default 1000 ms; no `setTimeout` call on `OTGWSerial` was found), so about 3 x 1.1 s + 2 x 60 ms, roughly 3.4 s per boot. Mitigation: the user can set `iBoardMode = 2`, which costs one probe per boot.
- A genuine OTGW32 left in auto also starts its pre-detection peripherals (0x26 watchdog disarm, `initOLED()` at `OTGW-firmware.ino:346`, the WiFi portal at `:357`) on the Classic pin map on every boot, because the auto pin map defaults to Classic until detection (`OTGW-firmware.h:774-775`). Under ADR-164 this happened on the first boot only. It is re-pinned at `:456`. Derived from reading the code, not checked on an OTGW32: the OLED connect screen during the WiFi portal may not show on an OTGW32 in auto.
- `iBoardMode = 0` on an OTGW32 no longer means "never detected"; it means "no PIC seen yet". Tooling or docs that read `boardmode` (`restAPI.ino:3851`) must not treat `0` as unresolved.
- A forced Classic board whose PIC dies runs degraded and never falls back to OT-Direct. This is intended: a Classic carrier has no OT-Direct transceiver.
- A false positive is sticky, the mirror image of the TASK-1031 stranding. If an OTGW32 in auto ever read a stray 0x03 byte as ETX, it would persist Classic, and forced Classic never re-checks towards OT-Direct, so it would run degraded with no OpenTherm until the user changes the mode. The 3-try loop stops at the first hit (`OTGW-firmware.ino:425`), so it gives such a byte up to three chances per boot. Not observed; mitigation is the manual mode setting (`settingStuff.ino:867`).
- On an S3 Mini Pro at `iBoardMode == 2`, the recovery probe pulses the wrong reset pin. `comboActivePinMap()` returns `COMBO_MAP_OTGW32` for mode 2 (`OTGW-firmware.h:768`), so `activePicRst()` returns `PIN_PIC_RST` = GPIO 12 (`OTGW-firmware.h:831-833`, `boards.h:264`), which is the Pro's I2C SDA line (`boards.h:290`); the Pro's PIC reset is GPIO 40 (`boards.h:288`). Recovery then rests on the `GW=R` command alone, which needs a running PIC firmware; the TASK-1031 bench run shows it worked on that Pro. A PIC that only answers to a hardware reset would not be recovered on a Pro from mode 2. Found by reading the code; not reproduced.

## Open Questions

- [ ] Should the forced OT-Direct recovery probe use the Pro reset pin (GPIO 40) when `state.hw.bClassicPro` is set, instead of GPIO 12 (see Consequences)? A code change, so it would be its own backlog task; it does not change this decision.
- [ ] Should a host test or `evaluate.py` gate check the Must Not list (no detection path writes `iBoardMode = 2`), so this ADR can become binding per ADR-080?

## Related Decisions

- **ADR-164 (Combo AUTO board detection persists its PIC verdict once (supersedes ADR-160))**: Supersedes ADR-164 on acceptance. Keeps its 3x retry and manual override; replaces its persist-once rule.
- **ADR-160 (Combo board: AUTO hardware detection re-probes every boot and never persists its verdict)**: Superseded by ADR-164; listed here as the rejected never-persist alternative.
- **ADR-127 (Combo ESP32-S3 single binary revived: runtime PIC/OTDirect boot detection)**: Depends on. This ADR refines its boot-detection cache-back.
- **ADR-158 (Combo board: add the LOLIN S3 Mini Pro as a third boot-detected Classic variant)**: Depends on. Defines `iBoardMode` 3 and the Pro pin map.
- **ADR-125 (Combo ESP32-S3 board: one binary, runtime PIC/OTDirect selection, Superseded)**: Cited for the probe-safety argument and the UART close before OT-Direct.
- **ADR-080 (Binding ADR rules must have a CI gate)**: This ADR is guideline-level under it.

## References

- TASK-1400 (this ADR), TASK-1031 (asymmetric persist fix), TASK-1399 (2026-10-25 bench observation, open), TASK-947 and TASK-949 (ADR-160 history).
- Commits: `2a820ac11` (TASK-1031), `c3afa5f4f`, `7f69f6b91`, `d7a34f4ad` (ADR-160/164 history).
- `src/OTGW-firmware/OTGW-firmware.ino:375-466` (hardware-mode resolution), `:220-239` and `:280` (`probeProImu()`), `:346` and `:357` (pre-detection OLED and WiFi).
- `src/OTGW-firmware/OTGW-Core.ino:1035-1059` (`detectPIC()`), `:5632-5650` (banner safety net).
- `src/OTGW-firmware/OTGW-firmware.h:745-779` (`comboActivePinMap()`), `:831-833` (`activePicRst()`).
- `src/libraries/Platform/src/boards.h:264` (`PIN_PIC_RST 12`), `:288` (`PIN_CLASSIC_PRO_PIC_RST 40`).
- `src/libraries/OTGWSerial/OTGWSerial.cpp:931-946` (`resetPic()`: reset pulse, `GW=R` command, 100 ms).
- `other-projects/otgw-6.6/gateway.asm:4734-4738` (PIC firmware: `GW=R` executes `reset`).
