# ADR-153 — SAT BLE Adds Plaintext Xiaomi MiBeacon (0xFE95); Encrypted MiBeacon Deferred to a Gated Phase 2

- **Status**: Proposed
- **Date**: 2026-06-25
- **Tags**: ble, esp32, sat, sensors, xiaomi, mibeacon
- **Supersedes**: (none)
- **Superseded by**: (none)
- **Relates to**: ADR-151 (BLE passive-continuous scan — upheld), ADR-092 (NimBLE adoption + continuous passive scan), ADR-051 (settings architecture, for the deferred per-slot bindkey)

## Status

Accepted, 2026-07-04 (Proposed 2026-06-25; accepted by the maintainer Robert van den Breemen 2026-07-04 after verifying the plaintext Xiaomi MiBeacon 0xFE95 decoder is implemented in `SATble.ino`). Authored from TASK-930 (maintainer-approved plan, 2026-06-25).
Guideline-level per ADR-080: no automated CI gate — correct advertisement decoding
is only fully observable against a live sensor on hardware (the field-validation AC).
Acceptance is the maintainer's manual checkpoint; this ADR is not self-accepted.

## Context

The SAT BLE roster (`SATble.ino`) ingests temperature/humidity sensors from passive
BLE advertisements. Before TASK-930 it decoded exactly two advertisement formats,
tried in order inside the NimBLE `onResult` scan callback:

1. **ATC/pvvx** custom firmware — service-data UUID `0x181A`.
2. **BTHome v2** (unencrypted) — service-data UUID `0xFCD2`.

Any other format is silently dropped. A 2026-06-25 live validation on the OTGW32
confirmed this: three nearby `A4:C1:38` (Telink/ATC) sensors auto-populated the
roster, but **stock Xiaomi Mijia** sensors — which advertise the **MiBeacon**
protocol on service-data UUID `0xFE95` — were never admitted.

MiBeacon has two relevant properties (cross-verified against ESPHome `xiaomi_ble.cpp`,
`Bluetooth-Devices/xiaomi-ble`, and `custom-components/ble_monitor`):

- **Plaintext vs encrypted is per-frame**, signalled by **frame-control bit 3**, and
  is *independent* of the MiBeacon version. The common "v4/v5 are encrypted, v2/v3 are
  not" claim is wrong; version only selects *which* cipher when bit 3 is set.
- The plaintext payload is an object **TLV list** (`id u16 LE | len u8 | value`) with
  optional, bit-gated header fields (MAC, capability), so the payload offset must be
  computed from the frame-control bits before walking objects.

Encrypted MiBeacon (v4/v5) uses authenticated **AES-CCM** with a 16-byte per-device
**bindkey**, which the user must obtain out-of-band (Xiaomi cloud account, token
extractor, or the pvvx flasher). Decoding it on the device would require the `mbedtls`
AES-CCM dependency (bundled in ESP-IDF, not currently linked into this firmware) and
per-roster-slot secret storage in `settings`.

A pragmatic alternative exists for Telink-based models (LYWSD03MMC, MHO-C401, CGG1,
CGDK2, MJWSD05MMC): reflash them to pvvx firmware over the air, after which they
advertise the already-supported `0x181A` (ATC) format with no key. MiBeacon support
is therefore aimed at sensors kept on stock firmware and at non-reflashable models
(e.g. the round MJ_HT_V1).

## Decision

1. **MiBeacon is the third pluggable parser branch.** `onResult` tries ATC (`0x181A`)
   → BTHome (`0xFCD2`) → MiBeacon (`0xFE95`), each feeding the same
   `bleFindOrAllocSlot` roster path. This follows the established multi-format pattern;
   no new architecture, no new dependency, no settings change for Phase 1.

2. **Phase 1 decodes PLAINTEXT MiBeacon only.** `parseBLEMiBeaconFormat` reads the
   little-endian frame control, **skips the frame if the encrypted bit (0x08) is set**
   (mirroring the existing BTHome encrypted-skip), computes the payload offset from the
   mac-included / capability-included bits, and walks the object TLV list, dispatching
   `0x1004` temperature (s16/10), `0x1006` humidity (u16/10), `0x100A` battery (u8 %),
   and `0x100D` combined (s16 temp/10 + u16 hum/10). It returns a valid frame only when
   a temperature object decoded, matching the ATC/BTHome contract.

3. **Encrypted MiBeacon (AES-CCM + per-sensor bindkey) is DEFERRED to a gated Phase 2.**
   It is not implemented now because it introduces (a) the `mbedtls` AES-CCM crypto
   dependency and (b) per-roster-slot secret (bindkey) storage in `settings` plus a UI
   to enter each 32-hex key — a real secret-management and UX cost that is a separate
   maintainer decision. This ADR records the deferral; a follow-up ADR will cover Phase 2
   if it is taken (the new dependency + secret storage are genuinely architectural).
   The legacy v2/v3 unauthenticated-AES cipher is out of scope entirely (ESPHome does
   not implement it; modern encrypted Mijia are all v4/v5).

## Alternatives Considered

- **Reflash stock → pvvx (already supported).** Zero firmware change; recommended for
  Telink models. Rejected as the *only* answer because it does not help sensors kept on
  stock firmware or non-reflashable models (MJ_HT_V1), which is exactly what TASK-930
  targets. The two paths are complementary, not exclusive.
- **Implement encrypted MiBeacon now (single phase).** Rejected for Phase 1: it forces
  the crypto dependency and per-sensor bindkey UX on a feature whose cheap, no-key half
  already covers a meaningful set of devices. KISS / minimal-change-surface: ship the
  plaintext half first, gate the crypto half on demand.
- **A generic manufacturer-data sniffer.** Rejected: MiBeacon is a structured,
  documented service-data format; a targeted parser is simpler and safer than a
  heuristic guesser.

## Consequences

- **Positive.** Stock plaintext Mijia sensors (and unencrypted LYWSD03MMC) now
  auto-populate the roster like ATC/BTHome, with no key and no configuration. One
  bounded parser branch; negligible flash/RAM. No change to the settings schema.
- **Negative / costs.** Encrypted Mijia (most stock LYWSD03MMC at default settings) are
  still skipped until Phase 2. Per-advert field overwrite: a sensor that splits
  temperature and humidity across separate adverts updates one field per advert; the
  other reflects the most recent frame (documented caveat; the existing ATC/BTHome path
  has the same single-advert model).
- **Neutral.** The encrypted path is cleanly stubbed by the bit-3 skip, so Phase 2 slots
  in as a decrypt-then-reuse-the-same-TLV-walker addition without reworking Phase 1.

## Related Decisions

- **ADR-154 (Encrypted Xiaomi MiBeacon via mbedtls AES-CCM with per-roster-slot bindkey)**: the gated Phase 2 this ADR defers; ADR-154 adds the crypto dependency and per-slot bindkey storage and reuses this ADR's TLV walker.
- **ADR-085 (SAT Integration)**: the SAT subsystem whose BLE roster (`SATble.ino`) this third advertisement decoder feeds.

## References

- TASK-930 (this work; plan approved 2026-06-25).
- ESPHome `esphome/components/xiaomi_ble/xiaomi_ble.cpp` (decode + AES-CCM reference).
- `Bluetooth-Devices/xiaomi-ble` `src/xiaomi_ble/parser.py` (frame-control bits, TLV walk).
- `custom-components/ble_monitor` `ble_parser/xiaomi.py` (independent confirmation).
