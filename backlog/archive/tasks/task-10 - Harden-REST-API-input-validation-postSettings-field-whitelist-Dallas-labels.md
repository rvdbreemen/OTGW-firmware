---
id: TASK-10
title: 'Harden REST API input validation (postSettings field whitelist, Dallas labels)'
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:53'
updated_date: '2026-03-12 22:00'
labels:
  - security
  - rest-api
dependencies: []
references:
  - 'src/OTGW-firmware/restAPI.ino:962-976'
  - 'src/OTGW-firmware/restAPI.ino:1007-1028'
  - 'src/OTGW-firmware/restAPI.ino:596'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The REST API accepts user input in several places without sufficient validation:

1. **postSettings field name not whitelisted** (restAPI.ino:962-976): The `postSettings()` handler extracts a `field` name from the JSON body and passes it to `updateSetting(field, newValue)`. While `updateSetting()` uses `strcasecmp_P` against known field names (so unknown fields are silently ignored), there is no explicit whitelist or rejection of unknown fields. This is defense-in-depth: if `updateSetting()` ever gains a catch-all handler, unvalidated field names would become dangerous.

2. **updateAllDallasLabels content not validated** (restAPI.ino:1007-1028): The handler writes the raw HTTP body to `/dallas_labels.ini` after only checking for `{` and `}` delimiters. While the file is later parsed as JSON key-value pairs (and LittleFS limits file size), writing arbitrary content to the filesystem is poor practice. Should validate that the body is well-formed JSON with expected structure (string keys mapping to string labels).

3. **getDallasAddress null check missing** (restAPI.ino:596): `getDallasAddress()` return value is passed directly to `sendJsonOTmonMapEntryDallasTemp()` without null check. If it returns nullptr, undefined behavior occurs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 postSettings returns 400 error with message for unrecognized field names
- [ ] #2 updateAllDallasLabels validates JSON structure before writing to LittleFS
- [ ] #3 getDallasAddress return value is null-checked before use in REST handlers
- [ ] #4 Existing valid API calls continue to work unchanged
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hardened REST API input validation:

1. postSettings: Added PROGMEM whitelist of ~45 known setting field names. Unknown fields now return 400 with error message instead of being silently ignored.
2. updateAllDallasLabels: Added 2KB body size limit + structural JSON validation that verifies all keys and values are properly quoted strings with colons and commas in expected positions. Invalid JSON is rejected before writing to LittleFS.
3. getDallasAddress: Added null-check on return value — skips sensor entry if address resolution fails.

Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
