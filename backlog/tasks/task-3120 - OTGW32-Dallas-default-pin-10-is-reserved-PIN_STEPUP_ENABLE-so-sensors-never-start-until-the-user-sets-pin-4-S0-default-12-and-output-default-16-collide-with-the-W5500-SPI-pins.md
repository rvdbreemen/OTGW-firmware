---
id: TASK-3120
title: >-
  OTGW32: Dallas default pin 10 is reserved (PIN_STEPUP_ENABLE) so sensors never
  start until the user sets pin 4; S0 default 12 and output default 16 collide
  with the W5500 SPI pins
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T08:20:00Z'
labels:
  - hardware
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1175000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-docs while writing the OTGW32 documentation (TASK-3115), coordinator 2026-11-27. An OTGW32 owner confirmed (#beta-testing 1575787624054919179): W5500 Ethernet uses SPI on GPIO 11,12,13,14,15 and 1-Wire is connected to GPIO 4. Firmware at origin/dev: 1-Wire is PIN_1WIRE 4 (src/libraries/Platform/src/boards.h:82) but no source file reads it; the Dallas pin is settings.sensors.iPin with default 10 (Sensorstypes.h:23), and 10 is PIN_STEPUP_ENABLE (boards.h:79), a reserved board pin, so sensors_ext.ino:308-311 refuses it and starts no sensors until the user sets the pin; GPIO 4 is not reserved (OTGW-firmware.h:1058-1067). The S0 counter default 12 (S0types.h:22) equals PIN_SPI_SCK 12 and the GPIO output default 16 (Outputstypes.h:22) equals PIN_SPI_RST 16 (boards.h:90-91): both collide with the W5500. A comment above LEGACY_S0_PIN in OTGW-firmware.h says the free OTGW32 header pins are not documented yet. Owner: otgw-dev. Open question to the owner of the hardware: which header pins are free for the S0 counter and the GPIO output on the OTGW32.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Board-specific defaults on the OTGW32 board class: Dallas default 4 (per the owner); S0 and output defaults that do not collide with the W5500 (PARTLY BLOCKED: needs the list of free header pins from the hardware owner)
- [ ] #2 Host test on the defaults (boards.h, Sensorstypes.h, S0types.h, Outputstypes.h): red on the old defaults, green on the new; no default may equal a reserved board pin
- [ ] #3 Migration: a stored pin in settings.ini is never overwritten; only an install that never saved the setting gets the new default
- [ ] #4 docs ch11 section 11.6 (en and nl) and docs/hardware/PINOUT.md updated to the new defaults
<!-- AC:END -->
