# RAM/HEAP audit — OTGWState (state-struct area) — wt-otgw-1.x.x (ESP8266)

All structs are in `src/OTGW-firmware/OTGW-firmware.h`. OTGWState is a single static
global (`OTGWState state;`), so every byte trimmed off a sub-struct is static-RAM saved 1:1.

## Findings (best-first by RAM saved x safety)

### 1. FlashSection.sError[129] -> [48]  (HEADLINE, -80 B static RAM)
- file: OTGW-firmware.h:271
- Every write is a fixed PROGMEM literal via snprintf_P (OTGW-Core.ino:4862-4874), capped by
  sizeof(field). Longest literal = "Hex file does not contain expected data" = 39 chars (+null=40).
- 129 is dead over-provision. [48] leaves 8 B margin; snprintf_P truncates safely regardless.
- Alignment: FlashSection has int iPICprogress (align 4). Current sizeof = 200
  (bool,bool,sError[129],sPICfile[65] = 196; int at offset 196; total 200).
  With sError[48]: 2+48+65 = 115; int at 116; sizeof = 120. Net -80, stays mult-of-4, rolls 1:1 into OTGWState.
- escape buffers errorEsc[129]/filenameEsc[129] (OTGW-Core.ino) are stack locals; oversized stack local is harmless, no change needed.
- impact: none. risk: low.

### 2. PicSettingsSection over-provisioned char[] (-16..-44 B, low-risk subset = -16)
- file: OTGW-firmware.h:348-366. All char[], align 1, strlcpy truncates (worst case cosmetic).
- LOW RISK (well-defined formats): sSetpointOverride[16]->[8] ("T20.5"/"C20.5"/"N", <=7), sSetback[16]->[8] ("15.0"). = -16 B.
- DO NOT shrink without confirming PIC return width: sSmartPower[16] (comment ambiguous "L" vs "Low power"),
  sBuilddate[24] ("17:52 12-03-2023"=16 but margin wanted). Truncation = cosmetic feature-change.
- impact: none (for the [8] subset). risk: low.

### 3. DiscoverySection u32->u16 on two per-run counters (-4 B)
- file: OTGW-firmware.h:309-310. iVerifyRunCount + iRepublishTriggeredCount are verify-cycle counters
  (verify ~daily). Transient (reset on reboot). u16 ceiling 65535 unreachable.
- iLastVerifyEpoch (unix epoch) and iPublishedTopicCount (per-topic, hundreds/cycle) STAY u32.
- Layout (declared order): epoch u32@0, runCount u16@4, repubCount u16@6, publishedTopic u32@8 (aligned),
  missing u16@12, orphan u16@14, outcome u8@16 -> pad to 20. sizeof 24 -> 20. -4. No reorder needed.
- impact: none. risk: low.

### 4. FlashSection.iPICprogress int -> int8_t (-4 B incremental, ONLY after #1)
- file: OTGW-firmware.h:273. Range -1..100. After sError shrink (#1) it is the LAST 4-byte member;
  making it int8_t drops FlashSection to align-1: sizeof 120 -> 116.
- sendJsonMapEntry has no int8_t overload (jsonStuff.ino) but int8_t promotes to int -> int32_t overload, renders as number. Safe.
- impact: none. risk: low-med (depends on promotion; verify build). Small win -> rank low.

## Alignment FALSE-POSITIVES (report as "no saving", do not propose)
- MQTTRuntimeSection: iLastConnectedMs is millis() timestamp (MQTTstuff.ino:867,937) -> must stay u32 ->
  struct pinned align-4 sizeof 8. Packing bConnected saves NOTHING.
- DebugSection: iOTGWSimulationIntervalMs/NextDueMs are millis timestamps compared via (int32_t)(millis()-due)
  (OTGW-Core.ino:3663,3723) -> must stay u32 -> struct align-4. Packing the 8 bools saves NOTHING.
- UptimeSection: iSeconds u32 (1Hz, overflows u16 at ~18h; used /day,/hour and >3600 nightly-restart) MUST stay u32;
  iRebootCount persisted lifetime counter MUST stay u32. No candidate.

## Verify-then-decide (medium risk, NOT recommended on defaults alone)
- PICSection sFwversion/sDeviceid/sType char[32]x3 (OTGW-firmware.h:249-251). Filled from vendored OTGWSerial
  library (firmwareVersion/processorToString/firmwareToString). Comment at OTGW-Core.ino:5152 documents
  "sDeviceid (max 32)" as a budget contract; deviceid is concatenated into download URLs/paths. Real values
  short ("6.6","PIC16F1847","gateway") but shrinking is a feature-change risk if library returns longer. medium.

## OUT OF AREA (cross-reference only)
- OTcurrentSystemState (OTGW-Core, has floats) and DallasrealDevice[16] (global, OTGW-firmware.h:546) are
  juicy but belong to OT-core/sensors agents.

## Net safe total (this area): -80 (#1) -16 (#2 subset) -4 (#3) = -100 B static RAM, all risk:low, impact:none.
  +4 more with #4 (low-med).
