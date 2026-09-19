---
id: TASK-1116
title: Allow only one writer on the OTmonitor bridge port 25238
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 05:06'
updated_date: '2026-09-03 05:22'
labels: []
dependencies: []
ordinal: 273000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of TASK-1115 on otgw-1.x.x. OTGWstream is declared AsyncSimpleTelnet<2>, so two clients can hold the port at once. available() and read() serve the first slot with data and there is no per-client stream identity, so two simultaneous writers splice into one PIC command stream.\n\nOn this branch the defect is sharper than on 1.x because the reader is not single-threaded: the same audit found that a disconnect between available() and read() could hand the PIC a fabricated 0xFF (fixed separately as TASK-1114). Reducing to one slot removes the interleaving half of the problem.\n\nWith MAX_CLIENTS at 1 the reconnect-rotation branch in AsyncSimpleTelnet becomes live: a new connection from the same address silently evicts the previous session, so a crashed OTmonitor can reclaim the port, while a different address is refused at accept.\n\nNote the template argument also appears in the setTelnetNegotiation call, which must change with the declaration or the firmware will not compile.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGWstream is declared with a single client slot and every reference to its template argument is updated so the firmware compiles
- [x] #2 The NEG_OFF call still runs before the listener binds, so telnet IAC escaping stays off for the raw bridge
- [x] #3 A reconnect from the same address takes over the session; a second client from a different address is refused at accept
- [x] #4 build.bat is green for the default targets and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Changed AsyncSimpleTelnet<2> to <1> in OTGW-Core.h and updated the matching template argument in the setTelnetNegotiation call, which would otherwise not compile. Verified no <2> reference survives.

AC 2: NEG_OFF still runs immediately before OTGWstream.begin() in startPICStream(), so the listener never binds with telnet negotiation active. Neither line was moved by this change.

AC 3 is a property of the library, verified by reading the accept path rather than by staging two clients: with MAX_CLIENTS at 1 the reconnect-rotation branch fires when the incoming address matches the active slot, evicting it and re-attaching; otherwise the accept falls through to close() and delete on the new client.

Gates: build.bat green for esp32, esp32-classic and esp32-combo plus all three filesystem images, six SUCCESS lines, binaries fresh. evaluate.py --quick 68/76 pass, 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced the OTmonitor bridge on port 25238 to a single client on the 2.0.0 line.

Two slots let two simultaneous writers splice their bytes into one PIC command stream, because the library has no per-client stream identity. One slot removes that. The reconnect rotation becomes active as a side effect and is the behaviour we want: a reconnect from the same address takes the session over, a different address is refused at accept.

The template argument appears twice on this branch, in the declaration and in the setTelnetNegotiation call, so both moved together; NEG_OFF still runs before the listener binds.

The cost is that a passive second consumer is refused as well, since the library cannot distinguish a reader from a writer. That is recorded in the code comment which previously documented the two-slot choice.
<!-- SECTION:FINAL_SUMMARY:END -->
