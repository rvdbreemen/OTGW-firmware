---
# METADATA
Document Title: OpenTherm v4.2 Compliance Review - Archive Overview
Review Date: 2026-02-15 14:33:00 UTC
Branch Reviewed: main (current codebase)
Target Version: v1.1.0-beta
Reviewer: GitHub Copilot Advanced Agent
Status: COMPLETE
---

# OpenTherm v4.2 Protocol Compliance Review

## Overview

This review compares the OTGW-firmware implementation against the OpenTherm Protocol Specification v4.2 (10 November 2020) to identify missing, incorrectly handled, or improvable message handling.

## Summary of Findings

| Category | Count | Severity |
|----------|-------|----------|
| Missing Message IDs (39, 93-97) | 6 | Medium |
| R/W Direction Mismatches | 10 | Medium-High |
| Data Type Inconsistencies | 2 | Medium |
| Critical Bug (ID 20 hour bitmask) | 1 | **Critical** |
| Label/Unit Issues | 4 | Low |
| Code Quality Issues | 3 | Low |

## Documents

| Document | Description | Audience |
|----------|-------------|----------|
| [OPENTHERM_V42_COMPLIANCE_PLAN.md](OPENTHERM_V42_COMPLIANCE_PLAN.md) | Complete analysis with detailed task breakdown | Developers |

## Key Actions Required

### Urgent (Sprint 1)
1. **Fix ID 20 hour bitmask bug** — `0x0F` → `0x1F` at OTGW-Core.ino:1297
2. **Correct R/W directions** for 10 IDs in OTmap (OTGW-Core.h)

### Important (Sprint 2) 
3. **Add missing IDs** 39, 93-95, 96-97
4. **Fix FanSpeed** print function inconsistency (ID 35)

### Nice-to-have (Sprint 3)
5. **Fix unit labels** (FanSpeed rpm→Hz, DHWFlowRate l/m→l/min)
6. **Fix MQTT typos** (breaking changes - needs migration strategy)
7. **Code quality** improvements

## Reference Specification

- `Specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`
- `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
