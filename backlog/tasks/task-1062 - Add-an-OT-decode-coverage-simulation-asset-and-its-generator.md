---
id: TASK-1062
title: Add an OT decode-coverage simulation asset and its generator
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 09:58'
updated_date: '2026-08-08 10:53'
labels:
  - test
  - tooling
dependencies: []
priority: medium
ordinal: 172000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The existing scripts/tests/otgw_simulation.log is a 2445-line realistic replay built for sustained heap load (TASK-901 rig); it covers only 21 MsgIDs. There is no asset that exercises the decode and publish paths broadly. This adds a second, complementary asset: a 423-frame set covering all 134 OTmap ids plus 9 out-of-map ids, all 6 OpenTherm message types, and all 5 source prefixes including the E parity-error path. Built by harvesting every real frame from 60 capture files in OTGW-logs and supplementing the 67 OTmap ids that appear in no capture with type-appropriate synthetic frames. Ships with the generator so the asset can be regenerated when OTmap gains ids. Verified on otgw1.local: one full loop decoded 143 distinct MsgIDs and published 394 distinct MQTT topics with zero errors and flat heap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/tests/otgw_simulation_coverage.log is committed and contains only raw frame lines (no comments: the replayer feeds every non-empty line to the parser)
- [x] #2 The generator script is committed and regenerates the asset from OTmap plus the capture corpus
- [x] #3 scripts/tests/README.md documents the asset, how it differs from the load rig, and how to run it
- [x] #4 Coverage is asserted on device: all 134 OTmap ids decode, all 6 message types and all 5 source prefixes appear
- [x] #5 A full validation run on the bench shows zero exceptions, zero discarded lines and no heap regression
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds scripts/tests/otgw_simulation_coverage.log (423 frames) and its generator make_simulation_coverage.py, complementing the existing load fixture. Covers all 134 OTmap ids plus 9 out-of-map ids, all 6 message types and all 5 source prefixes including the E parity-error path. Built from 232 real frames harvested across 60 captures, supplemented with 67 synthetic frames for ids no real boiler implements. Verified on otgw1.local over 20 minutes: 143 distinct MsgIDs decoded, 383 MQTT topics published, zero exceptions, zero discarded lines, flat heap. README documents two traps found the hard way: the replayer does not skip comment lines, and parity is signalled by an E prefix rather than computed. .gitignore gained a negation for scripts/tests/*.log since the blanket rule silently ignored the asset.
<!-- SECTION:FINAL_SUMMARY:END -->
