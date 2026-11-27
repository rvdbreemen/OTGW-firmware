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
updated_date: '2026-11-27T08:35:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 FINDING (coordinator asked, otgw-docs, code at origin/dev 8e518557f). (a) No repo source says which OTGW32 GPIO is exposed on a terminal block, pad or header: boards.h:56-97 and docs/hardware/PINOUT.md section 2 list signals per GPIO only (OT master IN 21 / OUT 1, OT slave IN 6 / OUT 7, I2C 17/18, 1-Wire 4, step-up enable 10, bypass relay 47, W5500 SPI 11 to 16, LEDs 2/8/48, buttons 0/9); OTGW-firmware.h:1087 says the free header pins are not documented yet; the only terminal-block text (ch02:72) is about the Classic. (b) Which of the 10 terminal-block pins map to GPIOs is not documented (inference only, not verified: the OT lines go through the OT interface to the thermostat and boiler terminals). The hardware owner said the 10-way terminal block is what he understands by header and that S0 and output are not known to be supported on the OTGW32. (c) S0 and GPIO output are compiled into the OTGW32 build with no HAS_* gate, default OFF (S0types.h:21, Outputstypes.h:21 bEnabled false), defaults 12 and 16 (S0types.h:22, Outputstypes.h:22) equal W5500 SCK and RST; when enabled with those pins the firmware REFUSES them (s0PulseCount.ino:42, outputs_ext.ino:26, sensors_ext.ino:308, boardPinReserved OTGW-firmware.h:1058-1067), so Ethernet cannot be broken by the defaults or by a reserved pin; the remaining risk is a pin outside the reserved list that is wired to something else (GPIO 4 is not in the list). RECOMMENDATION (otgw-docs, decision with the coordinator and Robert): hide or disable S0 and GPIO output on the OTGW32 board type behind a HAS_* flag (settings, UI, MQTT) until the hardware owner documents free pins; the Dallas half stays: default pin 4 on the OTGW32. AC1 is therefore rewritten in practice: S0 and output: flag off on OTGW32 (or board-specific defaults once pins are known); Dallas: default 4.

2026-11-27 Connector picture from the hardware owner (docs and task use only, not reposted; file OTGW-field-data/_comms/tim_otgw32_terminals_1575789914078056508.png, read by BlueFlame and opened by otgw-docs): a cropped schematic of connector J4 with ten pin numbers and a red cross drawn over it; the labels on the right, top to bottom, are Boiler, Thermostat, 5 V, SW, GND, 3.3V, 1wire, GND. UNCERTAIN: eight labels for ten pins, the pin-to-label order is not readable, and the meaning of the red cross is unknown (a follow-up question was asked of the sender). So no pin numbers and no meaning of the cross go into the docs. Safe facts: the connector carries 5 V, 3.3 V, GND, a 1-wire line, an SW line and the boiler and thermostat lines; no spare general-purpose GPIO is visible on it apart from the 1-wire line. This supports the recommendation that S0 and the GPIO output are not available on this board (no documented free pin), and it supports Dallas on GPIO 4. 'Which pins are free' stays unknown in the docs.

2026-11-27 Answer of the hardware owner on the picture (#beta-testing 1575791889997635705, verbatim): 'That is the terminal block, pin 1 is nearest the reset button (on the right), the red cross means the connector block is not installed at the factory.' So J4 is the 10-way terminal block, pin 1 is nearest the reset button, and the block is not fitted at the factory (fitted by the user). The pin-to-label order is still not confirmed (eight labels for ten pins), so no pin numbers go into the docs; ch11 and h11 now say the block is not fitted at the factory and list what it carries.
<!-- SECTION:NOTES:END -->
