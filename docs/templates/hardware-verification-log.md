# Hardware Verification Log

> Fillable evidence template for hardware-only acceptance criteria. Copy this
> file into `docs/releases/<version>/hardware-verification-<board>.md` (or a
> task-attached path), fill in every row that applies to the release under
> test, and link the completed log from the task's `--final-summary` so the
> AC evidence is part of the permanent record.
>
> Hardware-only ACs cannot be exercised by `python build.py` or `evaluate.py`.
> Without a log, "Done" rests on memory; with one, future regressions have a
> reference point.

---

## 1. Header

| Field | Value |
| --- | --- |
| Release tag | `v<x.y.z>` |
| Board under test | `OTGW32-S3 (ESP32-S3, 4 MB flash)` / `NodeMCU ESP8266` / `both` |
| Firmware bin SHA-256 | `<paste from SHA256SUMS.txt>` |
| Filesystem image SHA-256 | `<paste from SHA256SUMS.txt>` |
| Tester name | `<full name>` |
| Tester Discord handle | `<@discord-handle>` |
| Date (ISO 8601) | `YYYY-MM-DD` |
| Git commit at verification | `<short-sha>` on `<branch>` |

---

## 2. Test environment

- Power source: `<USB 5 V / OT-bus parasitic / external 5 V PSU>`
- OpenTherm boiler model: `<e.g. Intergas HRE 36/30, Remeha Tzerra Ace>`
- Thermostat type: `<e.g. Honeywell T87RF / Nefit ModuLine / SAT internal / none>`
- Network setup: `<WiFi-only / Ethernet-only (W5500) / WiFi+Ethernet failover>`
- MQTT broker: `<Mosquitto x.y on host / HA add-on>`
- Home Assistant version: `<core x.y.z>`
- BLE sensor models in range (OTGW32 only): `<e.g. ATC LYWSD03MMC x3, Xiaomi BTHome v2 x1>`

---

## 3. Acceptance-criteria matrix

Mark each row Pass / Fail / N/A. A row marked N/A must include a brief reason
(e.g. "OTGW32 only — board under test is ESP8266"). Evidence cell must point
to a concrete artefact (telnet log path, screenshot file, MQTT topic dump,
Discord message URL, REST capture). "Looked OK" is not evidence.

| AC ID | Description | Pass / Fail | Evidence | Notes |
| --- | --- | --- | --- | --- |
| `AC-BLE-1` | BLE sensor discovery time after boot (target < 30 s) | | telnet line `BLE sensor MAC=<mac> discovered` with timestamp | OTGW32 only |
| `AC-BLE-2` | BLE sensor MAC filter: configured-MAC strict, empty-MAC default-allow | | MQTT capture: only allow-listed MACs publish; empty-list run shows all in-range MACs | Test both filter modes |
| `AC-BLE-3` | BLE sensor MQTT discovery: per-MAC topics + HA auto-discovery configs | | `mosquitto_sub -t '<TopTopic>/sat/ble/+/+'` and `homeassistant/sensor/+/+/config` capture | TASK-488 / ADR-092 |
| `AC-OTD-1` | OTDirect queue high-water under stress stays < 12 | | telnet `OTD queue HWM=` line under 60 s mixed TT/TC/CC at 1 Hz from MQTT+REST | TASK-494 |
| `AC-OTD-2` | OTDirect TT/TC override apply + auto-clear cycle | | telnet log of TT command, MsgID 16+100 bus traffic, auto-clear release | TASK-466 fixture |
| `AC-MQTT-1` | MQTT HA auto-discovery for all entity classes (sensor, binary_sensor, climate, number, select, switch) | | HA UI device-card screenshot + `mosquitto_sub -t 'homeassistant/#'` capture; entity counts per class | Compare entity count to previous release |
| `AC-OTA-1` | OTA upload success + boot-loop survival | | `/api/v2/info` JSON before/after; build hash visible in Web UI footer; ≥ 3 consecutive clean reboots | Note OTA partition slot used |
| `AC-TELNET-1` | 24 h soak: no `OOM`, no `Exception`, no watchdog reset in telnet log | | Captured telnet log file path + `grep -E 'OOM\|Exception\|wdt'` empty result | Soak under typical load |
| `AC-WEBUI-1` | Web UI: index page renders, log streams in browser, components.css applied | | Screenshot of dashboard with live log; DevTools network tab showing `components.css` 200 OK | ADR-091 design-system check |
| `AC-WEBUI-2` | Cross-browser check: Chrome current, Firefox current, Safari current | | Screenshots per browser; DevTools console clean of red errors | Latest + 2 versions back per CLAUDE.md |
| `AC-SETTINGS-1` | Settings round-trip: save → reboot → reload, persists | | `diff` of `/api/v2/settings` pre- and post-reboot (only `version.h`-derived fields differ) | Watch `settings.mqtt.sUniqueid` length |
| `AC-SAT-1` | Multi-zone SAT (if hardware supports): two SAT slaves both update | | MQTT capture of `<TopTopic>/sat/zone/<n>/state` for each zone over a heat cycle | Mark N/A if single-zone hardware |

Add release-specific rows below this line as needed. Do not delete pre-filled
rows — mark them N/A with a one-line reason instead, so the matrix shape
stays comparable across releases.

| `AC-CUSTOM-1` |  |  |  |  |
| `AC-CUSTOM-2` |  |  |  |  |
| `AC-CUSTOM-3` |  |  |  |  |

---

## 4. Field telemetry collected

- Heap min during 24 h soak: `<bytes>` (from `/api/v2/info` `heap.min` or telnet `HEAP min=`)
- BLE sensor count discovered: `<n>` (OTGW32 only; mark N/A on ESP8266)
- MQTT publish rate: `<msg/s>` (broker-side count over 60 s window)
- OT message ID coverage: `<n>` unique IDs seen during soak (telnet `OT MsgID seen` summary)
- `evaluate.py` verdict: `<PASS / WARN / FAIL>` plus violation count
- Build artefact byte-size delta vs previous release: firmware `<+/-N bytes>`, filesystem `<+/-N bytes>`

---

## 5. Sign-off

I have personally executed every Pass-marked row above on the hardware
described in the header. Evidence linked in this log is real, dated, and
attributable to this release.

| Role | Name | Signature / commit | Date |
| --- | --- | --- | --- |
| Tester |  |  |  |
| Project owner |  |  |  |

---

## 6. Post-merge actions checklist

After a green sign-off, the project owner runs through these:

- [ ] Tag release on `dev` / `main`
- [ ] Generate `SHA256SUMS.txt` with `tools/generate_release_sha256sums.py`
- [ ] Upload artefacts + `SHA256SUMS.txt` to GitHub release page
- [ ] Update `RELEASE_GITHUB_<x.y.z>.md` with key changes
- [ ] Post `#beta-testing` announcement (technical changelog only, per `feedback_beta_announcements_facts_only`)
- [ ] Merge `dev` → `main` if release is final, OR cherry-pick hotfixes back to `dev` if alpha/beta
- [ ] Close associated `TASK-*` in backlog with `--final-summary` linking to this filled-in log

---

## 7. Footer

Context and rationale:

- ADR-080 (binding ADRs require CI gates) — hardware-only ACs are the residual
  set this template covers, since they cannot be expressed as a CI gate.
- `.full-review/06-followup-plan.md` — origin of TASK-501 4B-M5 and the
  broader release-discipline follow-up work.
