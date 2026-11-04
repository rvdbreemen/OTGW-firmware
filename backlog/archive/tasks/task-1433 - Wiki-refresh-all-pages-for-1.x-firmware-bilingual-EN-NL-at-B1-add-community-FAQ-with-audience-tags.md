---
id: TASK-1433
title: >-
  Wiki: refresh all pages for 1.x firmware, bilingual EN/NL at B1, add community
  FAQ with audience tags
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T20:32:00Z'
updated_date: '2026-10-28T22:11:00Z'
labels: []
dependencies: []
ordinal: 595000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bring the GitHub wiki (OTGW-firmware.wiki) up to date with the 1.x firmware (v1.7.7). Every page in English and Dutch at B1 level. Add a FAQ (about 25-30 questions) built from 5 years of #english-support and #nederlandse-ondersteuning, tagged per audience (new user / tinkerer / developer). No references to 2.0.0 (alpha). Source analysis: Discord dump 2026-10-28, top 10 already posted to #questions-and-answers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every existing wiki page checked against the otgw-1.x.x source and corrected (REST /api/v2, current UI names, flash tooling, settings behaviour)
- [x] #2 Every content page exists in EN and NL, linked to each other
- [x] #3 FAQ pages in EN and NL with audience tags, sorted beginner to developer
- [x] #4 Home and NL-Start index pages sort pages by audience
- [x] #5 No mention of 2.0.0 / ESP32 / OTGW32 in the wiki
- [x] #6 All links checked (HTTP 200), language reviewed for B1
- [x] #7 Wiki committed and pushed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wiki refreshed for firmware 1.x (v1.7.7) and pushed (wiki commit c83d748): 40 pages, every content page in EN and NL at B1, new bilingual FAQ (37 questions from 13,868 support messages 2021-2026), new PIC-firmware page and 5 new NL pages, index/sidebar sorted by audience (new user / tinkerer / developer). Evidence: fact sheet with path:line citations from the v1.7.7 tag; 6 adversarial reviewers checked every claim against v1.7.7 code, tclcode pages, OT spec v4.2 and staff Discord messages and fixed the wrong ones (e.g. /api/v1 410 since 1.2.0, upgrade order for <=1.3.x, 19-24 V on pre-2.3 boards, Therm contacts, D3 is BUTTON, <ignored> meaning, dead Discord invite); 2 B1 language passes; script check: 0 broken wiki links/anchors, 0 em dashes, 0 2.0.0/ESP32/SAT mentions; 75 external URLs checked (only placeholders and two bot-blocking sites fail); live check of 9 new/key pages on github.com. The 10 Discord forum answers in #questions-and-answers were corrected from the same review and Q7 became a two-part debug guide. Not verified on hardware: OTA order from <=1.3.x, triple-reset timing, Dallas pin choices. Repo doc drift found along the way is in TASK-1435.
<!-- SECTION:FINAL_SUMMARY:END -->
