# Daily Issue Report — 2026-05-06

**Generated**: 2026-05-06
**Period checked**: Since 2026-05-03T07:23:09Z (last check timestamp)

---

## Sources Checked

| Source | Status | Result |
|--------|--------|--------|
| GitHub Issues | ✅ Scanned | 3 issues updated/created since last check |
| Tweakers forum | ❌ Blocked | Host not in allowlist in this environment |
| Discord | ❌ Unavailable | Discord MCP server not responding (localhost:8085) |

---

## Findings

### 1. Bug — Services unreachable after WiFi reconnect (GitHub #560)

- **Source:** GitHub [#560](https://github.com/rvdbreemen/OTGW-firmware/issues/560)
- **Reporter:** andrebrait
- **Created:** 2026-05-04
- **Classification:** Bug report
- **Backlog task:** TASK-547 (new, created this run)

**Summary:** After a reboot the device reconnects to WiFi, gets an IP address, and is pingable — but telnet, curl, and the webUI are all unreachable. Forcing a reconnect through the UniFi dashboard immediately restores access. The reporter provided detailed analysis pointing to `networkStuff.ino`'s `WIFI_RECONNECTED` handler: services (`startTelnet`, `startOTGWstream`, etc.) are restarted without first stopping their previous instances, leaving stale port bindings that block the new servers from accepting connections.

**Proposed fix (from reporter):** Before restarting services in the `WIFI_RECONNECTED` handler, call `debugTelnet.stop()` and `OTGWstream.stop()` to release lingering port bindings.

**Information readiness:** Sufficient — clear reproduction steps, code area identified, active reporter.

---

### 2. Feature request — Static IP address settings (GitHub #561)

- **Source:** GitHub [#561](https://github.com/rvdbreemen/OTGW-firmware/issues/561)
- **Reporter:** andrebrait
- **Created:** 2026-05-04
- **Classification:** Feature request
- **Backlog task:** TASK-548 (new, created this run)

**Summary:** Request to add a static IP address configuration option in the firmware. Motivation: if DHCP is unavailable the device loses network access, which can be problematic when the OTGW is part of critical home-automation infrastructure.

---

### 3. Follow-up only — PIC not detected on Wemos D1 Mini Pro (GitHub #557)

- **Source:** GitHub [#557](https://github.com/rvdbreemen/OTGW-firmware/issues/557)
- **Reporter:** dwd1
- **Updated:** 2026-05-05 (maintainer response)
- **Classification:** Question / hardware issue (under investigation)
- **Backlog task:** TASK-486 (existing, status: Done — awaiting reporter logs)

**Summary:** User replaced the Wemos D1 with a D1 Mini Pro and the PIC is no longer detected (`picavailable=false`), and the "Run Boot Command" option disappears. The maintainer responded 2026-05-05 confirming no firmware-side pin mismatch and asked for a 74880-baud boot serial log and 115200-baud firmware boot log to determine if this is a hardware/assembly issue. No new code action required until logs are provided.

---

## Backlog tasks created this run

| Task | Title | Status |
|---|---|---|
| TASK-547 | Fix: Services unreachable after WiFi reconnect (GitHub #560) | To Do |
| TASK-548 | Feature: Static IP address settings (GitHub #561) | To Do |

---

## Timestamps updated

- `.claude/github_last_checked.txt` → `2026-05-06T00:00:00Z`
- `.claude/discord_last_checked.txt` → unchanged (source unavailable)
- `.claude/tweakers_last_checked.txt` → unchanged (source blocked)
