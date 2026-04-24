---
id: TASK-411
title: 'Docs: MQTT API migration guide + 2.0.0 release notes for OT-bus topic move'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 20:00'
updated_date: '2026-04-24 20:06'
labels:
  - documentation
  - mqtt
  - breaking-change
  - 2.0.0
  - release-notes
dependencies:
  - TASK-408
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Update consumer-facing documentation so that MQTT consumers and Home Assistant users understand the OT-bus topic migration in 2.0.0, and have copy-pasteable mosquitto_pub/sub commands for manual broker cleanup if desired.

Both documents are in English per project convention (docs/api/ is English; release notes must be English for international audience).

CHANGES:

A) docs/api/MQTT.md:
1. Top of file: breaking-change banner with anchor links to the migration section below and to ADR-084.
2. "PIC Gateway Information" section: remove the three keys boiler_connected, thermostat_connected, otgw_connected from the listed topics. Optional short note that these were moved; anchor link to the migration section.
3. "OT Direct (OTGW32)" section: remove the three keys boiler_connected, thermostat_connected, ot_online. Note that ot_online has been retired; the concept lives under otgw_connected now.
4. New section "OT-bus state (generic, since 2.0.0)" listing the three canonical topics under OTGW/value/<uniqueId>/ with semantic meaning and payload values (on/off for the two _connected, online/offline for otgw_connected; check actual payload format in sendMQTTData and CCONOFF macro).
5. New section "Migration from 1.4.x / pre-release 2.0.0 (OT-bus state topics)" containing:
   - The before/after table (6 old topics to 3 new topics)
   - Automatic cleanup paragraph explaining the self-healing firmware behaviour
   - Manual cleanup paragraph with mosquitto_pub shell snippet using -r -n to wipe retained entries (loop over 6 deprecated topic leaves)
   - "Listing which retained topics still exist" paragraph with mosquitto_sub --retained-only -W 2 examples
   - Home Assistant consumers paragraph explaining unique_id stability and entity history preservation

Exact wording: see docs/api/MQTT.md migration paragraph in the plan file (C:\\Users\\rvdbr\\.claude\\plans\\idempotent-weaving-wolf.md, Wijziging 4).

B) docs/releases/RELEASE_GITHUB_2.0.0.md:
1. Add new subsection under "Breaking changes": "OT-bus state moved to generic MQTT topics"
2. Content: list removed topics, list new canonical topics, explain "What you need to do" with HA = nothing required, custom consumers = update topic pattern and optionally use mosquitto_pub snippet from MQTT.md.
3. Rationale paragraph pointing to ADR-084.

Exact wording: see release notes snippet in the plan file, Wijziging 5.

POST-RELEASE REMINDER: if the 2.0.0 GitHub release is already published, after editing docs/releases/RELEASE_GITHUB_2.0.0.md in the repo, run `gh release edit 2.0.0 --notes-file docs/releases/RELEASE_GITHUB_2.0.0.md` to sync the published release body. Then merge main back to dev per project convention.

STYLE CONSTRAINTS:
- English language (per feedback memory)
- No em dashes anywhere in the output (per feedback memory); use colons, periods, commas, or parentheses.
- Preserve existing markdown style of each document (heading levels, code fence conventions).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/api/MQTT.md has a breaking-change banner at the top with anchor links to the migration section and ADR-084
- [x] #2 docs/api/MQTT.md PIC Gateway Information section no longer lists boiler_connected, thermostat_connected, or otgw_connected under the otgw-pic/ tree
- [x] #3 docs/api/MQTT.md OT Direct section no longer lists boiler_connected, thermostat_connected, or ot_online under the otgw-otdirect/ tree
- [x] #4 docs/api/MQTT.md contains a new 'OT-bus state (generic, since 2.0.0)' section with the three canonical topics under OTGW/value/<uniqueId>/ and their semantic meaning + payload values
- [x] #5 docs/api/MQTT.md contains a 'Migration from 1.4.x / pre-release 2.0.0' section with: before/after table, automatic cleanup explanation, manual cleanup mosquitto_pub snippet using -r -n, retained-listing mosquitto_sub snippet with -W 2, Home Assistant unique_id stability note
- [x] #6 docs/releases/RELEASE_GITHUB_2.0.0.md contains a new 'OT-bus state moved to generic MQTT topics' subsection under Breaking changes with removed topics, new canonical topics, 'What you need to do' guidance for HA and custom consumers, and ADR-084 reference
- [x] #7 No em dashes anywhere in the modified documents (use colons, periods, commas, parentheses instead)
- [x] #8 Both documents in English
- [x] #9 Any markdown-lint configured in the repo still passes on the modified files
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Both consumer-facing docs updated.

docs/api/MQTT.md:
- Breaking-change banner at line 5 with anchor links to the new generic section and the migration section, plus ADR-084 reference.
- New "OT-bus state (generic, since 2.0.0)" section at lines 63-73 listing the three canonical topics with semantic meaning and ON/OFF payload values.
- "PIC Gateway Information" section (lines 75-87) trimmed: boiler_connected, thermostat_connected, otgw_connected removed. Pointer note added.
- "OT Direct (OTGW32)" section (lines 111-128) trimmed: boiler_connected, thermostat_connected, ot_online removed. ot_online retirement noted.
- New "Migration from 1.4.x / pre-release 2.0.0 (OT-bus state topics)" section at lines 1247-1308 with: before/after table (6 old to 3 new canonical, ot_online explicitly in removed column only), automatic cleanup explanation, manual cleanup mosquitto_pub -r -n bash snippet, retained-listing mosquitto_sub --retained-only -W 2 snippet, Home Assistant unique_id stability note mentioning OTGW32 entities now visible.

docs/releases/RELEASE_GITHUB_2.0.0.md:
- New "Breaking changes" section at lines 54-82 with "OT-bus state moved to generic MQTT topics" subsection: removed topics list, new canonical topics list, "What you need to do" guidance (HA = nothing; custom consumers = update patterns), rationale paragraph with ADR-084 and migration-guide reference.
- Stale "No breaking changes vs v1.3.5" bullet at line 99 replaced with a pointer to the new section.

Verification:
- grep -n "—" returned only pre-existing em dashes (MQTT.md lines 1034-1036, RELEASE_GITHUB_2.0.0.md line 17), all outside edit scope. Zero em dashes added.
- All four anchor links verified against heading slug rules (GitHub strips parens, commas, periods; double-hyphen at " / " boundary).
- Both files in English; no Dutch slips.
- Existing markdown style (heading levels, code-fence conventions, table format) preserved.

All 9 acceptance criteria met.
<!-- SECTION:FINAL_SUMMARY:END -->
