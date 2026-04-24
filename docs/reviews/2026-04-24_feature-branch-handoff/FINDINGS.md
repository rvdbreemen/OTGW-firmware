# Feature Branch Review Handoff

Date: 2026-04-24
Branch: `feature-dev-2.0.0-otgw32-esp32-sat-support`
Reviewed HEAD: `f145a53e`
Review scope used: `e1f8e070..HEAD`

## Context

This review focused on the latest post-merge commit stack after merge commit
`e1f8e070`:

- `3384f6a9` guard `maybeWarnFlashMismatch`
- `38f57c5a` platform heap/diagnostic abstractions
- `08585837` ESP8266 PlatformIO pin to Core 2.7.4
- `b704243d` helper forward declarations for GCC 4.8.2
- `bb3f95f7` remove BGTRACE probes
- `5a4f47b1` route raw `ESP.getFreeHeap()` calls through `platformFreeHeap()`
- `0211f5fc` route max-free-block calls through `platformMaxFreeBlock()`
- `c90c94a5` ESP32 OTA deferred reboot path
- `be52c529` backlog task closure
- `f145a53e` ADR-082 and ADR-083 documentation

The local worktree was dirty before this handoff. Do not overwrite these unless
the user explicitly asks:

- Deleted: `logs-issue/DHW_Debug.txt`
- Deleted: `logs-issue/logout.txt`
- Deleted: `logs-issue/malformed_packets.txt`
- Deleted: `logs-issue/mqtt-01JXSX5BR2SZR76CNXTGQ0MJ06-OpenTherm Gateway (OTGW)-d7ae4d508faf5e84e2e33c79f872a380.json`
- Modified: `src/OTGW-firmware/data/version.hash`
- Modified: `src/OTGW-firmware/version.h`

Git also warns that it cannot read
`C:\Users\rvdbr\.config\git\ignore` due to permission denied.

## Finding 1: ESP32 OTA success path likely never reaches deferred reboot

Severity: High

Files:

- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h`

Problem:

The HTTP POST completion handler checks `_authenticated` before sending the
success response and scheduling the reboot:

```cpp
if (!_authenticated)
  return _server->requestAuthentication();
```

But `_handleUploadEnd()` resets `_authenticated = false` at the end of upload
processing, before the POST completion lambda is expected to run. That means the
new success branch with:

```cpp
logBootSignature("[OTA] pre-reboot");
requestDeferredReboot("[OTA] Rebooting...");
```

is likely skipped and replaced by an authentication challenge after a successful
upload. This breaks the intended ESP32 OTA reboot flow introduced by
`c90c94a5`.

Relevant lines at review time:

- POST completion guard: `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:83`
- deferred reboot scheduling: `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:107`
- premature auth reset: `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:300`

Recommended fix:

Keep the authenticated upload-completion state alive until the POST completion
handler has sent success or error and queued the reboot. Move the reset/cleanup
out of `_handleUploadEnd()` and into the POST handler after response handling.

One safe shape:

```cpp
// In _handleUploadEnd():
::state.flash.bESPactive = false;
// Do not clear _authenticated or _resetUploadTracking() here.

// In the POST lambda:
if (Update.hasError() || _updaterError[0] != '\0') {
  ...
  _authenticated = false;
  _resetUploadTracking();
} else {
  ...
  logBootSignature("[OTA] pre-reboot");
  requestDeferredReboot("[OTA] Rebooting...");
  _authenticated = false;
  _resetUploadTracking();
}
```

Be careful with `_uploadTarget`: the error path uses it to remount LittleFS if
needed, so do not call `_resetUploadTracking()` before the error path has
finished using it.

Verification after fix:

1. Build ESP32: `pio run -e esp32`
2. Build ESP8266 to catch shared header regressions: `pio run -e esp8266`
3. Exercise ESP32 OTA upload:
   - Upload a firmware image through `/update`.
   - Confirm browser receives success HTML.
   - Confirm logs include `[OTA] post-end`, `[OTA] pre-reboot`, deferred request,
     and `performing deferred reboot`.
   - Confirm device reboots and comes back without requiring manual reset.
4. Exercise ESP32 filesystem OTA:
   - Confirm LittleFS remount and settings restore still happen.
   - Confirm the reboot path runs only after POST success handling.

## Finding 2: `pio run` documentation says all targets, but config builds only ESP8266

Severity: Medium

Files:

- `platformio.ini`
- `docs/adr/ADR-083-platformio-as-primary-build-system.md`

Problem:

`platformio.ini` documents:

```ini
;   pio run                     # Build all targets
```

ADR-083 similarly says:

```text
pio run                     # build all targets (default_envs)
```

But `platformio.ini` currently has:

```ini
default_envs = esp8266
```

Plain `pio run` therefore builds only ESP8266. On a dual-target 2.0.0 branch,
that creates a release/CI footgun because ESP32 can silently skip validation.

Relevant lines at review time:

- `platformio.ini:7`
- `platformio.ini:13`
- `docs/adr/ADR-083-platformio-as-primary-build-system.md:60`

Recommended fix:

Choose one of these and make docs/config agree:

Option A, build both by default:

```ini
[platformio]
default_envs = esp8266, esp32
```

This matches the existing comments and ADR-083. It is the better fit if CI or
release validation should treat both targets as mandatory.

Option B, keep ESP8266 as the default:

```text
pio run -e esp8266
pio run -e esp32
```

Then update comments and ADR-083 to stop saying plain `pio run` builds all
targets.

Verification after fix:

1. Run `pio project config` or `pio run -v` and confirm the default env behavior.
2. Run `pio run` and verify whether one or both environments build according to
   the chosen decision.
3. Ensure CI/release scripts use explicit envs or rely on the corrected
   `default_envs`.

## Verification Already Performed

Commands run:

```powershell
git diff --stat e1f8e070..HEAD
git diff --name-status e1f8e070..HEAD
git log --oneline --first-parent e1f8e070..HEAD
rg "ESP\.getFreeHeap|ESP\.getMaxFreeBlockSize|getHeapFragmentation|getMinFreeHeap|getResetInfoPtr|getMaxFreeBlockSize|BGTRACE|BGTASKS_TRACE|MQTT_MAX_FREE_BLOCK" src platformio.ini docs/adr -n
rg "platformFreeHeap|platformMaxFreeBlock|platformHeapFragmentation|platformMinFreeHeap|platformExceptionCause|logBootSignature|requestDeferredReboot|doRestart" src/OTGW-firmware -n
git diff --check e1f8e070..HEAD
git diff --check
```

Results:

- `git diff --check e1f8e070..HEAD` reported no whitespace errors.
- `git diff --check` on the dirty worktree also reported no whitespace errors.
- Raw `ESP.getFreeHeap()` / `ESP.getMaxFreeBlockSize()` usage in
  application code was routed through platform wrappers. Remaining direct calls
  are inside platform headers or comments/docs/examples.
- BGTRACE/BGTASKS trace probes were removed from the main background task path.

Build not run:

- `pio run -e esp8266` failed because `pio` is not on PATH in this shell.
- `python -m platformio --version` and `py -m platformio --version` also failed
  because `python` and `py` are not on PATH.

## Suggested Next Agent Plan

1. Fix Finding 1 first; it is a behavior regression in the newly added ESP32 OTA
   success path.
2. Fix Finding 2 by choosing whether `pio run` should build both environments or
   docs should require explicit environment builds.
3. Run `pio run -e esp8266` and `pio run -e esp32` in an environment where
   PlatformIO is available.
4. Manually test ESP32 OTA firmware and filesystem updates if hardware or a
   suitable integration setup is available.
5. Leave the pre-existing dirty `logs-issue/*`, `version.h`, and `version.hash`
   changes alone unless the user instructs otherwise.
