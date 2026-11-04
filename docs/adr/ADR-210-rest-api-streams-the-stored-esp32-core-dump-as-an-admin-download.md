---
id: "ADR-210"
title: "REST API streams the stored ESP32 core dump as an admin download"
status: "Proposed"
date: "2026-11-04"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "core dump"
  - "crash diagnostics"
  - "rest api"
  - "streaming"
  - "admin endpoint"
aliases:
  - "/api/v2/device/coredump"
  - "coredump partition download"
components:
  - "REST API v2"
  - "Platform abstraction"
symbols:
  - "sendDeviceCoreDump"
  - "platformCoreDumpSize"
  - "platformCoreDumpRead"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-210 REST API streams the stored ESP32 core dump as an admin download

## Status

Proposed, 2026-11-04.

**Decision Maker:** User: Robert van den Breemen (maintainer) chose "admin-protected" for this endpoint on 2026-11-04 (interactive question, TASK-1560).

Guideline-level per ADR-080: no automated gate checks the API shape; the host test `tests/test_device_coredump_api.py` checks the route wiring, the auth call, the shim use and that no partition-sized buffer exists. Review checks the rest.

## Status History

```yaml
status_history:
  - date: 2026-11-04
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal (TASK-1560)
    changed_via: adr-kit
```

## Context

An ESP32 panic or task-watchdog reset leaves an ELF core dump in the `coredump` partition (64 KB at 0x3F0000, `partitions_otgw_esp32.csv`). The reboot log keeps only the summary (task, PC, up to 16 backtrace frames, TASK-1538). The application caller can sit deeper than 16 frames (the ipc1 panic of alpha.566 entered through `_frxt_int_enter` inside `heap_caps_malloc`). The full dump decodes with `esp-coredump` against the matching `.elf`, but today it can only be read over USB (`esptool read_flash`). A field tester without a cable cannot send it.

The dump holds task stacks and heap fragments, so it can contain credentials that were in RAM (WiFi, MQTT, HTTP password). It is more sensitive than the crash log summary.

## Decision

1. **Route.** `GET /api/v2/device/coredump`, a new branch in `handleDevice()` (`restAPI.ino`). Other methods answer 405 through the existing `handleDevice()` check.
2. **Access.** `sendDeviceCoreDump()` calls `checkHttpAuth()` first: HTTP Basic Auth plus the same-origin (CSRF) check of ADR-056, exactly like `/api/v2/debug/flags`. Like every admin route it is open while no admin password is set (the ADR-056 default).
3. **Streaming.** The body is an `application/octet-stream` response with a known length and a filler callback (`beginResponse(contentType, len, filler)`). The filler reads the flash range the server asks for. No copy of the dump is held in RAM, and no 64 KB buffer is allocated, so the heap gates of ADR-165 and ADR-188 stay valid. The REST in-flight slot is held until the connection closes, like every other streamed response.
4. **Length.** The response is as long as the dump image the IDF reports (`esp_core_dump_image_get()`), not the whole partition.
5. **Format.** Headers `Content-Disposition: attachment; filename="otgw-coredump-<githash>.bin"` (the githash names the matching `.elf`) and `Cache-Control: no-store`. The bytes are the raw IDF image, decoded with `esp-coredump info_corefile --core-format raw` or `--core <file>` against the `.elf` of that githash.
6. **No dump.** `404` through `sendApiError()` when `esp_core_dump_image_check()` finds no valid dump.
7. **Platform shims.** All `esp_core_dump` and `esp_partition` calls live in `platformCoreDumpSize()` and `platformCoreDumpRead()` in `src/libraries/Platform/src/platform_esp32.h`. The partition is found by type and subtype, no offset is hard-coded. The application code has no raw platform symbol.
8. **Read-only.** The route never erases or marks the dump as read. The existing NVS fingerprint (TASK-1479) still decides whether the reboot log reports a dump.

## Decision Contract

### Must

- Call `checkHttpAuth()` before any dump byte or length is produced.
- Stream the dump from flash in the filler's chunks.
- Answer 404 when no valid dump is stored.
- Reach flash only through `platformCoreDump*()`.

### Must Not

- Allocate a buffer of the partition or image size.
- Erase, rewrite or mark the dump as read from this route.
- Add the dump to an unauthenticated route (`crashlog`, `info`).

### Exceptions

- None.

### Verification

- `python tests/test_device_coredump_api.py`.
- On a bench unit with a stored dump: `curl -u admin:<pw> -o dump.bin -D - http://<ip>/api/v2/device/coredump`, check `Content-Length` equals the file size, then `esp-coredump info_corefile --core dump.bin --core-format raw <matching>.elf`. Without a dump: HTTP 404.

## Alternatives Considered

- **Open route (no auth), like crashlog.** Rejected: the dump can hold secrets from RAM.
- **Read the whole partition into a buffer, or into a LittleFS file first.** Rejected: 64 KB does not fit the heap budget of the async server; a LittleFS copy wastes flash (768 KB filesystem) and wear.
- **Stream the whole 64 KB partition.** Rejected: most of it is 0xFF past the image; the IDF reports the real length.
- **Upload to a cloud service.** Parked (TASK-1473): diagnostics stay local first.
- **Keep USB-only (status quo).** Rejected: field testers do not have a cable at hand.

## Consequences

**Positive:**

- A tester downloads a decodable dump with one `curl` or browser request; the capture of a field panic no longer needs USB.
- No RAM cost beyond the response object; no new dependency; no persisted state.

**Negative:**

- One more admin route; a script that depends on the file name depends on the githash naming.
- With no admin password set, anyone on the LAN can download the dump (ADR-056 default).
- Flash reads run in the async_tcp task for the length of a chunk (a few hundred bytes to a few KB per call).
- Not verified on hardware at proposal time (see the task for bench evidence).

## Related Decisions

- **ADR-056 (Protected admin endpoint security):** auth and CSRF contract used here.
- **ADR-146 (No ArduinoJson):** not involved; the body is binary, the 404 is built by `sendApiError()`.
- **ADR-165 / ADR-188 (REST in-flight cap, connection gates):** the dump is served under the same gates.
- **ADR-080:** guideline-level, test-gated.

## References

- Tasks: TASK-1560, TASK-1538 (16-frame summary), TASK-1479 (core dump in the reboot log).
- Source: `src/OTGW-firmware/restAPI.ino` (`sendDeviceCoreDump`), `src/libraries/Platform/src/platform_esp32.h` (`platformCoreDumpSize`, `platformCoreDumpRead`).
- Test: `tests/test_device_coredump_api.py`.
- API docs: `docs/api/README.md`, `docs/api/openapi.yaml`.

## Enforcement

No declarative rule: the host test is the gate.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "forbid_call": [],
  "require_call": [],
  "allowed_in": []
}
```
