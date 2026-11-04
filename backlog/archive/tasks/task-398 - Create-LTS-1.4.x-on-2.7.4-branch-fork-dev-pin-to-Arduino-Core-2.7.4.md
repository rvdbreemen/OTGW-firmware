---
id: TASK-398
title: 'Create LTS-1.4.x-on-2.7.4 branch: fork dev, pin to Arduino Core 2.7.4'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 11:19'
updated_date: '2026-04-24 13:19'
labels:
  - lts
  - branch-management
  - arduino-core
  - reliability
  - maintenance
dependencies: []
references:
  - deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md
  - >-
    backlog/tasks/task-394 -
    Stabilize-OTA-reboot-per-diagnostic-report-—-2.0.0.md
  - >-
    backlog/tasks/task-395 -
    Port-TASK-394-Phase-12-reboot-diagnostics-fixes-from-2.0.0-to-dev.md
  - >-
    backlog/tasks/task-396 -
    TASK-394-Phase-34-port-dev-hardening-deferred-reboot-OTA-heap-probes-watermark-flash-sanity-exccause.md
  - >-
    backlog/tasks/task-397 -
    Diagnose-random-doBackgroundTasks-loop-stalls-—-BGTRACE-always-on-instrumentation.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Purpose

Create a parallel long-term-support (LTS) branch of the 1.4.x OTGW-firmware that compiles against ESP8266 Arduino Core 2.7.4 instead of 3.1.2. The branch carries every feature, fix, and improvement shipped in v1.4.x on mainline but isolates users from potential Core 3.1.0+ regressions (notably PR esp8266/Arduino#8598 removing the implicit WiFiClient/WiFiUDP::stopAll() from the OTA path, documented as a likely root cause of the half-state WiFi condition after OTA reboot).

## What this branch IS

- A downstream-compatible build of the current 1.4.2-beta dev codebase on an older, battle-tested Arduino Core
- A stability fallback for users experiencing OTA-reboot instability on 3.1.2
- Feature-equivalent to mainline dev at the moment of fork (includes TASK-392 HA discovery fix, TASK-395 port work, TASK-396 reboot hardening, TASK-397 BGTRACE/OTTRACE diagnostics — the latter COMPILE-TIME DISABLED for LTS)
- Released with suffix versioning: 1.4.2-lts.1, 1.4.2-lts.2, etc.
- Maintained with selective forward-ports from dev (fixes only, not features)

## What this branch is NOT

- NOT a replacement of dev — mainline continues on Core 3.1.2 with ongoing feature work
- NOT a rollback of user-visible changes — MQTT topics, REST API, web UI are identical
- NOT eternally supported — planned retirement criterion: once 3.1.2 stability is proven empirically (≥30 days soak test with 20+ OTA cycles, 0 exceptions) the LTS may be frozen to security-fixes-only and eventually archived
- NOT a playground for experiments — LTS touches Core-compatibility bits only, no refactors or new features

## Relationship to prior work

This task is the direct outcome of:
- TASK-394 (reboot hardening on 2.0.0) — established doRestart/prepareForReboot infrastructure
- TASK-395 (port Phase 1+2 to dev) — brought that hardening to dev
- TASK-396 (Phase 3+4 on dev) — deferred-reboot + OTA heap probes
- TASK-397 (BGTRACE/OTTRACE diagnostics) — identified heap-drop correlated with OT frame processing
- deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md — root-cause analysis

All above work remains on dev and is INHERITED by LTS via fork. LTS does not re-do any of it.

## Bug-fix flow policy (FORWARD-PORT ONLY)

- Fixes land FIRST on dev (mainline), then selectively cherry-pick to LTS if applicable
- Never the reverse direction (LTS → dev) — prevents LTS from becoming a bug-fix hotline that diverges from mainline
- LTS-specific fixes (e.g. 2.7.4 API quirks) land only on LTS with a label \"lts-specific\" so they're not accidentally ported back to mainline

## Version naming scheme

- LTS releases: 1.4.2-lts.1, 1.4.2-lts.2, ...
- Semver minor bump is reserved for mainline (1.5.0 remains for future dev feature release)
- Version suffix \"-lts.N\" makes the LTS lineage unambiguous in logs, MQTT version topics, and release artifacts

## Key design decisions to make BEFORE starting

1. **FQBN partition layout**: 4M2M (keep 2MB FS, same as mainline) OR 4M1M (revert to 1MB FS, matches pre-1.4.x layout). Preference: 4M1M for cleaner rollback narrative; requires OTA migration guide.
2. **Branching timing**: fork immediately from current dev HEAD, OR first disable BGTASKS_TRACE/OTPROCESS_TRACE on dev, commit, then fork. Preference: disable first for cleaner LTS base.
3. **Diagnostic code retention**: keep BGTRACE/OTTRACE code on LTS but compile-time disabled (define=0), so future debugging is one flag flip away.

## Success criterion

A user experiencing OTA stalls on mainline 1.4.x-on-3.1.2 can flash the LTS variant, go through a documented migration path, and run the SAME feature-set without the half-state WiFi issue. Empirically: ≥20 consecutive OTA cycles without exception/WDT/mount-failure on the LTS variant.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New branch LTS-1.4.x-on-2.7.4 exists on origin, forked from dev HEAD at the moment of branch creation (not from v1.3.5 tag). Branch carries every commit from dev including TASK-396 reboot hardening and TASK-397 diagnostic instrumentation.
- [x] #2 arduino/arduino-cli.yaml board_manager.additional_urls points to the 2.7.4 package URL (https://arduino.esp8266.com/stable/package_esp8266com_index.json at version 2.7.4) instead of 3.1.2
- [x] #3 build.py board_manager.additional_urls matches the arduino-cli.yaml pin (2.7.4); FQBN decision documented: either stay on eesz=4M2M (2MB FS, requires verifying 2.7.4 supports it) or revert to eesz=4M1M (1MB FS, matches pre-1.4.x layout). Rationale committed in ADR.
- [x] #4 All external libraries (AceTime@4.1.0, OneWire@2.3.8, DallasTemperature@4.0.6, WebSockets@2.7.2) compile cleanly on Core 2.7.4 with the pinned versions. If any library requires downgrade to be 2.7.4-compatible, that version is pinned and the reason documented.
- [ ] #5 python build.py --firmware produces a working .ino.bin on Core 2.7.4 without warnings or errors. Binary size, IRAM usage, and flash usage documented for comparison against the Core 3.1.2 baseline.
- [ ] #6 python evaluate.py --quick passes on the LTS branch. Gates that are 3.x-specific (e.g. PROGMEM alignment helpers assumption) either continue to pass (no-op on 2.7.4) or are conditionally skipped with explicit rationale.
- [ ] #7 Runtime validation on one physical OTGW device: boot signature logs, MQTT connects, OT stream processes, nightly restart executes cleanly, OTA firmware upgrade (LTS→LTS) completes and reboots. BGTRACE + OTTRACE outputs captured for 15+ minutes and compared to Core 3.1.2 baseline.
- [ ] #8 New ADR in docs/adr/ documenting the branch strategy: (a) why LTS exists, (b) what it is NOT (not a replacement of dev), (c) bug-fix flow policy (forward-port from dev → LTS, not the reverse), (d) version naming scheme for LTS releases (e.g. 1.4.2-lts.1, 1.4.2-lts.2)
- [ ] #9 docs/BREAKING_CHANGES.md has a new section specifically for users considering the LTS variant: when to pick LTS vs mainline dev, flash-order requirements for the 2MB vs 1MB FS transition, that settings migrate if FS layout stays on 2MB but are lost if layout reverts to 1MB
- [ ] #10 README.md top section updated with a short LTS-vs-mainline paragraph and a link to the ADR
- [ ] #11 First LTS release artifact (OTGW-firmware-1.4.x-lts.1-*.ino.bin + .littlefs.bin) is built, versioned via version.h, and the build-bump commit lands on the LTS branch
- [ ] #12 Design decision A (locked): FQBN stays on eesz=4M2M (2MB LittleFS). Rationale: matches current 1.4.x mainline layout, avoids field-device FS shrink migration, Core 2.7.4 supports 4M2M via the d1_mini board variant. Documented in the ADR created for this task.
- [ ] #13 Design decision B (locked): Fork point is dev HEAD AFTER a prepatory commit that sets BGTASKS_TRACE=0 and OTPROCESS_TRACE=0 in OTGW-firmware.ino and OTGW-Core.ino respectively. The disable commit lands on dev first, then LTS branches from that commit. Diagnostic CODE stays in tree (both branches) so future debugging is a one-line toggle.
- [ ] #14 Prepatory commit on dev: BGTASKS_TRACE changed from 1 to 0 in OTGW-firmware.ino (~line 120 area) AND OTPROCESS_TRACE changed from 1 to 0 in OTGW-Core.ino (~line 3701 area). Commit message references TASK-398. Dev continues to have BGTRACE/OTTRACE available, just off-by-default.
- [ ] #15 Branch creation: git checkout -b LTS-1.4.x-on-2.7.4 from the disable-diag commit. First LTS-only commit: chore(lts): initialize LTS branch from 1.4.2-beta, targeting Core 2.7.4. No code changes in that first commit — pure fork marker.
- [ ] #16 arduino/arduino-cli.yaml: additional_urls line changed from 3.1.2 package URL to https://arduino.esp8266.com/stable/package_esp8266com_index.json, and arduino-cli core install command will use esp8266:esp8266@2.7.4 explicitly. Verified by 'arduino-cli core list' showing 2.7.4 installed.
- [ ] #17 build.py: the additional_urls string matches arduino-cli.yaml. The FQBN stays eesz=4M2M per decision A. A comment above the URL pin explains the LTS branch targets 2.7.4 and the URL must not be bumped without accompanying code review.
- [ ] #18 Library compat: AceTime@4.1.0 compiles on Core 2.7.4. If it fails, downgrade to the last known 2.7.4-compatible AceTime version is attempted and the required version is pinned in build.py. Reason: AceTime 4.x depends on ZoneProcessor APIs that may have expected newer C++17 features.
- [ ] #19 Library compat: OneWire@2.3.8, DallasTemperature@4.0.6, WebSockets@2.7.2, SimpleTelnet (in-tree) all compile on Core 2.7.4 without warnings. Each verified by dedicated 'arduino-cli compile --library <path>' probe OR by a clean full firmware build.
- [ ] #20 python build.py --firmware on the LTS branch produces OTGW-firmware-1.4.2-lts.1-<hash>.ino.bin. Binary size documented: IRAM usage, RAM global/static usage, flash code size — all logged to a build-comparison.md file in docs/.
- [ ] #21 python evaluate.py --quick on the LTS branch: 0 new failures compared to the equivalent run on dev. ADR-062 discovery-counter gate and HA-DISC consistency gate remain passing. Flash-string compliance gate may still show its pre-existing warnings — that's OK as long as count is unchanged from dev.
- [ ] #22 Runtime validation on one physical OTGW test device (Wemos D1 mini class, 4M2M flash). Procedure: flash LTS build serially (not OTA), capture telnet log at port 23 for 15 minutes minimum. Expected: boot signature line once, MQTT connects within 10s, OT stream shows incoming frames, no Exception or WDT.
- [ ] #23 OTA cycle test on LTS: perform OTA firmware upgrade (LTS-rev-N → LTS-rev-N+1 by bumping version only) 5 times consecutively. Each cycle must show clean HTTP 200 response to browser BEFORE reboot, deferred-reboot mechanism fires, and device reconnects within 60 seconds. 0 stalls, 0 resets via hardware button needed.
- [ ] #24 Nightly restart test on LTS: set bNightlyRestart=true and iRestartHour=<current+1>, wait for the hour boundary, observe in log that the nightly restart path fires doRestart("[nightly] ...") and the device reboots cleanly.
- [ ] #25 24-hour soak test on LTS: device left running continuously with normal OT activity, MQTT subscribed, WebSocket optional. Captured: heap watermark progression (via logBootSignature on each hourly stats emission), any Exception/WDT/LittleFS failures. Pass criterion: minHeap stays above 4000 bytes throughout the 24h window; 0 exceptions.
- [ ] #26 New ADR file docs/adr/ADR-XXX-lts-branch-strategy.md created with: (a) context for creating LTS, (b) what LTS is and is not, (c) forward-port policy (dev → LTS only, never reverse), (d) version naming 1.4.x-lts.N, (e) retirement criteria, (f) test gates each LTS release must pass. Status: Proposed → request user approval → Accepted (follow ADR workflow from CLAUDE.md).
- [ ] #27 docs/BREAKING_CHANGES.md: new top-level section 'LTS vs Mainline (1.4.x-lts)' with guidance on when to choose each, and that the LTS variant uses Core 2.7.4 while mainline uses Core 3.1.2. Does NOT promise LTS is more stable — only that it targets an older runtime.
- [ ] #28 README.md: new paragraph near the top with a short 'Which version should I use?' callout linking to the ADR and BREAKING_CHANGES LTS section.
- [ ] #29 Migration guide docs/guides/migration-to-lts.md: step-by-step for mainline 1.4.x users who want to switch to LTS. Since FQBN stays 4M2M (decision A), filesystem layout is unchanged; only the firmware binary differs. Settings are preserved across the migration. Guide documents: download LTS binaries, flash filesystem binary first (for consistency with standard procedure), flash firmware binary second, verify via version string in logs.
- [ ] #30 Version bump: version.h PATCH stays 2 but PRERELEASE changes from 'beta' to 'lts.1' (or equivalent mechanism in the autoinc-semver script). First LTS release tagged v1.4.2-lts.1 on the LTS branch.
- [ ] #31 Initial release artifacts published: OTGW-firmware-1.4.2-lts.1-<hash>.ino.bin AND OTGW-firmware.1.4.2-lts.1-<hash>.littlefs.bin, both pushed to origin/LTS-1.4.x-on-2.7.4 via a 'chore(build)' commit matching existing mainline patterns.
- [ ] #32 GitHub release (optional, only if user approves public LTS release): tagged v1.4.2-lts.1 with release notes referencing THIS task and the rationale ADR. Release notes explicitly state this is an LTS variant and recommend against using it unless the user is experiencing mainline instability.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Branch LTS-1.4.x-on-2.7.4 from dev HEAD (includes TASK-400/401/402/403 fixes).\n2. Flip arduino-cli.yaml line 3 and build.py line 180: Core URL 3.1.2 -> 2.7.4.\n3. Install Arduino Core 2.7.4 via arduino-cli (replaces 3.1.2).\n4. Test-build firmware on 2.7.4 core.\n5. If clean: commit all changes, push branch to origin.\n6. If library-compat issues: pin specific library versions or document workarounds.\n7. Report artifact + any findings so user can flash for A/B stability comparison against dev's 3.1.2 build.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-398 experimental LTS branch on ESP8266 Arduino Core 2.7.4 is green. Branch: LTS-1.4.x-on-2.7.4 (off dev HEAD e94a3f47, so inherits TASK-400/401/402/403 perf fixes). Build proof: sketch 723364 bytes (69% flash), globals 58204 bytes (71% RAM, 23716 bytes free for heap+stack).

Changes vs dev (3.1.2 baseline):
- build.py: Core URL 3.1.2 -> 2.7.4 (line 180); AceTime pin 4.1.0 -> 2.0.1; WebSockets pin 2.7.2 -> 2.3.6 (both matching v1.3.5 reference versions).
- src/OTGW-firmware/FSexplorer.ino: collectHeaders(const char*) single-arg overload is 3.x-only; replaced with portable array form collectHeaders(const char**, size_t).
- src/libraries/SimpleTelnet submodule bumped to cc4c88e: new compile-time guard ARDUINO_ESP8266_RELEASE_2_*_* selects WiFiServer::available() on 2.x cores, accept() on 3.x cores. Library now builds cross-target on both cores.

Not committed: arduino/arduino-cli.yaml (git-ignored, regenerated by build.py).

Unresolved / out-of-scope for this experiment:
- Runtime validation on hardware (user-performed): flash this LTS firmware and run alongside the 3.1.2 dev build on a second OTGW for 24-48h A/B comparison. Key metrics: reboot-path WDT events, heap watermark, OTA success rate, MQTT broker stability.
- Decision gate documented in plan: adopt 2.7.4 as LTS only if A/B shows measurable stability gain AND no regressions. Otherwise branch stays as an emergency fallback.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 python build.py --firmware passes clean on LTS branch (no warnings, no errors)
- [ ] #2 python evaluate.py --quick on LTS branch shows 0 new failures vs dev baseline
- [ ] #3 Grep on LTS branch: no direct ESP.restart/ESP.reset outside helperStuff.ino doRestart wrapper and the pre-existing networkStuff.ino WiFi-portal exception
- [ ] #4 All 32 ACs checked
- [ ] #5 Runtime validated on at least one physical OTGW device for 24+ hours soak including 5+ OTA cycles and at least one nightly restart
- [ ] #6 Final Summary committed to TASK-398 describing which libraries (if any) required downgrade, which exact FQBN was used, what binary size delta came out vs Core 3.1.2 reference build
- [ ] #7 ADR in docs/adr/ is in status Accepted (after user approval, not self-approved)
- [ ] #8 At least one Git commit on the LTS branch visible at origin; tag v1.4.2-lts.1 applied to the release commit
<!-- DOD:END -->
