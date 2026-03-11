# ADR-049: String Class Prohibition in Protocol Paths

**Status:** Accepted
**Date:** 2026-03-01
**Supersedes:** Strengthens ADR-004 (Static Buffer Allocation)

## Context

ADR-004 established a preference for `char[]` over `String` in performance-critical code. In practice, several high-frequency code paths still used the Arduino `String` class, causing hidden heap allocations on every invocation:

- `executeCommand()` returned `String` and was called on every OpenTherm message (~4/sec)
- `initWatchDog()` returned `String` with the reset reason
- `getpicfwversion()` returned `String` (value was never used by callers)
- `queryOTGWgatewaymode()` built intermediate `String` objects for parsing

Each `String` return value triggers at least one `malloc`/`free` cycle. On the ESP8266 with ~40KB usable RAM, this leads to progressive heap fragmentation, especially under sustained OpenTherm traffic.

## Decision

**Prohibit the Arduino `String` class as a return type or local variable in any function that executes on the OpenTherm message path, the main loop, or timer callbacks.**

Specifically:
1. Functions in the OT message pipeline (`processOTGW`, `executeCommand`, `addOTWGcmdtoqueue`) must use `char[]` buffers with explicit size parameters
2. Initialization functions (`initWatchDog`, `getpicfwversion`) must write to caller-provided `char[]` buffers or use `void` return when the value is unused
3. Parsing functions (`queryOTGWgatewaymode`) must use stack-allocated `char[]` with `strchr`/`strncmp` instead of `String.indexOf()`/`String.substring()`

**Pattern — before:**
```cpp
String executeCommand(const String sCmd) {
  String result = "";
  // ... builds result via String concatenation
  return result;  // hidden malloc + copy
}
```

**Pattern — after:**
```cpp
void executeCommand(const char* sCmd, char* outBuf, size_t outSize) {
  if (outSize > 0) outBuf[0] = '\0';
  // ... writes directly to outBuf via strlcpy/snprintf_P
}
```

## Alternatives Considered

### Alternative 1: Allow String with move semantics
Return `String` by value and rely on RVO (Return Value Optimization).

**Why not chosen:** ESP8266 Arduino core's `String` implementation doesn't guarantee RVO. Even with move semantics, the internal buffer is still heap-allocated. Fragmentation risk remains.

### Alternative 2: Global shared String buffer
Use a single global `String` that is reused across calls.

**Why not chosen:** Creates hidden coupling between unrelated functions. A `char[]` buffer achieves the same reuse without the String overhead and makes the size bound explicit.

## Consequences

### Positive
- Eliminates ~8 `malloc`/`free` cycles per OT message (4 messages/sec = 32 alloc/free per second removed)
- Heap fragmentation reduced measurably over long uptimes
- Buffer sizes visible at compile time — no hidden allocations
- Consistent with ADR-004's static allocation philosophy

### Negative
- More verbose call sites (must pass buffer + size)
- Developer must ensure buffer is large enough (mitigated by `sizeof()` at call site)
- Callers that previously relied on String methods need refactoring

## Implementation

Refactored in P1 of the C++ refactoring plan:
- `executeCommand()` → `void executeCommand(const char*, char*, size_t)` in OTGW-Core.ino
- `initWatchDog()` → `void initWatchDog(char*, size_t)` in OTGW-Core.ino
- `getpicfwversion()` → `void getpicfwversion()` (return value was unused)
- `queryOTGWgatewaymode()` → internal `char[128]` with `strchr` parsing

## Related Decisions
- ADR-004: Static Buffer Allocation Strategy (foundational principle)
- ADR-009: PROGMEM String Literals (complementary RAM savings)
- ADR-016: OpenTherm Command Queue (affected data path)
