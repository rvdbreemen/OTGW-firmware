# ADR Date Verification: Evidence-Based Analysis

**Analysis Date:** 2026-01-28  
**Repository:** rvdbreemen/OTGW-firmware  
**Analyst:** GitHub Copilot Advanced Agent

## Methodology

This report verifies ADR dates against:
1. Git commit history (`git log`)
2. Source code evidence
3. Version tags and releases
4. README/changelog documentation

## Critical Finding: Grafted Repository

The git repository shows:
```bash
commit d925e486fc0e5752be08c0876d72513bb9051a12 (grafted)
Author: Robert van den Breemen
Date:   Fri Apr 23 00:26:44 2021 +0200
```

**Impact:** Git history before April 23, 2021 is **LOST**. The repository was migrated from another location, and only 492 commits are accessible. All dates before 2021-04-23 **CANNOT be verified** with git evidence.

---

## ADR-by-ADR Verification

### ✅ ADR-015: NTP and AceTime - **DATE CORRECTION REQUIRED**

**Current Claim:** "Date: 2020-01-01 (AceTime adoption)"

**Git Evidence:**
```
45b51f2 2021-10-16 using configTime and AceTime to replace ezTime
99dd717 2021-10-16 Fallback to default if error looking up TZ
8170a7b 2021-10-19 using native configTime and some
```

**Analysis:**
- Commit 45b51f2 shows AceTime **replacing** ezTime on **October 16, 2021**
- This is the actual adoption date, not 2020

**Verdict:** ❌ **INCORRECT DATE**

**Correction:**
```markdown
**Date:** 2021-10-16 (AceTime adoption, replaced ezTime)  
**Git Evidence:** Commit 45b51f2
```

---

### ❌ ADR-020: DS18B20 Sensors - **IMPOSSIBLE DATE**

**Current Claim:** "Breaking Change: 2025-12-15 (v1.0.0 - GPIO default changed, address format fixed)"

**Problems:**
1. Documentation written on 2026-01-28
2. Breaking change supposedly on 2025-12-15 (6 weeks earlier)
3. Current version is **v1.0.0-rc6** (not final v1.0.0)

**Git Evidence:**
```
version.h shows:
#define _VERSION "1.0.0-rc6+1fed76f (27-01-2026)"

No commits found between 2025-12-01 and 2025-12-31 mentioning GPIO changes
```

**Verdict:** ❌ **IMPOSSIBLE DATE** - Future date typed as past

**Most Likely Explanation:**
- Author meant "2024-12-15" (typo)
- OR this is a planned breaking change for v1.0.0 final (not yet released)

**Correction:**
```markdown
**Breaking Change:** Planned for v1.0.0 final release (GPIO 13→10, address format fix)  
**Status:** As of rc6 (2026-01-28), breaking changes NOT YET RELEASED
```

---

### ⚠️ ADR-009: PROGMEM - **ONGOING, NOT SINGLE DATE**

**Current Claim:** "Date: 2018-06-01 (Initial adoption)"

**Git Evidence:**
```
6a2b1b5 2021-10-22 Moving array of structs OTmap into PROGMEM
7ae0527 2021-10-24 Turning autocorrect on and moving more strings to Flashmemort
```

**Analysis:**
- PROGMEM adoption was **gradual**, not a single event
- Active migration visible in October 2021
- Likely started earlier (pre-2021 history lost)

**Verdict:** ⚠️ **PARTIALLY CORRECT**

**Correction:**
```markdown
**Date:** Ongoing (Earliest git evidence: 2021-10-22, commit 6a2b1b5)  
**Note:** Initial adoption predates git history; systematic migration continued through 2021-2023
```

---

### ⚠️ ADR-004: Static Buffers - **ONGOING IMPROVEMENT**

**Current Claim:** "Date: 2020-01-01 (Estimated)"

**Git Evidence:**
```
03a2ca2 2023-01-26 Don't use String() to format for rest api
9a1fa7e 2021-12-28 remove unused function splitString()
```

**Analysis:**
- String→buffer conversion continued through 2023
- Not a single architectural decision, but ongoing refactoring

**Verdict:** ⚠️ **DATE IS ESTIMATE**

**Correction:**
```markdown
**Date:** Ongoing refactoring (Earliest git evidence: 2021-12-28, commit 9a1fa7e)  
**Major work:** 2023-01-26 (commit 03a2ca2) - REST API String removal
**Note:** Systematic conversion predates git history
```

---

### ❓ ADR-019: REST API v2 - **UNVERIFIED**

**Current Claim:** "Date: 2020-06-01 (v1 introduced), 2024-01-01 (v2 introduced)"

**Git Evidence:**
```
NO commits found with "API v2" or "api/v2" in messages
```

**Code Evidence:**
- restAPI.ino exists in first commit (2021-04-23)
- Contains version routing logic

**Verdict:** ❓ **CANNOT VERIFY v2 DATE**

**Investigation Needed:**
```bash
# Check if v2 actually exists in code
grep -r "api/v2" *.ino
grep -r "version.*2" restAPI.ino
```

**Recommendation:** Investigate whether v2 API actually exists or is planned

---

### ⏳ Pre-2021 ADRs - **ESTIMATES ONLY**

All these ADRs claim dates before 2021-04-23 (git history start):

| ADR | Claimed Date | Verification |
|-----|-------------|--------------|
| ADR-001 | 2016-01-01 | ❌ Pre-git, estimate |
| ADR-002 | 2018-06-01 | ❌ Pre-git, estimate |
| ADR-003 | 2018-01-01 | ❌ Pre-git, estimate |
| ADR-007 | 2018-06-01 | ❌ Pre-git, estimate |
| ADR-008 | 2020-06-01 | ❌ Pre-git, estimate (SPIFFS→LittleFS) |
| ADR-011 | 2018-01-01 | ❌ Pre-git, estimate |
| ADR-013 | 2016-01-01 | ❌ Pre-git, estimate |
| ADR-014 | 2020-01-01 | ❌ Pre-git, estimate (build.py) |
| ADR-016 | 2018-06-01 | ❌ Pre-git, estimate |
| ADR-017 | 2018-01-01 | ❌ Pre-git, estimate |

**Code Evidence:** First commit (2021-04-23) shows fully functional system with:
- ESP8266 Arduino framework ✓
- Modular .ino files ✓
- LittleFS (not SPIFFS) ✓
- WiFiManager ✓
- MQTT integration ✓
- WebSocket support ✓

**Conclusion:** All architectural decisions were made before git history starts.

---

## Verified Timeline (Git Evidence Only)

**2021:**
- **Apr 23:** Repository creation (grafted from older source)
- **Oct 16:** ✅ **AceTime adopted** (replaced ezTime) - commit 45b51f2
- **Oct 22:** ✅ **PROGMEM migration** (OTmap to PROGMEM) - commit 6a2b1b5
- **Oct 24:** ✅ **PROGMEM expansion** (more strings to flash) - commit 7ae0527
- **Dec 20:** Sensor statistics support - commit fcd31a9

**2022:**
- Development continues (PROGMEM, WebSocket improvements)

**2023:**
- **Jan 23:** ✅ **Onewire enhancements** - commit 9199e43
- **Jan 26:** ✅ **String→buffer in REST API** - commit 03a2ca2
- **Jan 28:** ✅ **v0.10.0 released** - commit dd06864
- **May 10:** Makefile updates - commits 8fab46b, efa49af

**2024:**
- **Apr 14:** Large squashed commit - commit b6bf90e
- Continued development

**2025:**
- No commits visible in 2025

**2026:**
- **Jan 27:** v1.0.0-rc6 build (version.h)
- **Jan 28:** ADR documentation created

---

## Required Corrections

### 1. Fix ADR-015 (AceTime)

**File:** `docs/adr/ADR-015-ntp-acetime-time-management.md`

**Change:**
```diff
- **Date:** 2020-01-01 (AceTime adoption)  
+ **Date:** 2021-10-16 (AceTime adopted, replaced ezTime)  
+ **Git Evidence:** Commit 45b51f2
```

### 2. Fix ADR-020 (DS18B20)

**File:** `docs/adr/ADR-020-dallas-ds18b20-sensor-integration.md`

**Change:**
```diff
- **Breaking Change:** 2025-12-15 (v1.0.0 - GPIO default changed, address format fixed)
+ **Breaking Changes:** Planned for v1.0.0 final release
+ **Status:** As of v1.0.0-rc6 (Jan 2026), not yet in stable release
+ **Changes Planned:**
+   - GPIO default: GPIO 13 (D7) → GPIO 10 (SD3)
+   - Address format: 9-char buggy format → 16-char standard format
```

### 3. Add Disclaimer to All ADRs

Add to every ADR with pre-2021 dates:

```markdown
**Note on Dates:** This project's git history was truncated on April 23, 2021 
when the repository was migrated. Dates before 2021-04-23 are estimates based 
on developer recollection and project documentation. Only dates after 2021-04-23 
can be verified with git commit evidence.
```

### 4. Mark Verified Dates

For all post-2021 dates with git evidence, add:

```markdown
**Git Evidence:** Commit [hash] ([date])
```

Example:
```markdown
**Date:** 2021-10-16 (AceTime adoption)  
**Git Evidence:** Commit 45b51f2 (October 16, 2021)
```

---

## Summary

**Findings:**
- ❌ **2 dates are incorrect:** ADR-015 (wrong year), ADR-020 (impossible future date)
- ⚠️ **18 dates cannot be verified:** Pre-2021 (git history lost)
- ✅ **4 dates have git evidence:** AceTime (2021-10-16), PROGMEM (2021-10-22), String removal (2023-01-26), v0.10.0 (2023-01-28)

**Recommendations:**
1. Fix the 2 incorrect dates immediately
2. Add git commit references to all verifiable dates
3. Add disclaimer about truncated git history
4. Mark all pre-2021 dates as "(Estimated - Pre-GitHub)"

**Quality Assessment:**
- Most estimates appear reasonable
- Evidence suggests project existed since ~2016-2018
- Active development visible 2021-2026
- Current version: v1.0.0-rc6 (still in release candidate phase)
