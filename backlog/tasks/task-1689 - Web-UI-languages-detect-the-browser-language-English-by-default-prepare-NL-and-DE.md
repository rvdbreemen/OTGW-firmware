---
id: TASK-1689
title: >-
  Web UI languages: detect the browser language, English by default, prepare NL
  and DE
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T11:06:00Z'
updated_date: '2026-11-07T15:08:00Z'
labels:
  - webui
  - i18n
  - feature
dependencies: []
priority: medium
ordinal: 809000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: detect which language the user uses; the default is always English; work towards supporting other languages, starting with NL and DE. Needs a design first (ADR via adr-kit): where the strings live (flash budget: app slot ~166 KB free, LittleFS 340 KiB + reserve), how the language is chosen (navigator.languages vs a setting), fallback to English per string, and how help texts (settingsHelp.h) and the v2 UI strings are split. Plan goes to Robert before any code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR for the web UI language mechanism Accepted by Robert
- [ ] #2 Browser language detected; English is used when the language is not supported, proven by a test
- [ ] #3 NL and DE translation plan agreed (scope, who translates, how strings are kept in sync)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert: research how open-source firmwares and web apps support multiple languages (frameworks, tools, language packs fetched from GitHub), without much flash growth. Research workflow wf_aaf1fb56-167 started: 6 angles (firmware with runtime packs such as EMS-ESP32/OpenDTU/AhoyDTU; compile-time firmwares such as Tasmota/ESPHome/WLED; pack ecosystems such as LuCI/OctoPrint/HA; small JS i18n libraries; translation platforms such as Weblate/Crowdin/Tolgee; GitHub delivery covering CORS, mixed content and offline), each re-checked by a verifier, then a synthesis. The report goes into docs/research/ and this task.

2026-11-07: research report written to docs/research/2026-11-07-web-ui-languages-research.md (workflow wf_aaf1fb56-167, 13 agents; each of the six angles re-checked by a verifier: 31 claims corrected, 50 left unchecked and labelled as such). Recommendation: no library (a t() helper with Intl.PluralRules), English built in as the per-key fallback, NL/DE as gzipped packs on LittleFS (estimated 13-16 KB gz each), other languages fetched by the browser from jsDelivr pinned to the firmware tag with SRI, translators on Hosted Weblate (libre plan). Open: HA ingress CSP for a jsDelivr fetch is untested. Robert asked to revisit ADR-003 for device-side fetching; TLS PoC in ../wt-tlspoc.

2026-11-07 Robert's decision: 'Zelfs als er PSRAM nodig is voor TLS dan wil ik dat ondersteunen. En fallback met functies die dat gebruiken. Of uitschakelen van functies als er geen redelijke fallback is.' Reading acted on: device-side TLS is a capability of boards WITH PSRAM (runtime check, not a build flag; the combo image runs on both). Each feature that uses TLS declares a fallback for boards without PSRAM (language packs: the browser fetches from jsDelivr with SRI, or the user uploads the pack); a feature with no reasonable fallback is switched off on such boards and the UI says why. The ADR that revisits ADR-003 records this capability rule. PoC run 1 on .63 (no allocator change): 9/9 handshakes failed with -0x7F00 SSL alloc failed (internal free 38.9 KB, largest block 30.7 KB, low point 15.8-19.9 KB); flash +165.6 KB with the full CA bundle (68,983 B bundle + ~96.6 KB TLS code), app slot left 4.6 KB. Run 2 with mbedtls_platform_set_calloc_free() redirecting blocks >= 1 KB to PSRAM is being built.

2026-11-07 PoC run 2 (build 2.0.0-beta.120+8774771 = origin/dev 87747717f + src/OTGW-firmware/tlsPocStuff.ino + tlsPocRoots.h, branch poc-tls-1689 in ../wt-tlspoc, not pushed; driver scripts and raw logs in the session scratchpad tlspoc/). The allocator is switched at runtime with mbedtls_platform_set_calloc_free(): blocks >= 1 KB go to PSRAM, the rest stays internal (the prebuilt libs define MBEDTLS_PLATFORM_MEMORY with a function allocator). Results: .63 (Classic + S3 Mini, 2 MB PSRAM): psram=0 control failed -0x7F00 as in run 1; psram=1 pinned roots 3/3 HTTP 200 (raw.githubusercontent.com README, 22,217 B, connect+handshake 706-781 ms); full CA bundle 2/2 (715-756 ms); jsDelivr tag URL 2/2 (22,230 B, 728-823 ms). Internal low point during a run 23.5-27.6 KB (from 37.5 KB before), PSRAM peak use about 50-54 KB, task stack used about 4.4 KB of 12 KB. Leak check: 20/20 consecutive bundle+PSRAM fetches OK, free internal before each run constant at 37,524 B, PSRAM 2,059,152 -> 2,059,664, connect median 724 ms, max 818 ms. .61 (no PSRAM): 5/5 failed -0x7F00 (internal free 53 KB, largest block 31.7 KB): no TLS on this board, as Robert's capability rule expects. Both units: no crash, no reboot (.63 bootcount 159 = flashes x2, .61 92), MQTT connected, settings identical before and after (diffed). Flash: full bundle build 2,289,603 B (99.8% of app0, 4.2 KB free); bundle = 68,983 B, so a pinned-roots build is about 2,220,600 B (~73 KB free); TLS code ~96 KB. .63 restored to beta.118 afterwards; .61 left on the PoC image for 6b (agreed).

2026-11-07 Robert's decision (AskUserQuestion): trust store = a mini CA bundle of about 15 roots built with ESP-IDF gen_crt_bundle.py (ISRG X1/X2, USERTrust RSA/ECC, DigiCert G2/G3, Google Trust R1-R4, Amazon Root CA 1-4 or similar), estimated 8-12 KB (to be measured), kept in the repo with a test that the target hosts validate through it. Rejected: 2 pinned roots (breaks on a CA change, GitHub moved to Let's Encrypt this year) and the full IDF bundle (+69 KB, 4.2 KB left in app0).
<!-- SECTION:NOTES:END -->
