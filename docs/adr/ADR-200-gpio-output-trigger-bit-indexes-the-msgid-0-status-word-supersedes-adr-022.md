---
id: "ADR-200"
title: "GPIO output trigger bit indexes the MsgID 0 status word (proposed successor to ADR-022)"
status: "Proposed"
date: "2026-10-24"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "gpio"
  - "outputs"
  - "relay"
  - "status-flags"
  - "opentherm"
aliases:
  - "GPIO outputs"
  - "GPIO relay output"
  - "trigger bit"
  - "GPIOOUTPUTStriggerBit"
  - "Statusflags"
components:
  - "GPIO outputs"
symbols:
  - "evalOutputs"
  - "initOutputs"
  - "setOutputState"
  - "validateGPIOOutputsConfig"
  - "OutputsSection"
  - "Statusflags"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-200 GPIO output trigger bit indexes the MsgID 0 status word (proposed successor to ADR-022)

## Status

Proposed, 2026-10-24.

This ADR (architecture decision record) is the proposed successor to ADR-022 (GPIO (general-purpose input/output) Output Control (Bit-Flag Triggered Relays), Accepted). ADR-022 stays Accepted and its file is untouched while this ADR is Proposed. The `supersedes` list in the front matter is therefore empty: adr-kit writes both sides of a supersession at once (`adr supersede ADR-022 --by ADR-200`), and running it now would retire ADR-022 before this decision is accepted; `adr lint --strict` also rejects a one-sided link. That command runs on acceptance, which needs the maintainer's explicit OK (TASK-1304).

Guideline-level per ADR-080: no CI (continuous integration) gate checks the trigger-bit layout.

The 1.x line (`otgw-1.x.x`) carries its own copy of ADR-022 with the same text. This ADR covers the `dev` (2.0.0) line only; the 1.x copy is out of scope here.

## Status History

```yaml
status_history:
  - date: 2026-10-24
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal; proposed successor to ADR-022, which lists the master status bits as trigger bits 0-7 while the code tests the slave byte there (TASK-1295 review, TASK-1304)
    changed_via: adr-kit
```

## Context

The GPIO outputs feature drives one relay pin from one OpenTherm status bit: the pin follows the bit, high when set, low when clear. ADR-022 (2020, documented 2026-01-28) records that design. The TASK-1295 review (2026-10-22) found that ADR-022 names the wrong bits. Each statement below was re-read in the code for this ADR. Firmware citations are at the `dev` branch head (HEAD) `057535397`, paths relative to `src/OTGW-firmware/`.

**What the code tests.** `evalOutputs()` computes `bitState = (OTcurrentSystemState.Statusflags & (1U << settings.outputs.iTriggerBit)) != 0` (`outputs_ext.ino:87`) and writes it to the pin (`outputs_ext.ino:96`, through `setOutputState()` at `:51-60`). `Statusflags` is a `uint16_t` (`OTGW-Core.h:52`) with one layout, written by two paths:

- Live OT bus path: `print_status()` stores `value = (OTcurrentSystemState.MasterStatus<<8) | OTcurrentSystemState.SlaveStatus` (`OTGW-Core.ino:2943`) into its reference parameter, which the caller binds to `OTcurrentSystemState.Statusflags` (`case OT_Statusflags: print_status(OTcurrentSystemState.Statusflags)`, `OTGW-Core.ino:4792`), after `publishMasterStatusState()` set `MasterStatus = valueHB` (`OTGW-Core.ino:2456`) and `publishSlaveStatusState()` set `SlaveStatus = valueLB` (`OTGW-Core.ino:2500`).
- PS=1 summary path: `publishCombinedStatusState()` returns `(OTcurrentSystemState.MasterStatus << 8) | OTcurrentSystemState.SlaveStatus` (`OTGW-Core.ino:2534`), assigned to `Statusflags` at `OTGW-Core.ino:4665`.

So bits 0-7 of `Statusflags` are the slave (boiler) status byte and bits 8-15 are the master (thermostat) status byte. Other readers of `Statusflags` in the firmware use the same layout: `isFlameOn()` tests `0x08` as "Slave bit 3" (`OTGW-Core.h:203`), SAT tests `0x08` as flame (`SATcontrol.ino:1537`), `0x02` as CH active (`SATcycles.ino:717`) and `0x04` as DHW active (`SATcycles.ino:926`), and the webhook header states "Bits 0-7: Slave status ... Bits 8-15: Master status (bit 8 = CH enable)" (`webhook.ino:9-11`) for the same test at `webhook.ino:391`.

**What the OpenTherm specification says.** `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`, section 5.3.1 (Class 1: Control and Status Information), lines 1431-1452: MsgID 0 "HB: Master status flag8" with bit 0 CH enable, 1 DHW enable, 2 cooling enable, 3 OTC active, 4 CH2 enable, 5 summer/winter mode, 6 DHW blocking, 7 reserved; "LB: Slave status flag8" with bit 0 fault indication, 1 CH mode, 2 DHW mode, 3 flame status, 4 cooling status, 5 CH2 mode, 6 diagnostic/service indication, 7 electricity production. Section 5.4 (Data-Id Overview Map), line 2341, lists MsgID 0 as "Status flag8 / flag8 Master and Slave Status flags". The spec defines the 16-bit DATA-VALUE as two 8-bit items DATA-BYTE1 and DATA-BYTE2 (section 4.2.5, lines 1083-1086); it does not number them as bits 0-15 of one word (v4.2 removed the MSB-LSB note, line 146). The packing of HB into bits 8-15 is therefore the firmware's, taken from the two code paths above; the meaning of each HB and LB bit is the spec's.

**What ADR-022 gets wrong**, checked against the above:

1. **Trigger bit layout.** ADR-022 lists "OpenTherm status flags available" as bit 0 CH enable through bit 6 DHW blocking (its Context list and its Status Bit Reference table). Those are the master bits, which sit at `Statusflags` bits 8-14. Bits 0-7 are the slave bits.
2. **Default.** ADR-022 says "Bit 0 trigger = CH enable" and uses trigger bit 0 for a circulation pump. The default `iTriggerBit = 0` (`Outputstypes.h:23`) is the slave fault indication. A relay configured per ADR-022's pump example switches on a boiler fault, not on central heating.
3. **Range.** ADR-022 validates the trigger bit as 0-7. The code accepts 0-15: `validateGPIOOutputsConfig()` rejects values outside 0-15 (`outputs_ext.ino:23`), the settings parser constrains to 0-15 (`settingStuff.ino:953`) and the REST settings schema declares 0-15 (`restAPI.ino:3866`).
4. **Enable switch.** ADR-022 disables the output with pin 0. The code has a separate `bEnabled` flag, default false (`Outputstypes.h:21`, checked at `outputs_ext.ino:33`, `:52`, `:63`), the default pin is 16 (`Outputstypes.h:22`) and the pin is validated as 0-16 (`outputs_ext.ino:19`).
5. **Cadence.** ADR-022 calls the update from the OT message handler. The code calls `evalOutputs()` on every main-loop pass (`OTGW-firmware.ino:1160`), so the pin follows the last stored `Statusflags`.
6. **Web UI.** ADR-022 shows a 0-7 dropdown labelled with master bit names. The v2 UI offers bits 0-3 only, labelled with the slave meanings Fault, CH active, DHW active and Flame (`data/v2.js:3035-3037`). Bits 4-15 can be set only through the settings file or the REST settings endpoint.
7. **Platform.** ADR-022's code samples and safety notes name the ESP8266 and use `settingGPIOOUTPUTSpin`/`settingGPIOOUTPUTStriggerBit`. The `dev` line is ESP32-S3 only (CLAUDE.md, Branches) and the settings live in `settings.outputs` (`Outputstypes.h:20-24`, ADR-051).

## Decision

The GPIO output follows one bit of the 16-bit MsgID 0 status word `Statusflags = (MasterStatus << 8) | SlaveStatus` (`OTGW-Core.ino:2534`, `:2943`), tested at `outputs_ext.ino:87`. The setting `GPIOOUTPUTStriggerBit` (`settings.outputs.iTriggerBit`) selects that bit, valid range 0-15.

**Trigger bit reference** (meaning from the spec, section 5.3.1, lines 1431-1452; position from the firmware):

| Trigger bit | Byte | Spec bit | Meaning |
|---|---|---|---|
| 0 | slave (LB) | 0 | Fault indication (the default) |
| 1 | slave (LB) | 1 | CH mode (central heating active) |
| 2 | slave (LB) | 2 | DHW mode (hot water active) |
| 3 | slave (LB) | 3 | Flame status |
| 4 | slave (LB) | 4 | Cooling status |
| 5 | slave (LB) | 5 | CH2 mode |
| 6 | slave (LB) | 6 | Diagnostic/service indication |
| 7 | slave (LB) | 7 | Electricity production |
| 8 | master (HB) | 0 | CH enable |
| 9 | master (HB) | 1 | DHW enable |
| 10 | master (HB) | 2 | Cooling enable |
| 11 | master (HB) | 3 | OTC active |
| 12 | master (HB) | 4 | CH2 enable |
| 13 | master (HB) | 5 | Summer/winter mode |
| 14 | master (HB) | 6 | DHW blocking |
| 15 | master (HB) | 7 | Reserved |

**Typical configurations**, replacing ADR-022's: a circulation pump on central heating uses 1 (boiler reports CH active) or 8 (thermostat requests CH); a DHW valve uses 2 (DHW active) or 9 (DHW enable); a second-zone valve uses 5 (CH2 mode) or 12 (CH2 enable); a fault lamp uses 0.

**Kept from ADR-022** (still true in the code): one output pin; stateless, the pin is recomputed from the stored bit every time (`outputs_ext.ino:62-97`); binary on/off; no inversion and no combination of bits; pin and bit validated before every write (`outputs_ext.ino:18-28`, called at `:34`, `:53`, `:64`); the reasons ADR-022 gives for rejecting temperature thresholds, multiple outputs, PWM, MQTT-driven control and scripting; and its relay safety notes (drive a relay module, never mains voltage on a GPIO pin), which apply to the ESP32-S3 as they did to the ESP8266.

The webhook trigger bit uses the same layout and the same test (`webhook.ino:9-11`, `:391`); this table applies to it too.

## Decision Contract

### Must

- Index the trigger bit into `Statusflags = (MasterStatus << 8) | SlaveStatus`: bits 0-7 slave, bits 8-15 master.
- Accept trigger bits 0-15 in the validator, the settings parser and the REST schema.
- Label trigger bits by the table above in any UI, document or support answer.
- Keep the output stateless: recompute the pin from the stored bit on each evaluation.

### Must Not

- Describe trigger bit 0 as CH enable, or bits 0-6 as the master flags.
- Change the bit order of `Statusflags` without superseding this ADR; the GPIO output, the webhook, `isFlameOn()` and SAT all read it.

### Exceptions

- None.

### Verification

- Every `file:line` in this ADR was opened at HEAD `057535397` in this session, and every spec line in `OpenTherm-Protocol-Specification-v4.2.md`.
- No host test drives `evalOutputs()`. The layout claim rests on reading the two writers and the test; it is not covered by an automated test. This ADR claims no gate.

## Alternatives Considered

- **Amend ADR-022 in place (erratum note).** Rejected: ADR-022 is Accepted and immutable, and the maintainer directed on 2026-10-24 that ADRs with wrong values are superseded by new ADRs.
- **Change the code to match ADR-022** (shift the trigger so that bit 0 means CH enable). Rejected: the code layout matches the frame (HB master, LB slave), is shared with the webhook, `isFlameOn()` and SAT, and is what existing user settings were saved against. Flipping it would silently re-wire every configured relay.
- **Restate only the bit table.** Rejected: the re-read found six more statements that do not hold (default meaning, range, enable switch, cadence, UI, platform). A successor that keeps them would be wrong again.

## Consequences

**Benefits**

- The GPIO output record matches the code at `057535397` and the spec, with a citation per value.
- One table serves the GPIO output and the webhook.

**Trade-offs**

- No CI gate. The layout can drift from this text unnoticed. Mitigation: guideline-level per ADR-080; two `Statusflags` readers state the byte order in code comments (`outputs_ext.ino:65-66`, `webhook.ino:9-11`), the writers (`OTGW-Core.ino:2534`, `:2943`) carry no such comment. The bit-name comment next to the GPIO test is partly stale against the spec: it lists master bits 5 and 6 as "reserved" (`outputs_ext.ino:74-75`; spec: summer/winter mode, DHW blocking) and slave bit 7 as "reserved" (`outputs_ext.ino:86`; spec: electricity production). The table in this ADR follows the spec; correcting that comment is a code change outside this ADR.

**Risks and mitigations**

- *Risk*: users who followed ADR-022 or older documentation set trigger bit 0 for a pump; their relay follows the boiler fault flag. *Mitigation*: this ADR and the v2 UI labels (`data/v2.js:3036`) name bit 0 "Fault"; the default itself is an Open Question below.
- *Risk*: the v2 UI cannot select bits 4-15, so master bits such as CH enable (8) are reachable only through the settings file or REST. *Mitigation*: Open Question below.

## Open Questions

- [ ] **Default trigger bit.** The default 0 (`Outputstypes.h:23`) is the fault indication. Whether the default should stay 0 or become 1 (CH active) is a product choice for the maintainer; changing it touches saved settings only where none was saved.
- [ ] **v2 UI range.** The UI lists bits 0-3 (`data/v2.js:3036-3037`) while the firmware accepts 0-15. Whether to offer 4-15 in the UI is open.

## Related Decisions

- **ADR-022 (GPIO Output Control (Bit-Flag Triggered Relays))**: superseded by this ADR on acceptance. Its single stateless binary output, its validation, its rejected alternatives and its safety notes carry over; its bit table, bit-0 = CH enable default, 0-7 range, pin-0 disable, per-message cadence, 0-7 dropdown and ESP8266 code samples are replaced.
- **ADR-051 (Settings/State architecture)**: the `settings.outputs` section that holds the pin, the enable flag and the trigger bit.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level and claims no gate.

## References

- OpenTherm Protocol Specification v4.2, `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`: section 4.2.5 (lines 1083-1086), section 5.3.1 MsgID 0 (lines 1431-1452), section 5.4 (line 2341), revision note line 146.
- Firmware at HEAD `057535397`, `src/OTGW-firmware/`: `outputs_ext.ino:18-28`, `:33`, `:51-60`, `:62-97`, `:87`; `Outputstypes.h:20-24`; `OTGW-Core.h:52`, `:203`; `OTGW-Core.ino:2456`, `:2500`, `:2524-2535`, `:2875-2945`, `:4665`, `:4792`; `OTGW-firmware.ino:1160`; `settingStuff.ino:950-956`; `restAPI.ino:3866`; `webhook.ino:9-11`, `:391`; `SATcontrol.ino:1537`; `SATcycles.ino:717`, `:926`; `data/v2.js:3035-3037`.
- Tasks: TASK-1304 (this supersession), TASK-1295 (review that found the error; corrected `c4-code-sensors.md` and the `outputs_ext.ino` comment).

The decision has no declarative rule a regex could check and no host test, so it has no Enforcement block: it is guideline-level per ADR-080.
