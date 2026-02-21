---
# METADATA
Document Title: Issue #143 MQTT Source Separation - Full Options Analysis
Review Date: 2026-02-20 06:54:00 UTC
Branch Reviewed: copilot/plan-scenarios-for-issue-143 → main (N/A)
Target Version: v1.x
Reviewer: GitHub Copilot Advanced Agent
Document Type: Options Analysis
PR Branch: copilot/plan-scenarios-for-issue-143
Commit: pending
Status: COMPLETE
---

# Issue #143: Full Analysis of Options

## Problem Summary

Issue #143 reports that a single MQTT topic can represent multiple real OpenTherm sources (thermostat vs boiler, and sometimes gateway override path), which can produce conflicting values for the same semantic metric (example: setpoint-related IDs).

The design goal from feedback is:

1. Keep **legacy API/topic compatibility**.
2. Minimize **memory impact** and avoid extra dynamic buffers.
3. Recommend a path to proceed.

## Relevant Architectural Constraints

- **ADR-004 (Static Buffer Allocation)**: avoid new dynamic allocation and avoid large transient buffers.
- **ADR-006 (MQTT Integration Pattern)**: preserve topic compatibility patterns and Home Assistant behavior.
- **ADR-038 (OpenTherm Pipeline)**: source is already available via `OTdata.rsptype` in current processing flow.

## Current Technical Baseline

- OpenTherm source is already classified as:
  - `T` -> `OTGW_THERMOSTAT`
  - `B` -> `OTGW_BOILER`
  - `R` -> `OTGW_REQUEST_BOILER`
  - `A` -> `OTGW_ANSWER_THERMOSTAT`
- Value publishing is centralized in `print_f88()`, `print_u16()`, `print_s16()`, `print_s8s8()`.
- Existing skip logic already suppresses overridden stale values in specific R/A timing cases.

This means source-specific publication can be added with small, localized edits and no parser redesign.

## Option Analysis

### Option A — Dual Publish (legacy topic + source-suffixed topic)

**Description**
- Continue publishing legacy topic unchanged.
- Also publish source-specific topic (`_thermostat`, `_boiler`, `_gateway`).

**Legacy support**
- Full backward compatibility (no migration required).

**Memory impact**
- Low if implemented with fixed local topic buffer reuse.
- No new setting strings required.
- No dynamic allocation required.

**Operational impact**
- Higher MQTT message count (duplicate publishes).
- Increased broker traffic and HA entity count if discovery is enabled for source topics.

**Risk**
- Moderate operational churn, low implementation risk.

---

### Option B — Configurable separation switch (recommended)

**Description**
- Add boolean setting (e.g. `mqttseparatesources`).
- Default to compatibility-first mode (`false`), optionally enable source topics.
- When enabled: publish legacy + source topics (or source-only in future migration stage).

**Legacy support**
- Strong. Existing users unchanged by default.
- Power users can opt in.

**Memory impact**
- Very low:
  - one global `bool` setting
  - reuse existing global settings buffers (`settingMQTTtopTopic`, `settingMQTTuniqueid`, namespace buffers)
  - fixed local topic buffer only
- No new large buffers.

**Operational impact**
- Controlled topic growth only for opt-in users.
- Easier rollout and support.

**Risk**
- Low implementation risk, low migration risk.

---

### Option C — Source-only topics (breaking)

**Description**
- Stop publishing legacy unsuffixed topics.
- Publish only source-qualified topics.

**Legacy support**
- None (breaking).

**Memory impact**
- Low implementation memory footprint, but migration complexity is high.

**Operational impact**
- Requires migration for all integrations and automations.

**Risk**
- High compatibility risk.

---

### Option D — Selective separation by message ID

**Description**
- Split only conflict-prone IDs (e.g. setpoint/control family), keep others legacy-only.

**Legacy support**
- Partial compatibility with less churn than full split.

**Memory impact**
- Low in buffers.
- Slightly higher code complexity due to ID allow-list logic.

**Operational impact**
- Lower traffic than A/B.
- Harder mental model for users ("some topics split, others not").

**Risk**
- Medium complexity/maintainability risk.

## Comparative Summary

| Option | Legacy Support | Memory Impact | Complexity | Migration Risk | Recommendation Fit |
|---|---|---|---|---|---|
| A | Excellent | Low | Low | Low | Good |
| B | Excellent (default-off) | **Lowest practical + controlled** | Low-Medium | Low | **Best** |
| C | Poor | Low | Low | **High** | Not advised |
| D | Medium | Low | Medium | Medium | Situational |

## Recommendation to Proceed

Proceed with **Option B (configurable source separation switch)**.

### Why Option B is best for this PR feedback

1. **Keeps legacy API/topic behavior by default**, satisfying compatibility requirement.
2. **Minimizes memory impact**:
   - no new dynamic buffers
   - no topic String construction
   - only one new boolean setting
   - reuse existing global setting buffers and existing publish flow.
3. Enables phased adoption and easy support in production.

## Implementation Guardrails (Memory + Compatibility)

1. Reuse existing topic-generation path and global MQTT namespace buffers.
2. Add one helper that appends source suffix into a fixed-size stack buffer with bounds checks.
3. Keep legacy publish call unchanged in first phase.
4. Gate source-publish by setting and `is_value_valid()`.
5. Keep R/A/B/T mapping static via `switch(rsptype)` + PROGMEM literals.

## Suggested Rollout

1. Phase 1: Option B with default `false` (legacy behavior).
2. Phase 2: allow HA discovery for source topics only when enabled.
3. Phase 3 (optional future): evaluate default-on after migration evidence.

