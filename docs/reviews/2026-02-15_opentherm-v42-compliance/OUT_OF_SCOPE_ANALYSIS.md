---
# METADATA
Document Title: Deep Analysis of Out-of-Scope Issues from OpenTherm v4.2 Compliance Review
Analysis Date: 2026-02-15 16:34:00 UTC
Branch: copilot/check-opentherm-message-handling
Analyst: GitHub Copilot Advanced Agent
Document Type: Issue Analysis & Solution Evaluation
Status: ANALYSIS COMPLETE — Awaiting Decision
---

# Deep Analysis: Out-of-Scope Issues

This document provides a thorough analysis of the three issues that were intentionally skipped during the OpenTherm v4.2 compliance implementation, plus the `msglastupdated` array bounds issue identified in a prior codebase review.

---

## Issue 1: MQTT Topic Typo — `eletric_production`

### Location
- **File**: `src/OTGW-firmware/OTGW-Core.ino`, line 780
- **Context**: Inside `print_status()`, which handles OT message ID 0 (Status flags)
- **Current code**: `sendMQTTData("eletric_production", ...)`
- **Correct spelling**: `electric_production`

### Root Cause Analysis
The typo `eletric` (missing first 'c') was introduced when the Status flags decoder was written. This MQTT topic is published every time the master/slave status message (ID 0) is received — which is every ~1 second on most installations.

### Impact Assessment
- **HA Auto-Discovery**: There is **no** `eletric_production` entry in `data/mqttha.cfg`. The topic is published as raw MQTT data but has no auto-discovery config. This means:
  - Users who set up HA auto-discovery **won't see this topic** at all
  - Only users who manually configured this MQTT topic in their HA `configuration.yaml` would be affected by a rename
- **Scope of breakage**: Limited to users who manually created MQTT sensors using the misspelled topic name

### Solution Options

#### Option A: Fix typo + add HA auto-discovery (Recommended)
- **Change**: Rename `eletric_production` → `electric_production` in `OTGW-Core.ino`
- **Also**: Add a new auto-discovery entry in `data/mqttha.cfg` for `electric_production`
- **Pro**: Correct spelling, proper HA integration, clean for new users
- **Con**: Breaks manual MQTT configurations that use the typo. Since there is no HA auto-discovery for this topic, the number of affected users is likely very small.
- **Migration**: Document the change in release notes. Users can search-replace in their `configuration.yaml`.
- **Risk**: LOW — no auto-discovery entry exists, so few users would have this configured manually

#### Option B: Publish both old and new topic names (transition period)
- **Change**: Keep `eletric_production` AND add `electric_production` — both publish the same value
- **Also**: Add HA auto-discovery for `electric_production` only
- **Pro**: Zero breakage, graceful migration
- **Con**: Wastes ~40 bytes of MQTT bandwidth per status message, indefinite technical debt. Slightly more flash usage for the extra string.
- **After 2-3 releases**: Remove the old misspelled topic
- **Risk**: NONE during transition

#### Option C: Leave as-is
- **Change**: None
- **Pro**: Zero risk
- **Con**: Perpetuates the typo forever. No HA auto-discovery.
- **Risk**: NONE

### Recommendation
**Option A** is recommended. Since there is no HA auto-discovery entry for this topic, the real-world impact is very small. The typo has no auto-discovery, so most users never see it. Fix it cleanly and add proper HA discovery. Document in release notes.

If you want to be extra cautious, **Option B** provides a zero-risk transition path.

---

## Issue 2: MQTT Topic Typo — `solar_storage_slave_fault_incidator`

### Location
- **File**: `src/OTGW-firmware/OTGW-Core.ino`, line 817
- **Context**: Inside `print_solar_storage_status()`, which handles OT message ID 101 (Solar Storage Master/Slave)
- **Current code**: `sendMQTTData(F("solar_storage_slave_fault_incidator"), ...)`
- **Correct spelling**: `solar_storage_slave_fault_indicator`
- **Also in**: `src/OTGW-firmware/data/mqttha.cfg`, line 102

### Root Cause Analysis
The typo `incidator` (transposed 'i' and missing 'i'→'a') appears in **two places**:
1. The C++ source code that publishes the MQTT value
2. The HA auto-discovery configuration file that tells Home Assistant about this topic

Both must be changed together, or the auto-discovery will point to a non-existent topic.

### Impact Assessment
- **HA Auto-Discovery**: YES — there is an auto-discovery entry in `mqttha.cfg` line 102 that references `solar_storage_slave_fault_incidator`. Users who have solar storage systems will have this entity auto-discovered in HA with the misspelled name.
- **Scope of breakage**: Users with solar storage boilers who rely on this binary sensor in automations or dashboards. Solar storage is uncommon in most OTGW installations, making this a niche feature.
- **Entity naming**: The HA entity would be named `binary_sensor.<hostname>_solar_storage_slave_fault_incidator`. Renaming changes the entity ID.

### Solution Options

#### Option A: Fix both source + config simultaneously (Recommended)
- **Change**: Fix spelling in both `OTGW-Core.ino` (line 817) and `data/mqttha.cfg` (line 102)
- **Pro**: Clean fix, correct everywhere
- **Con**: HA auto-discovery will create a NEW entity with the correct name. The old entity becomes "unavailable" and must be manually deleted. Automations referencing the old entity ID break.
- **Migration**: Document in release notes. Users delete old entity and update automations.
- **Risk**: LOW — solar storage is a niche feature, few users affected

#### Option B: Fix source + keep both config entries (transition period)
- **Change**: Fix `OTGW-Core.ino` to publish `solar_storage_slave_fault_indicator`. In `mqttha.cfg`, keep the old entry AND add a new entry for the correct name. Both point to the same stat_t.
- **Problem**: This doesn't actually work cleanly — the old config entry's `stat_t` still points to the misspelled topic, but the code now publishes to the correctly-spelled topic. So the old entity would go "unavailable" anyway.
- **Better variant**: Publish to BOTH topic names (old + new) in the code, keep both config entries
- **Pro**: Truly zero breakage
- **Con**: Double MQTT messages + two entities visible in HA
- **Risk**: NONE during transition but creates confusion with duplicate entities

#### Option C: Leave as-is
- **Change**: None
- **Pro**: Zero risk
- **Con**: Perpetuates the typo. It's in HA auto-discovery too, so entity names are misspelled.
- **Risk**: NONE

### Recommendation
**Option A** is recommended. Solar storage is a niche feature with very few users. The fix is straightforward — change both files together. Document in release notes that the entity ID changes.

---

## Issue 3: Unused Struct Field `RoomRemoteOverrideFunction`

### Location
- **File**: `src/OTGW-firmware/OTGW-Core.h`, line 88
- **Declaration**: `uint16_t RoomRemoteOverrideFunction = 0;`
- **Also**: Referenced in OTmap at line 446 as the label for ID 100

### Root Cause Analysis
The `OTdataStruct` has **two** fields related to the same concept:
1. `RoomRemoteOverrideFunction` (line 88) — in the "RF" section of the struct, **NEVER assigned** anywhere in the code
2. `RemoteOverrideFunction` (line 146) — in the "Statistics" section, **actually used** by `print_remoteoverridefunction()` and `getOTGWValue()`

The code flow for ID 100:
- `processOT()` calls `print_remoteoverridefunction(OTcurrentSystemState.RemoteOverrideFunction)` — uses field #2
- `getOTGWValue()` returns `String(OTcurrentSystemState.RemoteOverrideFunction)` — uses field #2
- OTmap entry at line 446: label is `"RoomRemoteOverrideFunction"` — this is the **MQTT topic name**, not a code reference

So `RoomRemoteOverrideFunction` (field #1) is **truly dead code** — it's declared, initialized to 0, but never written to or read from by any code. The only place the string "RoomRemoteOverrideFunction" appears is as an MQTT topic label in OTmap.

### Impact Assessment
- **Memory**: 2 bytes of RAM wasted (trivial on ESP8266, but every byte counts)
- **Confusion**: Having two similarly-named fields for the same concept is error-prone
- **External dependencies**: No external code reads this struct — it's internal to the firmware

### Solution Options

#### Option A: Remove the unused field (Recommended)
- **Change**: Delete line 88 (`uint16_t RoomRemoteOverrideFunction = 0;`)
- **Pro**: Removes dead code, eliminates confusion, saves 2 bytes RAM
- **Con**: None — the field is verifiably never used
- **Risk**: NONE — no code reads or writes this field

#### Option B: Consolidate — remove `RemoteOverrideFunction` and use `RoomRemoteOverrideFunction`
- **Change**: Delete `RemoteOverrideFunction` (line 146), update all code references to use `RoomRemoteOverrideFunction` instead
- **Pro**: The name `RoomRemoteOverrideFunction` matches the OTmap label better
- **Con**: More code changes, higher risk of typos
- **Risk**: LOW but more changes needed

#### Option C: Leave as-is
- **Change**: None
- **Pro**: Zero risk
- **Con**: Dead code, confusing naming
- **Risk**: NONE

### Recommendation
**Option A** is recommended. Simple one-line deletion. The field is provably unused. If you prefer cleaner naming, Option B renames the working field to match the OTmap label.

---

## Issue 4: OTmap Array Bounds — `msglastupdated[255]` and `OTmap[OTdata.id]`

### Location
Multiple locations in `src/OTGW-firmware/OTGW-Core.ino`:

**Bug 4a — `msglastupdated` OOB write**:
- **File**: `OTGW-Core.h`, line 488: `time_t msglastupdated[255] = {0};`
- **File**: `OTGW-Core.ino`, line 1657: `msglastupdated[OTdata.id] = now;`
- **Problem**: Array has 255 elements (indices 0-254). `OTdata.id` is `uint8_t` (0-255). When `id == 255`, this writes 1 element past the end of the array.

**Bug 4b — `OTmap` OOB read**:
- **File**: `OTGW-Core.ino`, line 1660: `PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem);`
- **Problem**: `OTmap` has 134 entries (indices 0-133). `OTdata.id` can be 0-255. For any id > 133, this reads from PROGMEM beyond the array bounds.
- **Note**: The `messageIDToString()` function at line 474-478 correctly checks `message_id <= OT_MSGID_MAX` before accessing OTmap, but line 1660 does NOT.

### Root Cause Analysis
The OpenTherm protocol uses 8-bit message IDs (0-255), but the firmware only defines entries for IDs 0-133 in OTmap. The code at line 1660 assumes all IDs are within OTmap bounds, but boilers or thermostats could send any ID 0-255.

For `msglastupdated`, the array was apparently sized to "cover all possible IDs" (the comment says "all msg, even if they are unknown") but was declared as `[255]` instead of `[256]`.

### Impact Assessment
- **Bug 4a**: If a boiler sends message ID 255, `msglastupdated[255]` writes 4-8 bytes (sizeof time_t) past the end of the array into whatever follows it in memory. This could corrupt other global variables. **Memory corruption risk**.
- **Bug 4b**: If a boiler sends message ID > 133, `PROGMEM_readAnything` reads arbitrary PROGMEM data into `OTlookupitem`. This could cause the firmware to try to publish MQTT with garbage topic names, crash from null pointers, or behave unpredictably. **Potential crash**.
- **Real-world likelihood**: Most boilers stick to standard IDs (0-127), but some OEM-specific boilers may use higher IDs (Remeha uses 131-133). An ID of 255 is unlikely but not impossible (malformed data, parity error).

### Solution Options

#### Option A: Fix array size + add bounds check (Recommended)
- **Change 1**: `msglastupdated[255]` → `msglastupdated[256]` — fixes OOB for id=255
- **Change 2**: Add bounds check before OTmap access at line 1660:
```cpp
if (OTdata.id <= OT_MSGID_MAX) {
    PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem);
} else {
    // Initialize OTlookupitem with safe defaults for unknown IDs
    OTlookupitem.id = OTdata.id;
    OTlookupitem.msgcmd = OT_UNDEF;
    OTlookupitem.type = ot_undef;
    strlcpy(OTlookupitem.label, "", sizeof(OTlookupitem.label));
    strlcpy(OTlookupitem.friendlyname, "", sizeof(OTlookupitem.friendlyname));
    strlcpy(OTlookupitem.unit, "", sizeof(OTlookupitem.unit));
}
```
- **Pro**: Fixes both bugs, prevents memory corruption, handles unknown IDs gracefully
- **Con**: 4-8 bytes more RAM for `msglastupdated[256]`, small code addition
- **Risk**: VERY LOW — purely defensive changes

#### Option B: Minimal fix — just fix the sizes
- **Change**: `msglastupdated[255]` → `msglastupdated[256]`
- **Change**: Add early-return for `OTdata.id > OT_MSGID_MAX`
```cpp
if (OTdata.id > OT_MSGID_MAX) {
    AddLogf("Unknown message ID [%d]", OTdata.id);
    return; // or continue, depending on context
}
```
- **Pro**: Minimal change, prevents crash
- **Con**: Silently ignores unknown IDs — no logging of their values
- **Risk**: VERY LOW

#### Option C: Full defensive approach
- All of Option A, plus:
- Add bounds checks to ALL array accesses indexed by `OTdata.id` in `processOT()`
- Add validation when parsing the OpenTherm frame
- **Pro**: Maximum safety
- **Con**: More code, more testing needed
- **Risk**: LOW but largest changeset

### Recommendation
**Option A** is recommended. Both bugs are memory safety issues that should be fixed. The `msglastupdated` fix is a one-character change (`255` → `256`). The OTmap bounds check is a few lines that prevent reading random PROGMEM data. The changes are purely defensive and can't break existing functionality.

---

## Summary & Decision Matrix

| Issue | Severity | Risk of Fix | Recommended Solution | Breaking? |
|-------|----------|-------------|---------------------|-----------|
| 1. `eletric_production` typo | Low | Very Low | Option A: Fix + add HA discovery | Minimal (no auto-discovery existed) |
| 2. `fault_incidator` typo | Low | Low | Option A: Fix both code + config | Yes (entity rename, niche feature) |
| 3. Unused `RoomRemoteOverrideFunction` | Very Low | None | Option A: Remove dead field | No |
| 4. Array bounds bugs | **High** | Very Low | Option A: Fix size + add check | No |

### Suggested Implementation Order

1. **Issue 4 first** (array bounds) — this is a memory safety bug, highest priority
2. **Issue 3** (unused field) — trivial one-line fix, zero risk
3. **Issue 1** (`eletric_production`) — low risk since no auto-discovery exists
4. **Issue 2** (`fault_incidator`) — slightly higher risk due to HA entity rename

### Breaking Change Strategy

For the MQTT topic renames (Issues 1 and 2):
- **Release notes**: Clearly document the renamed topics
- **Version bump**: Include in a minor version bump (not patch)
- **Wiki update**: Update the MQTT topic documentation
- **No dual-publish needed**: Both topics are binary sensors for niche features, and the renaming is a one-time migration
