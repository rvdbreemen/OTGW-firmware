# Verbose Debug Auto-Enable for Alpha/Beta Prerelease Builds Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Automatically enable all pure-logging debug toggles in `state.debug` when the firmware's prerelease tag contains "alpha" or "beta".

**Architecture:** A new `enableDebugForPrerelease()` function in `debugStuff.ino` sets the five toggles that are `false` by default. It is called once from `setup()` after `startTelnet()`, guarded by `#if defined(_VERSION_PRERELEASE)` + a runtime `strstr` on `_SEMVER_FULL`. Release builds (no `_VERSION_PRERELEASE`) see zero overhead — the entire block is stripped by the preprocessor.

**Tech Stack:** Arduino C++ (ESP32), PlatformIO (`pio run`), telnet debug port 23.

---

### Task 1: Add `enableDebugForPrerelease()` to `debugStuff.ino` and its forward declaration to `debugStuff.h`

**Files:**
- Modify: `src/OTGW-firmware/debugStuff.ino` (append after line 114)
- Modify: `src/OTGW-firmware/debugStuff.h` (append forward declaration after line 92)

- [ ] **Step 1: Append the function to `debugStuff.ino`**

Add the following block at the end of `src/OTGW-firmware/debugStuff.ino` (after line 114, the last line of `_debugBOL`):

```cpp
void enableDebugForPrerelease() {
  state.debug.bRestAPI    = true;
  state.debug.bMQTT       = true;
  state.debug.bMQTTGate   = true;
  state.debug.bSensors    = true;
  state.debug.bSATBLE     = true;
  DebugTln(F("[prerelease] verbose debug enabled: restapi mqtt mqtt_gate sensors sat_ble"));
}
```

Note: `bOTGWSimulation` and `bSensorSim` are intentionally NOT set — they simulate behaviour, not just log it.

- [ ] **Step 2: Add the forward declaration to `debugStuff.h`**

In `src/OTGW-firmware/debugStuff.h`, after line 92 (the existing `void _debugBOL(...)` declaration), add:

```cpp
void enableDebugForPrerelease();
```

The declaration block at the bottom of `debugStuff.h` will then read:

```cpp
void _debugPrintf_P(PGM_P fmt, ...);
void _debugBOL(const char *fn, int line);
void enableDebugForPrerelease();
```

- [ ] **Step 3: Verify the build compiles clean**

Run from the repo root:
```
pio run
```
Expected: exits 0, no errors, no new warnings related to `enableDebugForPrerelease`.

- [ ] **Step 4: Commit**

```bash
git add src/OTGW-firmware/debugStuff.ino src/OTGW-firmware/debugStuff.h
git commit -m "feat: add enableDebugForPrerelease() to debugStuff (TASK-721)"
```

---

### Task 2: Call `enableDebugForPrerelease()` from `setup()` after `startTelnet()`

**Files:**
- Modify: `src/OTGW-firmware/OTGW-firmware.ino` (around line 179)

- [ ] **Step 1: Insert the call in `setup()`**

In `src/OTGW-firmware/OTGW-firmware.ino`, locate line 179:
```cpp
  startTelnet();              // start the debug port 23
```

Insert the following block immediately after that line (before `startMDNS`):

```cpp
  startTelnet();              // start the debug port 23
#if defined(_VERSION_PRERELEASE)
  if (strstr(_SEMVER_FULL, "alpha") || strstr(_SEMVER_FULL, "beta"))
    enableDebugForPrerelease();
#endif
```

`_SEMVER_FULL` is defined in `version.h` as a quoted string literal (e.g. `"2.0.0-alpha.71+c9f1a84"`). `strstr` on a string literal is safe on ESP32 (linear flash address space). The `#if defined(_VERSION_PRERELEASE)` outer guard strips the entire block — including the `strstr` call — in release builds.

The call is placed after `startTelnet()` so the `DebugTln` inside `enableDebugForPrerelease()` reaches the telnet client rather than being silently dropped.

- [ ] **Step 2: Verify the build compiles clean**

```
pio run
```
Expected: exits 0, no new warnings.

- [ ] **Step 3: Verify the `#if` guard works for release (smoke check)**

Temporarily comment out the `#define _VERSION_PRERELEASE` line in `src/OTGW-firmware/version.h` and run:
```
pio run
```
Expected: still exits 0. The `enableDebugForPrerelease()` call disappears entirely.
Restore the line afterwards.

- [ ] **Step 4: Commit**

```bash
git add src/OTGW-firmware/OTGW-firmware.ino
git commit -m "feat: auto-enable verbose debug for alpha/beta prerelease builds (TASK-721)"
```

---

### Task 3: Mark AC items done and close TASK-721

- [ ] **Step 1: Check all acceptance criteria**

```bash
backlog task edit 721 --check-ac 1 --check-ac 2 --check-ac 3 --check-ac 4 --check-ac 5
```

- [ ] **Step 2: Add final summary**

```bash
backlog task edit 721 --final-summary "Added enableDebugForPrerelease() to debugStuff.ino (sets bRestAPI, bMQTT, bMQTTGate, bSensors, bSATBLE to true). Forward declaration added to debugStuff.h. setup() calls it after startTelnet() guarded by #if defined(_VERSION_PRERELEASE) + strstr on _SEMVER_FULL. bOTGWSimulation and bSensorSim excluded by design. Build verified clean."
```

- [ ] **Step 3: Mark task Done**

```bash
backlog task edit 721 -s Done
```

- [ ] **Step 4: Push to origin**

```bash
git push origin feature-dev-2.0.0-otgw32-esp32-sat-support
```
