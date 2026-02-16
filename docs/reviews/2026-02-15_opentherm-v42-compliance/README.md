---
# METADATA
Document Title: OpenTherm v4.2 Compliance Review - Archive Overview
Review Date: 2026-02-15 14:33:00 UTC
Implementation Date: 2026-02-15 15:47:00 UTC
Branch Reviewed: main (current codebase)
Target Version: v1.1.0-beta
Reviewer: GitHub Copilot Advanced Agent
Status: IMPLEMENTED
---

# OpenTherm v4.2 Protocol Compliance Review

## Overview

This review compares the OTGW-firmware implementation against the OpenTherm Protocol Specification v4.2 (10 November 2020) to identify missing, incorrectly handled, or improvable message handling.

**Status: All priority items implemented.** See implementation summary below.

## Summary of Findings & Implementation

| Category | Count | Severity | Status |
|----------|-------|----------|--------|
| Critical Bug (ID 20 hour bitmask) | 1 | **Critical** | ✅ Fixed |
| R/W Direction Mismatches | 10 | Medium-High | ✅ Fixed |
| Missing Message IDs (39, 93-97) | 6 | Medium | ✅ Added |
| Data Type Inconsistencies (FanSpeed) | 1 | Medium | ✅ Fixed |
| Label/Unit Issues (non-breaking) | 2 | Low | ✅ Fixed |
| Label/Unit Issues (breaking MQTT) | 2 | Low | ⏭️ Skipped |
| Code Quality (is_value_valid) | 1 | Low | ✅ Fixed |
| Code Quality (unused field, bounds) | 2 | Low | ⏭️ Skipped |

## Implementation Commits

| Commit | Phase | Description |
|--------|-------|-------------|
| 893c73e | 1+2 | Fix DayTime hour bitmask + R/W direction mismatches (10 IDs) |
| 14e865d | 3 | Add 6 missing message IDs (39, 93-97) |
| 53973e9 | 4+5+6 | Fix FanSpeed datatype, units, is_value_valid consistency |

## Documents

| Document | Description | Audience |
|----------|-------------|----------|
| [OPENTHERM_V42_COMPLIANCE_PLAN.md](OPENTHERM_V42_COMPLIANCE_PLAN.md) | Complete analysis with detailed task breakdown | Developers |

## Items Intentionally Not Changed

- **MQTT topic typos** (`eletric_production`, `solar_storage_slave_fault_incidator`): Breaking change for existing Home Assistant automations — requires migration strategy
- **Unused struct field** `RoomRemoteOverrideFunction`: May be referenced by external tools — needs broader impact analysis
- **OTmap array bounds check**: Already identified in prior codebase review — tracked separately

## Reference Specification

- `Specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`
- `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
