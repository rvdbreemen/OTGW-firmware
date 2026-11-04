---
id: TASK-425
title: Fix stale ADR-054 references in source comments to point to ADR-056
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 22:02'
updated_date: '2026-04-25 22:03'
labels:
  - docs
  - adr-hygiene
dependencies: []
references:
  - docs/adr/ADR-054-optional-http-basic-auth.md
  - >-
    docs/adr/ADR-056-protected-admin-endpoint-security-and-secret-handling-contract.md
  - src/OTGW-firmware/restAPI.ino
  - src/OTGW-firmware/FSexplorer.ino
  - src/OTGW-firmware/OTGW-firmware.h
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-054 (Optional HTTP Basic Auth) is fully superseded by ADR-056 (Protected Admin Endpoint Security and Secret-Handling Contract) since 2026-03-21. Four comment references in source code still cite ADR-054 while describing live behavior that is actually documented in ADR-056. Update the references so a reader who follows the comment lands on the live contract instead of the superseded predecessor.

Discovered during ADR discovery pass on 2026-04-25 (option 3 in the user's discovery shortlist). Verification confirmed ADR-056 §2 covers the same-origin/CSRF behavior verbatim, so no new ADR or amendment is needed; this is a pure documentation alignment.

Scope is intentionally limited to source-code comments under src/. Documentation files (docs/c4/, docs/manuals/, README.md, ADR-080) are out of scope for this task: those references may legitimately discuss ADR-054 as historical context. ADR-054 itself and ADR-056 also reference each other, which is correct supersession metadata.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 src/OTGW-firmware/restAPI.ino:75 comment references ADR-056 §2 (CSRF same-origin) instead of ADR-054
- [x] #2 src/OTGW-firmware/restAPI.ino:109 comment references ADR-056 instead of ADR-054
- [x] #3 src/OTGW-firmware/OTGW-firmware.h:225 declaration comment references ADR-056 instead of ADR-054
- [x] #4 src/OTGW-firmware/FSexplorer.ino:244 comment references ADR-056 §2 instead of ADR-054
- [x] #5 No remaining ADR-054 references in src/ that describe live (non-historical) behavior — verified via grep
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated four source-code comments that referenced the superseded ADR-054 to point to ADR-056, the live "Protected Admin Endpoint Security and Secret-Handling Contract" ADR. Two CSRF-specific comments (restAPI.ino:75, FSexplorer.ino:244) now reference ADR-056 §2 (Authentication and same-origin contract); two general auth comments (restAPI.ino:109, OTGW-firmware.h:225) reference ADR-056 without a section qualifier.

Verified post-edit with `grep -r "ADR-054" src/` returning zero matches. The 11 docs/* references to ADR-054 are out of scope and remain untouched: ADR-054 itself, ADR-056, ADR-080 and README.md legitimately discuss the supersession history; the C4 docs and user manuals can be aligned in a separate hygiene pass if desired.

No new ADR was needed: discovery investigation (option 3 in the user's ADR-discovery shortlist of 2026-04-25) confirmed ADR-056 §2 already covers the same-origin/CSRF behavior verbatim. This task is the documentation-side of that finding; the architectural decision is unchanged.

Files modified:
- src/OTGW-firmware/restAPI.ino:75, 109
- src/OTGW-firmware/FSexplorer.ino:244
- src/OTGW-firmware/OTGW-firmware.h:225
<!-- SECTION:FINAL_SUMMARY:END -->
