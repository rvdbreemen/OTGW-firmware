---
# METADATA
Document Title: GPIO Conflict Regression and OTA Observability Post-Mortem
Review Date: 2026-03-16 00:20:00 UTC
Branch Reviewed: dev-branch-1.3-improved-setting-and-state-structures -> dev
Target Version: 1.3.0-beta
Reviewer: GitHub Copilot
Document Type: Post-Mortem Assessment
PR Branch: dev-branch-1.3-improved-setting-and-state-structures
Commit: 56f19aaea7592ae3b35f264f483b4c25d826ef66
Status: COMPLETE
---

# GPIO Conflict Regression and OTA Observability Post-Mortem

## Executive Summary

This incident started as a field-visible crash after flashing the filesystem image and rebooting the device. The crash was traced to the settings loading path, specifically GPIO conflict validation. The regression was caused by using a `PGM_P` string token as an internal discriminator and then comparing it with `strcasecmp_P()`, effectively treating a flash-memory pointer as a RAM string.

The final production fix replaced the string discriminator with a strongly typed enum declared in the shared header. That removed the unsafe RAM-vs-flash ambiguity and restored a cleaner API.

During the same investigation window, OTA upload observability and UI polish were improved:

- The OTA handler now reports XHR upload start, progress, completion, and abort events block by block for both firmware and filesystem uploads.
- OTA logs that are relevant to operators now use timestamped `DebugT*` macros consistently.
- The web footer now reserves explicit spacing between the heap indicator and the copyright text.

## Incident Summary

### User-visible symptoms

- Device crashed after flashing the filesystem and reconnecting over USB serial.
- The crash did not reproduce on version 1.2.0, which strongly indicated a regression in newer changes.
- OTA telnet output initially lacked enough upload visibility for block-level diagnosis.
- One OTA success message lacked a timestamp, which made the log stream inconsistent.
- The heap footer text in the UI rendered too tightly against adjacent text.

### Impact

- Filesystem flash workflows became unreliable.
- Boot-time settings parsing could hard-fail instead of degrading gracefully.
- Field diagnostics were slower because OTA upload state was not fully visible in telnet.
- UI footer readability was reduced.

## Technical Root Cause

### Primary cause: unsafe PROGMEM string discrimination

The original GPIO conflict helper accepted a `PGM_P caller` argument and then used string comparison to decide which logical caller was active. That design was fragile on two levels:

1. It encoded internal program state as strings instead of using an enum.
2. It mixed flash-resident string handling and RAM string expectations in a way that is unsafe on ESP8266.

The result was a crash during settings processing, reached from the boot path:

- `readSettings()`
- `updateSetting()`
- `checkGPIOConflict()`

This is the key lesson from the failure: on ESP8266, PROGMEM misuse is not a soft bug. Pointer-domain mistakes can produce immediate exceptions.

### Why the final fix is correct

The final fix moved the discriminator into a strongly typed shared declaration:

- `enum class GPIOConflictCaller : uint8_t` in `OTGW-firmware.h`
- `bool checkGPIOConflict(int pin, GPIOConflictCaller caller);`

That change improved the code in three ways:

1. The compiler now enforces the valid caller set.
2. The helper no longer depends on string identity or flash-pointer semantics.
3. The API better reflects intent: this is internal control flow, not text processing.

## Contributing Factors

### 1. Internal branching was represented as text

Using text tokens for internal logic creates avoidable failure modes:

- spelling drift
- inconsistent storage class assumptions
- unnecessary runtime comparisons
- difficulty catching errors at compile time

### 2. Arduino sketch auto-prototype behavior complicated the first clean fix

The first attempt to fix the issue with an enum ran into Arduino `.ino` auto-prototype limitations. That led to a temporary `uint8_t` workaround to validate the logic quickly. The eventual proper solution was to move the enum and function declaration into the header.

This is a good example of a valid two-step repair strategy:

- first restore correctness quickly to prove the diagnosis
- then restore design quality with the proper shared declaration

### 3. Observability was insufficient during OTA uploads

The OTA path handled both firmware and filesystem uploads, but the telnet logs did not provide enough block-level visibility into XHR upload flow. That made it harder to separate transport progress from flash finalization.

### 4. Log formatting was inconsistent by macro choice

The missing timestamp was not random. It came from using `Debugf()` instead of `DebugTf()`. The distinction matters:

- `Debug*` prints directly
- `DebugT*` prefixes the line with timestamp and source context

This means timestamp consistency is not an emergent property. It is an explicit macro-selection choice.

## Fix Assessment

### GPIO regression fix

**Assessment**: Strong

Why it is strong:

- fixes the root cause, not the symptom
- restores type safety
- avoids future flash/RAM confusion in this path
- matches the project’s preference for explicit, bounded, low-fragility patterns

### OTA observability fix

**Assessment**: Strong

Why it is strong:

- implemented once in the shared upload path
- works for both firmware and filesystem uploads
- exposes start, progress, complete, and abort lifecycle stages
- adds operator-visible detail without changing upload semantics

### Footer spacing fix

**Assessment**: Appropriate and low risk

Why it is appropriate:

- the spacing is structural in the markup
- it does not depend on the heap string contents
- it avoids baking whitespace into runtime text assembly

## What Worked Well

1. The crash was debugged against a rebuilt ELF instead of guessing from the exception alone.
2. The comparison to version 1.2.0 helped confirm that the issue was a regression, not a longstanding latent bug.
3. The temporary workaround was used as a diagnostic bridge, not mistaken for the final design.
4. The final implementation corrected the API shape rather than preserving a weak abstraction.
5. OTA improvements were applied in the shared handler, avoiding duplicated logic for firmware vs filesystem.

## What Did Not Go Well

1. The original implementation allowed text-based internal branching into a hardware-sensitive path.
2. The initial OTA logging did not provide enough operational visibility.
3. Timestamp consistency was not treated as part of the logging contract.
4. A later rebuild surfaced unrelated merge-conflict markers in `version.h`, which shows workspace hygiene checks were not fully separate from change verification.

## What We Learned

### 1. Use enums for internal control flow

If a function argument selects one of a small set of internal behaviors, use:

- enum class
- integer IDs
- dedicated boolean flags when truly binary

Do not use user-facing or flash-resident strings as internal discriminators unless text is genuinely part of the problem domain.

### 2. Treat flash/RAM boundaries as correctness boundaries

On ESP8266, APIs involving:

- `PGM_P`
- `__FlashStringHelper`
- `strcmp_P()` / `strcasecmp_P()` / `memcmp_P()`

must be chosen based on actual storage domain and actual data type. A wrong choice can crash the device.

### 3. Temporary fixes should reduce time-to-diagnosis, not become permanent

The `uint8_t` workaround was useful as a probe, but the proper finish was the typed enum in the header. That is the right pattern for embedded incident response.

### 4. Operator logs need a formatting contract

For logs that users depend on during firmware updates or recovery:

- prefer timestamped macros by default
- keep event names explicit
- report progress in stable, parseable fields

### 5. UI polish still benefits from structural fixes

Even a small presentation issue should be fixed at the structure or style layer when possible, not by stuffing whitespace into generated text.

### 6. Verification should check both the change and the tree

The later build interruption from `version.h` merge markers was unrelated to the actual OTA fix, but still important. Verification should distinguish:

- change-specific regressions
- unrelated workspace breakage

That separation improves decision quality.

## Preventive Actions

### Code-level rules

1. Do not use string tokens for internal branch selection when an enum is available.
2. Avoid passing `PGM_P` as a semantic discriminator.
3. Use `_P` string APIs only when the storage domain and data model match.
4. Use `DebugT*` for operator-visible lifecycle logging.

### Review checklist additions

When reviewing embedded changes, explicitly ask:

1. Is this value really data, or is it an enum disguised as text?
2. Does this pointer live in RAM or flash?
3. Would the wrong string helper here crash the device?
4. Will this log line be useful in the field, and does it include a timestamp?
5. Is this UI spacing/layout concern fixed structurally instead of with ad hoc text padding?

### Testing improvements

1. Include filesystem-flash-and-reboot scenarios in validation.
2. Keep at least one regression comparison point against a known-good release.
3. Validate OTA behavior from both browser UI and telnet operator logs.
4. Add a pre-build sanity check for unresolved merge markers in generated or versioned files.

## Final Assessment

The main regression fix is high quality because it removed a fragile abstraction and replaced it with a compile-time-safe one. The OTA logging work is also high value because it improves field diagnosability without widening the runtime surface area significantly. The footer spacing fix is small but correctly implemented.

The biggest transferable lesson is straightforward:

**In embedded firmware, type safety and storage-domain correctness are operational reliability features.**

When internal state is represented explicitly and logs are designed for operators, both failure rate and recovery time improve.
