---
# METADATA
Document Title: 1.2.0 Beta Branch vs Dev Branch – Root Cause Analysis
Review Date: 2026-02-21 14:45:00 UTC
Branch Reviewed: dev-branch-v1.2.0-beta-adr040-clean → dev comparison
Target Version: 1.2.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: ROOT CAUSE ANALYSIS
PR Branch: copilot/analyze-dev-branch-issues
Status: COMPLETE
---

# 1.2.0 Beta Branch Analysis vs Dev Branch

## Executive Summary

The `dev-branch-v1.2.0-beta-adr040-clean` branch is a superset of `dev` — every feature in `dev` exists in `1.2.0`, plus the new ADR-040 source-separation feature. However, a "memory optimization" commit (`29807b28`) introduced several **breaking changes** that cause the 1.2.0 branch to malfunction for existing users:

| Issue | Severity | Root Cause |
|-------|----------|-----------|
| MQTT buffer size reductions | **CRITICAL** | Silent truncation of MQTT topic/prefix settings >12 chars |
| `settingMQTTSeparateSources = true` default | **HIGH** | Memory pressure + unexpected new HA entities on upgrade |
| Scratch buffer contention in `publishToSourceTopic` | **MEDIUM** | Source-specific HA discovery permanently deferred |
| No separate `sLine` read buffer in `doAutoConfigure` | **LOW** | Code quality regression |

## Features in Dev NOT in 1.2.0

**None.** The 1.2.0 branch is forward-only — all dev capabilities are present.

## Features in 1.2.0 NOT in Dev (ADR-040)

1. **Source-specific MQTT topics** (`_thermostat`, `_boiler`, `_gateway` per WRITE/RW OT ID)
2. **HA auto-discovery** for source-specific topics (via `publishToSourceTopic`)
3. **GPIO conflict detection** (`checkGPIOConflict()`)
4. **Bloom filter deduplication** for source-specific discovery (per-source, 2048-bit)
5. `mqttseparatesources` setting exposed in REST API

## Root Causes of "No Longer Works"

### Issue 1 (CRITICAL): MQTT Buffer Size Reductions

The optimization commit reduced MQTT setting buffer sizes:

| Field | dev / stable | 1.2.0-beta | Max usable |
|-------|-------------|------------|------------|
| `settingMQTTtopTopic` | `char[41]` | `char[13]` | **12** chars |
| `settingMQTThaprefix` | `char[41]` | `char[17]` | **16** chars |
| `settingMQTTuniqueid` | `char[41]` | `char[26]` | **25** chars |

**Impact:** When `readSettings()` loads saved settings via `strlcpy(..., sizeof(field))`, any saved value longer than the new limit is **silently truncated**. A user with `settingMQTTtopTopic = "my-otgw-home"` (13 chars) would have it truncated to `"my-otgw-hom"` (12 chars), breaking all MQTT publish/subscribe routing.

The default "OTGW" (4 chars) still fits, but many real-world installations use longer topic prefixes.

**Fix:** Restore to `char settingMQTThaprefix[41]`, `char settingMQTTtopTopic[41]`, `char settingMQTTuniqueid[41]`.

### Issue 2 (HIGH): `settingMQTTSeparateSources = true` Default

With this ON by default:
- Every WRITE/RW OT parameter gets **3 additional MQTT topics** published per message
- On MQTT connect, ~90 additional HA discovery configs are attempted (30 IDs × 3 sources)
- Each discovery attempt tries to borrow the shared scratch buffer
- If the scratch is busy (during `doAutoConfigure` or `doAutoConfigureMsgid`), source discovery is **silently deferred indefinitely**

For existing users upgrading from v1.0.0/v1.1.0:
- HA receives 3× more entities than expected, cluttering dashboards
- Risk of heap exhaustion due to memory pressure
- Source-specific discovery may never complete for many sensors

**Fix:** Default to `false` (users can opt in).

### Issue 3 (MEDIUM): Scratch Buffer Contention in `publishToSourceTopic`

The current beta `publishToSourceTopic` borrows the shared MQTT auto-config scratch buffer to send HA discovery messages. This creates a deadlock-like scenario:

1. `doAutoConfigure()` acquires the scratch
2. During processing, an OT message arrives → `publishToSourceTopic` is called
3. `publishToSourceTopic` tries to acquire the scratch → **fails** (busy)
4. Source-specific HA discovery is logged as "deferred" and **never retried** until next data occurrence
5. For rarely-updated OT IDs (e.g., max CH setpoint, protocol version), this can mean discovery is **never sent**

The stable version (`dev-1.2.0-stable-version`) fixes this by simplifying `publishToSourceTopic` to only publish data values to source-specific topics — without attempting HA auto-discovery. The base-topic discovery (via `doAutoConfigure`) covers the HA integration sufficiently.

### Issue 4 (LOW): No Separate `sLine` Read Buffer

In `doAutoConfigure` and `doAutoConfigureMsgid`, the current beta reads into `sMsg` and then passes `sMsg` as both input and output to `splitLine()`. This works because `copyTrimmedField` uses `memmove`, but it creates code that is difficult to reason about. The stable version uses a separate `sLine` buffer for reads.

## Comparison with `dev-1.2.0-stable-version`

The `dev-1.2.0-stable-version` branch (updated 2026-02-21) has applied all fixes:

1. ✅ Buffer sizes restored to `[41]`
2. ✅ `settingMQTTSeparateSources = false`  
3. ✅ `publishToSourceTopic` simplified (2-arg, reads `OTdata.rsptype` directly)
4. ✅ RAII session lock (`MQTTAutoConfigSessionLock`) with proper scope
5. ✅ Separate `sLine` read buffer
6. ✅ `CACHE_SIZE = 3` (restored)

## Fix in this PR

This PR applies the same fixes as `dev-1.2.0-stable-version`:

1. **OTGW-firmware.h**: Restore MQTT buffer sizes to 40/41 chars; set `settingMQTTSeparateSources = false`; update `publishToSourceTopic` declaration
2. **MQTTstuff.ino**: Simplify `publishToSourceTopic`; add RAII lock; add separate `sLine` buffer; remove Bloom filter
3. **OTGW-Core.ino**: Update `publishToSourceTopic` calls to 2-arg form

## What Was Fixed in dev vs 1.2.0

The dev branch had a **critical inverted condition bug** in `doAutoConfigure()`:

```cpp
// dev (BROKEN): only processes IDs already configured — skips new sensors
if (bForceAll || getMQTTConfigDone(lineID))

// 1.2.0 (CORRECT): processes IDs NOT yet configured  
if (bForceAll || !getMQTTConfigDone(lineID))
```

This bug (also affecting Dallas sensors) caused HA to never receive discovery configs for rarely-updated sensors (issue #445: max CH setpoint, protocol version). The 1.2.0 branch fixes this — all configs are bulk-published on MQTT connect.
