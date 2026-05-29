---
id: TASK-750
title: 'feat(oled): first-setup / AP-mode display — splash + WiFi config instructions'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 08:34'
updated_date: '2026-05-29 08:41'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field feedback (tjfs, alpha.84, OTGW32): nothing useful on the OLED when booting unconfigured. tjfs's OT-Thing port shows a splash + setup instructions (connect to the device SSID, then point a browser at 192.168.4.1). Audit of OLED.ino: there is a boot splash (~line 405) and 5 runtime status pages cycled by the boot button, but NO AP/config-mode screen with setup instructions. Note: OLED + wired ethernet are NodoShop OTGW32-specific (OT-Thing has neither) — this is our own OLED, the reference look is tjfs's port, not upstream OT-Thing. NEEDS SPEC from Robert: what to show during (a) first-setup/AP mode, (b) boot/splash (tjfs misses the flame icon), (c) running mode. Possible overlap with TASK-580/734.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OLED shows an AP/config-mode screen with the AP SSID and the config URL (192.168.4.1) when unconfigured
- [ ] #2 Boot splash present (flame icon per Robert's spec)
- [ ] #3 Distinct display states for first-setup vs boot vs running, per Robert's spec
- [ ] #4 Gated behind HAS_OLED_CAPABLE; ESP8266 unaffected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
REFERENCE SPEC from other-projects/OT-Thing-OTGW32/Firmware/src/main.cpp (read-only; dev worktree). OLED is guarded by OLED_PRESENT (runtime I2C probe at 0x3C, begin() success) so absent-display is graceful. States to mirror: (1) BOOT SPLASH (main.cpp ~282-308): 'Nodo OTGW32' / 'Press Boot for more' / 'Hold Reset for config' + a 16x16 flame bitmap at (56,44) — this is the flame icon tjfs misses. (2) CONFIG-MODE page 1 (main.cpp ~141-153, when configMode): 'SSID: <softAPSSID>', 'PW: 12345678', 'http://<softAPIP>/' — the setup instructions tjfs wants on first boot. (3) NORMAL page 1 (~154-174): wired -> 'Wired Network' + 'IP: <eth ip>'; wifi -> 'WiFi: <ssid>' + 'IP: <ip>' or 'Connecting...'; footer 'Press Boot for more'. (4) pages 2/3 heating circuits, page 4 status (fw/runtime/mqtt). Auto-wake: display turns ON (pageno=1) on a network event or button; 30s timeout -> OFF; Boot button cycles. KEY GAP in our OLED.ino: we have a splash + 5 runtime pages but NO config-mode/AP screen, so first-boot shows nothing actionable. Our config-mode label name is 'Hold Reset for config' in OT-Thing but our wipe is triple-reset (TASK-752) — align the on-screen hint with our actual mechanism.
<!-- SECTION:NOTES:END -->
