---
id: TASK-1115
title: Allow only one writer on the OTmonitor bridge port 25238
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 05:05'
updated_date: '2026-09-03 05:12'
labels: []
dependencies: []
ordinal: 206000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGWstream is declared SimpleTelnet<2>, so two clients can hold the port at once. available() and read() serve the first slot that has data, and there is no per-client stream identity anywhere: read() returns a bare int. Two clients writing at the same time therefore splice into one PIC command stream, for example client A's half-line PR=A concatenating with client B's TT=20.5 into a single malformed command. Within one client order is preserved; between clients it is not.\n\nFound by an adversarial audit of byte-exactness on port 25238. The audit deliberately left the choice open because it is a field question rather than a code question: fixing it in the library means adding a per-slot read API and changing every caller, while declaring the instance single-client is one template argument. The maintainer decided on one writer.\n\nWith MAX_CLIENTS at 1 the library's reconnect rotation becomes active: a new connection from the SAME address silently evicts the previous session and takes it over, so a crashed OTmonitor can always reclaim the port, while a connection from a different address is refused at accept. That is the intended single-writer semantic and it is strictly better than silently interleaving two writers into the PIC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGWstream is declared with a single client slot
- [x] #2 A reconnect from the same address takes over the session rather than being refused, so a client that crashed can reclaim the port
- [x] #3 A second client from a different address is refused at accept instead of having its bytes interleaved into the PIC stream
- [x] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Changed SimpleTelnet<2> to SimpleTelnet<1> in OTGW-Core.h. AC 2 and 3 are properties of the library, verified by reading it rather than by staging two clients: the accept path takes the reconnect-rotation branch when MAX_CLIENTS is 1 and the incoming address matches the active slot, evicting it silently and re-attaching; otherwise it falls through to close() and delete on the new client. So same address takes over, different address is refused.

Gates: build.bat green, fresh build/OTGW-firmware-1.7.5-beta.7+700247e.ino.bin and .littlefs.bin, "Build completed successfully". evaluate.py --quick 35/37 pass, 0 failed.

Recorded trade-off: the slot was <2> for "HA + one debug consumer" and the library cannot tell a reader from a writer, so a passive second consumer is refused too. The maintainer chose this over a per-slot read API.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced the OTmonitor bridge on port 25238 to a single client.

The library has no per-client stream identity: read() returns a bare int and available()/read() serve whichever slot holds data. With two slots, two simultaneous writers spliced their bytes into one PIC command stream, so one client's partial line could concatenate with another's into a malformed command. One slot removes that failure mode outright.

The reconnect rotation becomes active as a side effect and is the behaviour we want: a reconnect from the same address takes the session over, so a crashed client can reclaim the port, while a different address is refused at accept.

The cost is stated in the code comment that replaces the old one: the second slot existed for "HA + one debug consumer", and a passive consumer is now refused as well because the library cannot distinguish a reader from a writer. One writer with several readers would require a per-slot read API and a change to every caller.

No prerelease bump: beta.7 is published and firmware changes accumulate under the last tag between releases. The next /beta-prerelease rolls this into beta.8.
<!-- SECTION:FINAL_SUMMARY:END -->
