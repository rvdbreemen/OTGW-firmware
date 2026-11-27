---
id: TASK-3115
title: 'Docs: OTGW32 sections, two supported hardware configurations for firmware 2.0'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T05:41:00Z'
updated_date: '2026-11-27T06:01:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-27 (chat, relayed by the coordinator): the sale of the OTGW32 starts; extend the documentation with sections about the OpenTherm Gateway OTGW32; there is no ban any more on naming it; refer to it as one of the 2 supported hardware configurations for firmware 2.0 (OTGW Classic with an ESP32-S3 module, and the OTGW32). Facts only from code, ADRs and bench notes; sales date, price, shop and availability stay out.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with the list of files and sections in the task notes, reported to the coordinator before the large write
- [ ] #2 README, 2.0.0 release notes intro, manuals en and nl, hardware pinout page, flash/install guide and FAQ/troubleshooting have OTGW32 sections; every fact cites code, ADR or bench note in the task notes
- [ ] #3 Leftover 'never name the OTGW32 board in public' rules in process docs and skills reworded; findings in the core block of CLAUDE.md, AGENTS.md and copilot-instructions reported to the coordinator
- [ ] #4 No em dash, plain user text, docs tests green
- [ ] #5 Open questions for Robert listed (sales date, price, shop, availability)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 PLAN (approved by the coordinator, Robert's words: OTGW32 docs, README, 2.0 release notes, disclaimer that the OTGW32 is a new device that still has to prove itself in the field; two chapters; only version 1.0 exists, the first production run; silent launch, nothing on Discord). Files: A) README.md: section Supported hardware for 2.0 with exactly two configurations (OTGW Classic with an ESP32-S3 module and PIC; OpenTherm Gateway OTGW32, OT-Direct, no PIC), disclaimer in that section (not in the headline). B) docs/releases/RELEASE_NOTES_2.0.0-draft.md and the beta notes template: intro and the same disclaimer. C) docs/manuals/en and nl: new chapter OTGW32 hardware configuration and new chapter Differences between an OTGW Classic with PIC and an OTGW32 (comparison table); adjust ch01, ch02, ch03, ch06, ch07, ch10 (nl: h01, h02, h03, h06, h07, h10). D) docs/hardware/PINOUT.md and combo-esp32-s3-pinout.md, docs/guides/FLASH_GUIDE.md and WIFI_FIRMWARE_UPDATE.md (+ reference), docs/api/MQTT.md where hardware matters, SAT docs where hardware matters, wiki drafts. E) Facts only, each cited (file:line or ADR) in these notes from OTGW-field-data/_comms/otgw32_facts_sheet.md; no sales date, price, shop or availability.

2026-11-27 OPEN QUESTIONS for Robert: (1) reading of his sentence: 'Version 1.0 of the OTGW32 is the first production run' (the original 'Een enkele van de OTGW 32, 1.0 is de eerste productierun' is unclear); (2) sales date, price, shop, availability and who sells it stay out; (3) the existing hardware docs call the board an OT-Thing OTGW32 PCB: is the OT-Thing project the origin or maker, and may that be said? Until he answers the new text names only the OpenTherm Gateway OTGW32; (4) official product name spelling: first mention OpenTherm Gateway OTGW32, then OTGW32 (decided by the coordinator, Robert's rule). Decisions of the coordinator 2026-11-27: disclaimer in the hardware section and in the README hardware section, not in the README headline; no order or manual page to link.
<!-- SECTION:NOTES:END -->
