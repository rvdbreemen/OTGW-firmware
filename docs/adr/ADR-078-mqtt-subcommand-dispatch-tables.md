# ADR-078 MQTT Sub-command Dispatch Tables

## Status

Accepted (2026-04-19)

## Context

The MQTT message handler in `MQTTstuff.ino` routes sub-topic commands to SAT (and, in future, OTDirect and other subsystems) via chained `strcasecmp_P` blocks. As of 2026-04-19 the `set/<nodeId>/sat/<subCmd>` handler contains 76 `else if` branches over 140+ source lines:

```cpp
if (strcasecmp_P(satSubCmd, PSTR("target")) == 0) { satHandleTargetTemp(msgPayload); }
else if (strcasecmp_P(satSubCmd, PSTR("indoor_temp")) == 0) { satHandleExternalTemp(msgPayload); }
...
else if (strcasecmp_P(satSubCmd, PSTR("solar_freeze_integral")) == 0) { updateSetting("SATsolarfreezeint", msgPayload); }
```

This grew organically as SAT features landed and causes three concrete problems:

1. **Silent misses.** TASK-292 (`solar_freeze_integral` never reached dispatch) happened because a buffer was too small, not because a name was wrong, but the same buffer foot-gun can hide genuine missing entries. A table makes missing entries obvious.
2. **CLAUDE.md "typed control flow" rule violation.** The project rule is `enum class` or numeric IDs, never string tokens as discriminators. The chain is a 76-way string-token discriminator.
3. **Merge-conflict surface.** Every new SAT setting means editing the same 140-line block, which causes repeated conflicts during feature-branch work.

The REST API already solved the equivalent problem with `kV2Routes[]` (`restAPI.ino:1077`): a PROGMEM string + handler-pointer table with a sentinel terminator. The same pattern applies cleanly to MQTT sub-command handlers.

Alternatives considered:

1. **Keep the chained `strcasecmp_P` block.** Zero refactor cost but the three problems above compound with each new feature.
2. **Compile-time `switch` on a hashed enum.** Fast lookup but turns silent misses into compile-time requirements in a second location (the enum). Over-engineered for a branch count that fits on one screen.
3. **Hash-table at runtime.** Faster lookup at cost of RAM and complexity. Not worth it at this scale (< 100 entries).
4. **Dispatch table with `(command-string, handler-or-setting-key)` entries** (chosen). Mirrors `kV2Routes[]` precedent, single place to add/review entries, new additions are one-line changes.

## Decision

MQTT sub-command handlers use PROGMEM dispatch tables in the style of `kV2Routes[]`. The pattern applies to any `set/<nodeId>/<subsystem>/<subCmd>` dispatch that fans out to more than five handlers.

### Table entry shape

```cpp
typedef void (*SubCmdHandler)(const char* payload);

struct SubCmdEntry {
  const char*   cmd;          // PROGMEM command name, lower-case
  const char*   settingKey;   // PROGMEM setting key, or nullptr when a handler is used
  SubCmdHandler handler;      // nullptr when settingKey is used
};
```

Each entry either:
- delegates to a hand-written handler (`satHandleTargetTemp`, `satHandleControlMode`, ...) which is responsible for all payload parsing and validation, OR
- forwards the payload to `updateSetting(key, payload)` using the PROGMEM `settingKey`.

Entries that need sub-tokens beyond the sub-command (e.g. `sat/area/<idx>`, `sat/zone/<idx>/<cmd>`) stay outside the table as hand-written special cases *before* the table scan.

### Dispatch loop

```cpp
for (const SubCmdEntry* e = kSatMqttCmds; e->cmd != nullptr; e++) {
  if (strcasecmp_P(satSubCmd, e->cmd) == 0) {
    if (e->handler)          e->handler(msgPayload);
    else if (e->settingKey)  updateSetting_P(e->settingKey, msgPayload);
    return;
  }
}
// falls through to "unknown command" handling
```

`updateSetting_P(const char* keyProgmem, const char* value)` is a thin wrapper around `updateSetting` that accepts a PROGMEM key (copied into a small stack buffer before calling the existing RAM-key function). This avoids a `strcpy_P` boilerplate in every passthrough entry.

### Sentinel

Tables terminate with `{ nullptr, nullptr, nullptr }` so the dispatch loop works without a separate count constant. This matches `kV2Routes[]`.

### String literal storage

Command names and setting keys use `static const char kXxx[] PROGMEM = "...";` declarations grouped by subsystem above the table. This keeps PROGMEM discipline explicit (CLAUDE.md rule) and makes it trivial to grep for an exact token.

### Scope

This ADR applies to:
- `sat/<subCmd>` MQTT sub-command dispatch (first to adopt the pattern).
- Any future `otd/<subCmd>`, `ble/<subCmd>`, or similar per-subsystem dispatch that exceeds five entries.

It does NOT apply to:
- Top-level `set/<nodeId>/<subsystem>` dispatch, which is a short, stable list handled inline.
- Binary payload dispatch (OT message IDs), already table-driven via `mqttHaSensors[]` and OT-message-specific compose functions (ADR-077).

## Consequences

### Positive

- Adding a SAT setting is a single entry in `kSatMqttCmds[]`, not an edit inside a 140-line chain.
- `kSatMqttCmds[]` is greppable: `grep "kSet" MQTTstuff.ino` gives the full set of MQTT-exposed settings in one view.
- Reviewers can cross-check `kSatMqttCmds[]` against the settings struct to spot missing or orphaned entries, making the TASK-292 class of silent-miss bug far less likely.
- The REST and MQTT transports now follow the same dispatch-table shape, which reduces the per-contributor mental model.
- The chain is removed from the main message-handler function, so `MQTTstuff.ino`'s hot path becomes shorter and easier to reason about.

### Negative

- Table + strings cost ~900 bytes of flash for the full SAT table (~70 entries × 12 bytes struct + ~1 KB of strings). Not significant on ESP8266 (flash is not the constraint) but worth noting.
- Function-pointer indirection has a micro-cost per dispatch vs. an inline `if` chain. Unmeasurable in practice against MQTT round-trip latency.
- Initial refactor is bulky: 140 lines of chained `if` become ~90 lines of table entries plus a 10-line dispatch loop. Diff-heavy but reviewed once.
- `updateSetting_P` helper is a new (small) public surface in `settingStuff.ino`.

### Neutral

- Lookup is linear (O(n)) over 76 entries. Acceptable on ESP8266 because MQTT message rates are bounded and each dispatch already does I/O. Sorting the table alphabetically would enable binary search but is not justified at this size.

## Related

- **ADR-051** Dual encapsulating settings/state structs — provides the setting-key naming convention the dispatch table references.
- **ADR-077** Streaming MQTT HA discovery — orthogonal, different direction (outbound discovery publishes).
- **`kV2Routes[]`** in `restAPI.ino:1077` — the precedent pattern this ADR generalises.
- **CLAUDE.md** "Architecture rules" → "Typed control flow: `enum class` or numeric IDs, never string tokens as discriminators" — the rule this refactor restores compliance with.
- **TASK-292** (satSubCmd buffer enlarged for `solar_freeze_integral`) — the class of silent-miss bug that dispatch tables make harder to introduce.
- **TASK-307** — the review finding and tracking task that this ADR resolves.
