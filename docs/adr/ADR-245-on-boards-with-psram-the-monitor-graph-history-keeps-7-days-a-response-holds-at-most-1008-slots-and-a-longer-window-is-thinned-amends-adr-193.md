---
id: "ADR-245"
title: "On boards with PSRAM the Monitor graph history keeps 7 days; a response holds at most 1008 slots and a longer window is thinned (amends ADR-193)"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-193"
  - "ADR-256"
topics:
  - "graph"
  - "psram"
  - "rest-api"
  - "web-ui"
aliases:
  - "7 day graph"
  - "graph history 7 days"
  - "hours parameter"
  - "GRAPH_HIST_MAX_EMIT"
components:
  - "Monitor graph device history (graphHistoryStuff)"
  - "GET /api/v2/graph/history ?hours= window"
symbols:
  - "GRAPH_HIST_LARGE_SLOTS"
  - "GRAPH_HIST_MAX_EMIT"
  - "GRAPH_HIST_DEFAULT_WINDOW_S"
  - "GRAPH_HIST_MAX_WINDOW_S"
  - "graphHistMakeSnapshot"
  - "graphTrim"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-245 On boards with PSRAM the Monitor graph history keeps 7 days; a response holds at most 1008 slots and a longer window is thinned (amends ADR-193)

## Status

Accepted, 2026-11-15.

Guideline-level per ADR-080: verified by host tests and web UI tests, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: Agent (92)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Proposed
    changed_by: Agent (92)
    reason: ADR-245 amends ADR-193 (TASK-1963)
    changed_via: adr-kit lifecycle
  - date: 2026-11-15
    status: Accepted
    changed_by: Robert van den Breemen (maintainer), recorded by Agent (92)
    reason: "Robert chose A, Discord DM 1571457707725168736 on question 1571457301280202873 ('Recommendation: A, and accept ADR-245 with it'); 7-day ring: DM 1571454228763779093"
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Accepted
    changed_by: Agent (ba)
    reason: Related to ADR-256 (TASK-2018)
    changed_via: adr-kit lifecycle
```

## Context

ADR-193 keeps a bounded 24 h history of the Monitor graph on the device: a ring of 14-byte slots, on a board with
PSRAM (pseudo-static random-access memory, the external memory of the S3 module) 726 slots of 120 s in one never-freed block, and on
other boards 126 slots of 30 s. The browser loads it once through the GET (read) request /api/v2/graph/history and merges it with the
values it collects live.

Robert asked for a longer view and chose option A on 2026-11-15 (Discord direct message 1571454228763779093, item 1,
answering "A) keep 7 days on PSRAM boards instead of 24 h (~70 KB PSRAM)"; option B, a flash copy every 15 minutes,
was not chosen). Keeping 7 days is cheap: 5046 slots of 14 B are 70644 B of PSRAM.

Sending 7 days is not cheap. The response goes out through restSendChunked() (jsonChunked.h): for every
TCP (Transmission Control Protocol) send window the firmware re-runs the whole JSON emitter, and RestChunkWindow counts
every byte up to the end of the body while it copies only the bytes of that window. The work per response therefore
grows with the square of the body size. The default 24 h body is 25355 bytes on a dense ring (measured by
tests/test_graph_history.cpp); 7 days at full 2 minute resolution would be about 7 times that, about 49 times the
formatting work on the async_tcp task that also serves every other web request (the cost TASK-1306 already tracks).

Robert was asked how to serve the longer window (Discord direct message 1571457301280202873) and chose option A
(Discord direct message 1571457707725168736): the ring keeps 7 days at 2 minutes, the default response stays 24 h at
full resolution, and the 7-day view is served thinned to 10 minute points. The same question asked to accept this
ADR with it.

## Decision

1. On a board with PSRAM the ring holds 5046 slots of 120 s (GRAPH_HIST_LARGE_SLOTS), 5040 of them readable: 7 days.
   It stays in RAM (random-access memory) only; a reboot still empties it, as under ADR-193. Boards without PSRAM keep their 126 slots of
   30 s unchanged.
2. GET /api/v2/graph/history without a parameter covers at most 24 h (GRAPH_HIST_DEFAULT_WINDOW_S), so its body and
   its cost stay what they were under ADR-193.
3. `?hours=N` (1 to 168) asks for a longer or shorter window, capped at 7 days (GRAPH_HIST_MAX_WINDOW_S) and at what
   the ring holds. A response holds at most 1008 slots (GRAPH_HIST_MAX_EMIT). A window of more ring slots is served
   as groups of `step` ring slots, aligned to the newest slot: `?hours=168` gives 1008 groups of 5 slots, 10 minutes
   each, and `interval_s` 600. In a group a temperature and the modulation take the newest value that is not empty;
   the flame, hot water and central heating flags are set when any slot of the group had them, so a short burn
   stays visible. With step 1 the body is byte for byte the body of ADR-193.
4. The Monitor graph gets a 7-day window chip. The browser asks for `?hours=168` only while a window longer than 24 h
   is chosen, and shows the chip only when the device answers with slots of 2 minutes or longer (a ring that holds
   days). It keeps device records for 7 days and its own live 5 s records for 24 h, trimmed by time instead of by
   count; a count limit of 24 h of live slots would cut the 7-day device records away after a day with the page
   open. The CSV export covers the window shown, at the resolution the graph has: 10 minutes for 7 days.

## Decision Contract

### Must

- Keep the default response (no `?hours=`) at most 24 h and at full ring resolution.
- Cap a response at GRAPH_HIST_MAX_EMIT slots and serve a longer window thinned, aligned to the newest slot.
- Keep the ring in RAM only and leave boards without PSRAM unchanged.

### Must Not

- Serve 7 days at full resolution in one response.
- Trim the browser's graph history by record count.

### Exceptions

- None.

### Verification

- `tests/test_graph_history.py` (runs `tests/test_graph_history.cpp` on the real `graphHistoryStuff.h`): the ring holds
  exactly 7 days, the default response is 720 slots of 120 s, `?hours=168` is 1008 groups of 5 slots with
  `interval_s` 600 and flags OR-ed over a group, a window above 7 days is capped, and a board without PSRAM serves
  120 slots of 30 s whatever the window. A mutant with the old 726-slot ring fails 7 checks.
- `tests/webui/graph-7day-window.test.mjs` (the real `graphTrim()`, `mergeGraphHistory()` and `loadGraphHistory()`
  of `data/v2.js`): the trim by time, a 7-day response surviving a day of live records, `?hours=168` only for the
  7-day window, and the chip hidden for the 30 s ring.

## Alternatives Considered

- **B: 7 days at full resolution in 7 pages of 24 h.** A full-resolution CSV for 7 days and a cost that grows only
  linearly, but the browser has to keep 7 requests consistent while the ring moves. Rejected by Robert for now
  (option B of 1571457301280202873); it stays possible behind the same `?hours=` contract.
- **C: one full 7-day response.** About 49 times the formatting work of today on async_tcp. Rejected.
- **A flash copy every 15 minutes, so the history survives a reboot.** Option B of the first question; not chosen.

## Consequences

**Positive:**

- A week of boiler behaviour on the device, for the Monitor graph and for the capture tool, at about 60 KB more
  PSRAM than ADR-193.
- The default request costs what it cost before; the 7-day request costs about 1.4 times that body.

**Negative:**

- The 7-day view and its CSV are at 10 minute resolution; a 2 minute view needs a window of 24 h or less. The graph
  draws about 740 pixel columns, so the 7-day line loses nothing visible.
- The browser keeps up to about 18000 records (24 h of live slots plus the week of device records), about the same
  as before.

## Open Questions

- [x] How should the 7-day window be served? Answered by Robert (Discord direct message 1571457707725168736): option A,
  thinned to 10 minutes, default unchanged.

## Related Decisions

- **ADR-193 (Keep a bounded 24 h Monitor graph history on the device)**: this ADR amends its PSRAM ring size (726 to
  5046 slots) and adds the `?hours=` window; the slot layout, the small ring, the RAM-only rule and the JSON shape stay.

## References

- TASK-1963; TASK-1306 (response cost on async_tcp); TASK-1776 (the live domestic hot water (DHW) temperature series in the same graph).
- `src/OTGW-firmware/graphHistoryStuff.h:45` (GRAPH_HIST_LARGE_SLOTS and the window constants), `graphHistoryStuff.h:206`
  (graphHistMakeSnapshot), `src/OTGW-firmware/jsonChunked.h:115` (restSendChunked) and `jsonChunked.h:78` (RestChunkWindow);
  `src/OTGW-firmware/graphHistoryStuff.ino` (sendGraphHistory, `?hours=`), `src/OTGW-firmware/jsonChunked.h`
  (restSendChunked, RestChunkWindow), `src/OTGW-firmware/data/v2.js` (graphTrim, loadGraphHistory).
- Robert's decisions: Discord direct messages 1571454228763779093 and 1571457707725168736; questions
  1571452589231439943 and 1571457301280202873.
- ESP32-S3 external memory: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html
