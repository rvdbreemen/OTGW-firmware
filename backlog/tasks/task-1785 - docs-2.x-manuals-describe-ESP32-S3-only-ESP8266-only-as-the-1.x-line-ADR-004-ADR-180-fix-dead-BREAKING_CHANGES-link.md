---
id: TASK-1785
title: >-
  docs: 2.x manuals describe ESP32-S3 only, ESP8266 only as the 1.x line;
  ADR-004 -> ADR-180; fix dead BREAKING_CHANGES link
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T18:40:00Z'
updated_date: '2026-11-10T19:08:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 880000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's decision 2026-11-10 ('clean up everything now', relayed by the coordinator) on the TASK-1783 findings. (1) ESP8266 passages in EN+NL manuals (ch01/h01 5/6, ch02/h02 7/8, ch04/h04 1/1, ch07/h07 1/1, ch08/h08 3/3, ch10/h10 9/9) and docs/MANUAL.md (25): 2.x is ESP32-S3 only; ESP8266 appears only as 'the 1.x line' (going back to 1.x = put the Wemos back); pure 1.x content removed. 'Wemos D1 mini socket' (the physical socket name) stays. (2) ADR-004 (superseded) cited for the String rule: ADR-180 (h08:309 and the ch08/h08/MANUAL ADR tables); historical changelog lines stay. (3) docs/BREAKING_CHANGES.md:126 links ../RELEASE_NOTES_1.4.1.md, a dead path: point to releases/archive/RELEASE_NOTES_1.4.1.md. Every claim checked against code, EN and NL together, no em dashes, no OTGW32. One subagent per chapter.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch01/h01 ESP8266 passages reworked
- [x] #2 ch02/h02 ESP8266 passages reworked
- [x] #3 ch04/h04, ch07/h07 ESP8266 passages reworked
- [x] #4 ch08/h08 ESP8266 passages reworked and ADR-004 -> ADR-180
- [x] #5 ch10/h10 ESP8266 passages reworked
- [x] #6 docs/MANUAL.md matches the reworked chapters
- [x] #7 docs/BREAKING_CHANGES.md link to the 1.4.1 notes fixed
- [x] #8 ch01/h01: release status no longer 'alpha' (matches the published betas); EN has the report-issues section NL h01 has (parity)
- [x] #9 ch07/h07 7.8: 1.x-only root cause removed or reduced to one 1.x-line reference
- [x] #10 MANUAL.md: ADR-014 row correct for 2.x (build.py via build.bat/build.sh); 'Key ADRs' table aligned with ch08 / docs/adr/ADR-INDEX.md; mirrors the ch01/ch07 changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC7: docs/BREAKING_CHANGES.md:126 now links releases/archive/RELEASE_NOTES_1.4.1.md (relative to docs/, file exists after TASK-1783's archive move).

AC1 ch01/h01: EN ESP8266 lines 2->1, NL 3->1. Kept line 7 (2.x is ESP32-S3 only; 1.x = Wemos D1 mini/ESP8266, going back = put the Wemos back). Reworded EN 57/NL 66 (duplicate 'ESP8266 moved to 1.x' removed) and NL 197 (1.x binaries). 'Wemos D1 mini socket' kept (physical socket). Verified: platformio.ini envs esp32, esp32-classic, esp32-combo, no esp8266. Findings (not changed): EN ch01:59 says 2.0.0 is 'still in alpha' although public betas exist; NL h01 ~191-197 has a report-issues section with no EN counterpart.

AC2 ch02/h02: ESP8266 lines EN 2->1, NL 3->1; remaining line 20 (both): 'The 1.x line (maintenance branch otgw-1.x.x) runs on the Wemos D1 mini (ESP8266). Going back to 1.x means putting the Wemos D1 mini back in its socket and flashing a 1.x release; a 1.x zip does not run on an S3.' Removed '(ESP8266)' parentheticals (EN 255, NL 276/280). Wemos socket/GPIO-column mentions kept (physical socket, 1.x pin migration).

ch04/h04: ESP8266 1->0 each. The 'Streaming discovery architecture' paragraph (EN 206 / NL 217) was also wrong for 2.x ('128-byte chunks', 'nothing buffered in RAM', '~200 bytes per entity'); reworded to the code: measure pass + transient heap buffer (~900 B worst case) published in one piece and freed (MQTTstuff.ino:24-27, MQTTHaDiscovery.cpp:2226-2230, 2258-2264), re-checked in the main session. Finding: the heading 'Streaming discovery architecture' and 'stream*Discovery' names stay (real function names); renaming is Robert's call.

ch07/h07: ESP8266/Wemos 1->0 each; section 7.8 root cause reworded to '1.x line' (stack overflow from ~1,600 B local buffers in publishToSourceTopic, now static: MQTTstuff.ino:2246/~2301); the table row's '4 KB CONT stack' (ESP8266-specific) became 'stack (1.x line)'. Finding: 7.8 and its table row describe a 1.x-only root cause; dropping them is Robert's call (cross-referenced at NL 425, 566).

ch08/h08: ESP8266 2->0 and ADR-004 2->0 each. 'Wemos D1 mini socket' (physical socket) kept at EN 161/NL 160. Line 198/197 -> 'The 1.x line continues only on the otgw-1.x.x maintenance branch' (platformio.ini envs esp32:107, esp32-classic:244, esp32-combo:282, default_envs esp32:15); 273/272 (1.x PIC link) removed. ADR refs: heading 'Static Buffers (ADR-053 / ADR-180)'; superseded ADR-004 table rows removed, ADR-180 row added (EN+NL); NL 307/309 cite ADR-053 (buffers) and ADR-180 (String).

ch10/h10: ESP8266 EN 8->5, NL 9->6. ESP8266 glossary entry -> '1.x line' entry; ESP32 entry: 240 MHz (platformio.ini:116 f_cpu) and BLE, unsourced RAM claim removed; PROGMEM rule 'carried over from the 1.x line'; v1.3.5 changelog line now says 'on the 1.x line'. Remaining hits are the allowed 1.x-line form (v2.0.0 changelog, NL 333 release assets) and verbatim titles of immutable ADRs (ADR-001 'ESP8266 Platform Selection', ADR-061 'Unified ESP8266/ESP32 Platform Abstraction') in the ADR index and supersession list.

MANUAL.md: mirrors the chapter changes (15 ESP8266 lines -> 0, ADR-004 for current rules -> ADR-053/ADR-180, 'Migrating from ESP8266 (1.x)' -> 'Migrating from 1.x' with anchor #migrating-from-1x updated, glossary 1.x line/ESP32/PROGMEM, ch07 7.8, ch08 ADR table). The v1.0.0 changelog line keeps 'ADR-004' as history (main session reverted the subagent's rewrite, matching ch10:288). Findings, not fixed: MANUAL ADR-014 row still says 'build.py for Arduino CLI' (1.x-era; ch08 is current); MANUAL's 'Key ADRs' table is older/shorter than ch08's.

2026-11-10 coordinator, under Robert's 'clean up everything now': also fix the remaining findings (alpha status, EN/NL parity of the report-issues section, ch07 7.8, MANUAL ADR-014 row and ADR table). ch04 heading stays as is.

ch01/h01: status sentence now 'in public beta: releases are published as GitHub prereleases ... SAT stays experimental and opt-in' (tag v2.0.0-beta.170; gh release list shows 170/167/162/158 as Pre-release; TASK-1740 line 21). EN gained the 'Community and Support' section NL had (GitHub, Discord https://discord.gg/zjW3ju7vGQ from README.md:3/:334, Releases, Documentation). A false NL claim (built-in update from the releases list) was not ported and is corrected in NL: 2.x does not download updates itself (ch03:357), give the release zip to the Update page. Unverified, ported: Discord 'international, English'. Other EN/NL structure gaps in ch01 (Key Features subheadings, NL hardware subsections, 'How to use this manual' scope, Prerequisites grouping) are recorded as findings, not fixed. EN ch07:303 still says 'check the repository for the Discord invite link'.

ch07/h07 7.8: 1.x root cause and NL 'opgelost in v2.0.0' subsection removed; 7.8 now has the same structure in EN and NL (what it is, symptoms, what to do) with one closing 1.x-line sentence. Kept 2.x claims checked: OTGW-Core.ino:741 (platformSerialHasOverrun), :865 DebugT 'Serial Overrun', :866 reportOTGWEvent_P -> MQTT event_report (:187) + WebSocket (:190), 15 s startup quiet (:106, :151-159); MQTTstuff.ino:2301 static sourceTopic. Removed a wrong NL claim of a 'serial_overrun' WebSocket event type (no such string in src/, re-checked). 7.10a table row kept as 1.x-line history; cross-references to 7.8, 7.2, 7.4, 7.7, 7.11 resolve.

MANUAL.md + ADR tables (coordinator follow-up): MANUAL mirrors ch01 status/Community section and ch07 7.8; MANUAL 'no breaking changes' sentence replaced by ch01's ADR-106 paragraph (ADR-106 Accepted). Key ADRs table (ch08/h08/MANUAL) now lists current ADRs: 014->083, 042->146 (via 141), 069->197, 100->174 (successor text from each ADR's Decision). ch08/h08 build-target section: ADR-126->127, ADR-160->201, persist-once prose replaced by ADR-201 Decision items 1-4 (checked against the ADR text). ArduinoJson glossary (MANUAL, ch10) -> ADR-146/JsonEmit (jsonEmit.h, settingStuff.ino:150). Discord link (README.md:3/:334) in ch07/h07, ch10/h10, MANUAL. ch10/h10 ADR catalog aligned with ADR frontmatter: 005->133, 011 Amended, 014->083, 022->200, 026->139, 042->141->146, 048->136, 069->197, 071->198; supersession list extended; main-session re-check: 0 catalog mismatches in EN and NL (remaining script hits are other tables). Note: ADR-083 itself names 'pio run' while CLAUDE.md requires build.bat/build.sh; the manual follows the project rule.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ESP8266 cleanup per Robert's decision, docs only. Script counts after (ESP8266+Wemos, OTGW32 0 everywhere): EN ch01 4, ch02 7, ch04 0, ch07 0, ch08 1, ch10 7; NL the same; MANUAL.md 15 (all remaining hits are the physical 'Wemos D1 mini socket', the 1.x-line sentence, or verbatim titles of immutable ADR-001/ADR-061; literal 'ESP8266' is 0 in MANUAL.md). AC1-AC5 one subagent per chapter pair, each hit decided with file:line evidence, re-checked in the main session (0 em dashes, 0 OTGW32 lines in the diff). Bonus fix: ch04/h04 discovery paragraph described the old ESP8266 128-byte chunker; now matches MQTTstuff.ino:24-27 and MQTTHaDiscovery.cpp:2226-2230. ADR-004 -> ADR-053 (buffers) / ADR-180 (String) in ch08/h08/MANUAL; history lines keep ADR-004. AC7: BREAKING_CHANGES.md links releases/archive/RELEASE_NOTES_1.4.1.md. Open for Robert: ch01:59 still says 2.0.0 is 'alpha'; NL h01 has a report-issues section EN lacks; ch07 7.8 (1.x-only root cause) could go; ch04 heading 'Streaming discovery architecture'; MANUAL ADR-014 row and older ADR table.
<!-- SECTION:FINAL_SUMMARY:END -->
