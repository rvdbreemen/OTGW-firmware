---
id: TASK-1445
title: >-
  feat-2.0.0: port TASK-1378 (1.x) — FSexplorer reboot redirect and helper forms
  relative, so they work behind a reverse proxy
status: To Do
assignee: []
created_date: '2026-10-29T06:15:00Z'
labels:
  - port-from-1.x
  - webui
  - reverse-proxy
  - ha-ingress
  - low-priority
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 605000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Origin: 1.x TASK-1378 (HA ingress / reverse proxy), commit 18068a353 on otgw-1.x.x. Most of TASK-1378 is already on dev as dev TASK-1377.1/.3/.5/.7 (6da168984, f3ebc29b4, aa040560b, ab2a36fe5). What is already there: v2.js BASE-relative URLs (origin/dev:src/OTGW-firmware/data/v2.js:88-92), WebSocket over BASE+'ws' (:452, :991), updater PAGE_BASE (origin/dev:src/OTGW-firmware/updateServerHtml.h:549-557), the configuration_url Visit link (origin/dev:src/OTGW-firmware/MQTTstuff.ino:2501-2514), and the PIC flash pending-poll handling (v2.js:3990-4049). This task closes the one remaining server-side gap in FSexplorer.ino.

Gap 1: the reboot and reset-wireless countdown redirect goes to the host root.
- origin/dev:src/OTGW-firmware/FSexplorer.ino:783 reBootESP(): doRedirect("Reboot OTGW firmware ..", 120, "/", true)
- origin/dev:src/OTGW-firmware/FSexplorer.ino:792 resetWirelessButton(): doRedirect(..., 120, "/", true)
- doRedirect (origin/dev:FSexplorer.ino:801-845) writes URL into a Refresh header (:820), an <a href> and window.location.replace() (:840-842).
- These handlers are reached by form GET from origin/dev:src/OTGW-firmware/data/FSexplorer.html:443 (action='ReBoot') and :446 (action='ResetWireless'). The browser navigates to the countdown page. Behind HA ingress (/api/ingress/otgw/) or nginx on /otgw/, "/" then resolves to the proxy root (the HA frontend or the nginx root), not to the gateway.
- 1.x fix: src/OTGW-firmware/FSexplorer.ino:706 and :714 use "./" (18068a353). "./" resolved against /<prefix>/ReBoot gives /<prefix>/, both direct and proxied.
- The v2 UI calls ReBoot/ResetWireless with fetch (v2.js:4231, :4239) and ignores the body, so it is not affected. Do not change v2.js.

Gap 2: the Helper page (shown when FSexplorer.html is missing from LittleFS, origin/dev:FSexplorer.ino:294 and :597) has absolute form actions.
- origin/dev:src/OTGW-firmware/FSexplorer.ino:61 <form method="POST" action="/upload" ...>
- origin/dev:src/OTGW-firmware/FSexplorer.ino:66 <form action='/update' method='GET'>
- 1.x fix: src/OTGW-firmware/FSexplorer.ino:51 action="upload" and :56 action='update' (18068a353). Check which path the Helper is served on, because the relative action resolves against that page's directory. Through the catch-all handleFile it is served for the requested path. A request for /<prefix>/FSexplorer.html resolves to /<prefix>/upload, which is correct.

What NOT to port:
- The 1.x raw Header literal fix (FSexplorer.ino:61 on 1.x: Content-Length/Connection/blank line). Dev's sendFSexplorerRedirect() (origin/dev:FSexplorer.ino:578-582) uses the async server, which emits a complete response. The const Header at origin/dev:FSexplorer.ino:69 is a leftover; remove it only if grep shows no reference.
- index.js changes (basePath, PROXY_NOTICE_TEXT, per-base-path PIC settings localStorage key, pollFlashStatus fix). Dev has no index.js. v2 has its equivalents or does not need them (v2 localStorage holds only UI preferences).
- CSRF: no code change on either line (maintainer decision recorded in dev task-1377 and 1.x task-1378.3).
- updateServerHtml.h and FSexplorer.html: already relative on dev.

Risks: very small. A relative './' must not break direct access: on direct access /ReBoot then resolves to '/'. Do not touch doRedirect's signature or the deferred-reboot logic.

Verification rig: scripts/ha-ingress-rig on dev (aa040560b), with validate_ingress.py and an HA hass_ingress panel or nginx sub-path, as documented in docs/guides for 2.0.0 (ab2a36fe5).

## Evidence that dev lacks it

- dev already ported most of it as dev TASK-1377.1/.3/.5/.7: commits 6da168984, f3ebc29b4, aa040560b, ab2a36fe5 (alpha.506/507); port plan 09b4b6a69 explicitly from 1.x TASK-1378
- origin/dev:src/OTGW-firmware/data/v2.js:88-92 BASE = location.pathname.replace(/[^/]*$/,''), APIGW built on BASE (equivalent of 1.x basePath)
- origin/dev:src/OTGW-firmware/data/v2.js:452 and :991 WebSocket on port 80 'ws' relative to BASE: live log passes the proxy (better than 1.x, which only shows a notice)
- origin/dev:src/OTGW-firmware/data/v2.js:3713 PICBASE, :4201 upload, :4227 update, :4231 ReBoot, :4239 ResetWireless all on BASE; ReBoot/ResetWireless use fetch, so the redirect page is not navigated
- origin/dev:src/OTGW-firmware/data/v2.js:3990-4049 startPicFlash/pollPicFlash with picSawRunning + 'pending' polls (PIC_PENDING_MAX) = equivalent of the 1.x flashSeenActive fix 1f7553247
- origin/dev:src/OTGW-firmware/updateServerHtml.h:549-557 PAGE_BASE; req() rebases every root path, so OTA/safeboot work behind a proxy
- origin/dev:src/OTGW-firmware/FSexplorer.ino:578-582 sendFSexplorerRedirect() via webPushHeader/webSendStatus(303): the raw Header literal at :69 is no longer sent (async server builds a complete response), so the 1.x 502 fix is not needed
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:2501-2514 configUrl + origin/dev:src/OTGW-firmware/MQTTHaDiscovery.cpp:2357-2359 cu field (Visit link ported)
- CSRF: no code change on either line (1.x task-1378.3 notes; dev task-1377 notes line 72: same maintainer decision)
- GAP: origin/dev:src/OTGW-firmware/FSexplorer.ino:783 and :792 doRedirect(..., "/", true) for ReBoot/ResetWireless (1.x changed to "./" at FSexplorer.ino:706/714); reached by form GET from origin/dev:src/OTGW-firmware/data/FSexplorer.html:443/446, the countdown page (FSexplorer.ino:820 Refresh header, :840-842 window.location.replace) then lands on the proxy root, not the gateway
- GAP: origin/dev:src/OTGW-firmware/FSexplorer.ino:61 action="/upload" and :66 action='/update' in the Helper page (served at :294 and :597 when FSexplorer.html is missing); 1.x made them relative
- Not needed: 1.x per-base-path PIC-settings localStorage key; v2.js localStorage keys (:125-218, :1316) hold only UI preferences, no per-gateway PIC data

## Platform notes

On 2.0.0, FSexplorer runs on ESPAsyncWebServer (webPushHeader/restBeginStream/webSendStatus), so the 1.x raw "Header" literal fix (Content-Length, Connection: close, blank line) does not apply. Leave the dead const at origin/dev:FSexplorer.ino:69 alone, or remove it only if it is truly unreferenced. doRedirect on dev takes (msg, wait, URL, bool reboot), not a reboot-reason string as on 1.x, so only change the URL argument. The doRedirect URL goes into a Refresh header and into window.location.replace(). "./" resolves against /ReBoot's directory, which is the proxy prefix, so it works both direct and behind ingress. No ESP8266-specific concerns (PROGMEM/RAM) apply beyond the existing F()/PSTR usage.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. Every dev line I opened matches the analyst's citations. The two remaining gaps (doRedirect "/" and the absolute actions in the Helper forms) appear on origin/dev, and I found no dev task that fixes them. Two small corrections. First, Header is declared at origin/dev:FSexplorer.ino:69, and the only other mention, at :327, is inside a comment. It is therefore dead code, which the draft already hedges. Second, the AC "git diff limited to FSexplorer.ino plus the version bump" assumes dev's per-commit bump hook. That is fine for dev, but the AC should name version.h and data/version.hash explicitly. In my reading, the reboot AC in its current form cannot be fully red/green, because a reboot ends the session and the bench run then depends on the rig. A cheaper deterministic red/green check: curl -i the /ReBoot response through the proxy (or directly) and inspect the Refresh header and the window.location.replace target. The old firmware shows url=/, the fix shows url=./. Caveat: this check reboots the device.

The line numbers are right. Change the line references to "FSexplorer.ino:69 const Header (only otherwise mentioned in a comment at :327)". Add a deterministic red/green AC: the /ReBoot response carries the header 'Refresh: 120;url=./' and the body contains window.location.replace('./'). Old firmware shows url=/. Capture it with curl -i before and after the change. The rig screenshot stays as a secondary check. In the diff-scope AC, name the bump files (version.h, data/version.hash).

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Behind the ingress rig (HA hass_ingress panel or nginx on /otgw/), pressing ReBoot on the FSexplorer page shows the countdown. After it, the browser lands on the gateway UI under the proxy prefix (e.g. /api/ingress/otgw/), not on the proxy root. Old firmware ("/") shows the wrong landing page on the same rig: record before/after screenshots or the request log on .63 (Classic+S3) or .61 (OTGW32).
- [ ] #2 On direct access (http://<ip>/FSexplorer.html), ReBoot still redirects to http://<ip>/ after the countdown.
- [ ] #3 The Helper page served when FSexplorer.html is absent posts its upload to <page directory>/upload and opens <page directory>/update. Check by grepping the served HTML for action="upload" and action='update', with no leading slash.
- [ ] #4 v2.js, FSexplorer.html, updateServerHtml.h and the CSRF checks are unchanged (git diff limited to src/OTGW-firmware/FSexplorer.ino plus the version bump files version.h and data/version.hash).
- [ ] #5 python build.py for esp32, esp32-classic and esp32-combo succeeds and python evaluate.py --quick shows no new failures.
- [ ] #6 The /ReBoot response carries the header 'Refresh: 120;url=./' and the body contains window.location.replace('./'); old firmware shows url=/. Captured with curl -i before and after the change (this check reboots the device).
- [ ] #7 The 1.x rig scripts pic_flash_dryrun.py, pic_flash_ui_sim.py and pic_flash_via_proxy.py and the polling text in REVERSE_PROXY_AND_HA_INGRESS.md exist on dev (ported or recorded as not needed with a reason)
<!-- AC:END -->
