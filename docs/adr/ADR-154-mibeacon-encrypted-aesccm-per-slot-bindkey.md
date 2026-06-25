# ADR-154 — Encrypted Xiaomi MiBeacon (v4/v5) via mbedtls AES-CCM with Per-Roster-Slot Bindkey Secret Storage

- **Status**: Proposed
- **Date**: 2026-06-25
- **Tags**: ble, esp32, sat, sensors, xiaomi, mibeacon, crypto, security
- **Supersedes**: (none)
- **Superseded by**: (none)
- **Relates to**: ADR-153 (added plaintext MiBeacon, deferred this), ADR-051 (settings architecture), ADR-032 / ADR-056 (trusted-LAN security model + secret handling), ADR-092 / ADR-151 (NimBLE passive-continuous scan)

## Status

Proposed, 2026-06-25. Authored from TASK-930 Phase 2 (the gated half ADR-153 deferred,
unblocked by maintainer approval 2026-06-25). Guideline-level per ADR-080: no automated
CI gate on the runtime path (decoding a real encrypted sensor is hardware-observable),
BUT the decrypt recipe is pinned by a host known-answer test (`scripts/test_mibeacon_decrypt.py`)
that must pass. Acceptance is the maintainer's manual checkpoint; not self-accepted.

## Context

ADR-153 added plaintext MiBeacon (`0xFE95`) and explicitly deferred encrypted frames
"to a gated Phase 2 (new mbedtls dependency + per-sensor secret storage)". Most stock
LYWSD03MMC at default settings advertise **encrypted** MiBeacon, so without this they
stay invisible to the roster. Encryption is signalled per-frame by frame-control bit 3;
v4/v5 use authenticated **AES-128-CCM** with a 16-byte per-device **bindkey** (a.k.a.
beaconkey) that the user obtains out-of-band (Xiaomi cloud, token extractor, or the
pvvx flasher). A wrong key cannot forge a valid CCM tag, so it simply auth-fails.

Two new things are needed that the plaintext path did not require: a crypto dependency,
and somewhere to store the per-sensor secret. Both are architectural, hence this ADR.

## Decision

1. **Crypto dependency: mbedtls AES-CCM.** `SATble.ino` now `#include <mbedtls/ccm.h>`
   (bundled in ESP-IDF v5.x / Arduino-ESP32 3.3.x — newly *linked*, not a new package).
   `mibeaconDecryptInPlace()` is a faithful port of ESPHome's `decrypt_xiaomi_payload`:
   nonce(12) = on-air MAC(6) + raw[2..5] (product-id 2 + counter 1) + ext-counter(3 at
   `len-7`); AAD = `{0x11}`; 16-byte key; 4-byte tag; ciphertext geometry by total size
   (19 → @5 len 7; 22-24 → @11 len `size-18`). It decrypts in place, clears the
   encryption bit, and hands the plaintext object region to the Phase-1 TLV walker. The
   recipe is verified by a **known-answer test** against the xiaomi-ble
   `test_Xiaomi_LYWSD03MMC_encrypted` vector (`scripts/test_mibeacon_decrypt.py`, PASS:
   plaintext `061002d301` → humidity 46), since the bench has no encrypted sensor to
   field-test against.

2. **Per-roster-slot secret storage.** `settings.sat.sBleBindkey[SAT_BLE_MAX_ROSTER][33]`
   (32 lowercase hex + NUL). Serialized to `settings.ini` like any setting; persisted
   **plaintext** under the trusted-LAN model, exactly as `httppasswd` / `mqttpasswd`
   already are (ADR-032/056). The device has no secure element; the value must be usable
   verbatim as the AES key, so hashing/encrypting it is not an option.

3. **The bindkey is a secret on every read path.** Discovery JSON exposes only
   `has_key` (bool), never the key. The curated settings-GET dump does not emit it (like
   the password placeholder). REST debug (`postSettings`) and `updateSetting` log lines
   mask `satblebindkey*` the same way they mask the passwords.

4. **Provision MAC + key together.** `POST /api/v2/sat/ble/bindkey {mac,key}`
   (`satBLERosterSetBindkey`) **allocates a roster slot if the MAC is new**, because an
   encrypted sensor cannot self-announce into the roster before it has a key. The user
   pastes the MAC and the 32-hex key (both from the pvvx/cloud tooling) in one step; an
   empty key clears it; `forget` clears it.

## Alternatives considered

- **Require reflashing to pvvx (ADR-153's recommended path).** Still recommended for
  Telink models, but it does not cover stock-kept or non-reflashable sensors — which is
  this feature's reason to exist. Complementary, not a substitute.
- **Auto-roster unkeyed encrypted sensors and show a "needs key" state in the UI.**
  Rejected: adds a roster state + flow for sensors we cannot decode anyway.
  Provision-together is simpler (KISS) and matches how the user gets the data (MAC and
  key come from the same flasher screen).
- **Encrypt/hash the stored bindkey.** Rejected: it must be used as the raw AES key and
  there is no secure element; this only adds complexity over the existing
  plaintext-secret-in-flash model already accepted for passwords.
- **Support legacy v2/v3 (unauthenticated AES, 12-byte key).** Out of scope: ESPHome
  itself does not implement it and modern encrypted Mijia are all v4/v5 CCM.

## Consequences

- **Positive.** Stock encrypted Mijia decode and populate the roster once provisioned;
  the decrypt path reuses the Phase-1 walker; correctness is pinned by a repeatable
  known-answer test rather than left unverified.
- **Negative / costs.** A per-sensor secret must be sourced and entered by the user
  (irreducible: it is the sensor's key). `+264 B` settings + `settings.ini` growth. A
  secret now lives plaintext in flash (same risk class as the existing passwords; raw
  `settings.ini` file access via FSexplorer carries the same caveat it already does for
  passwords). Runtime field-validation needs a real encrypted sensor + its bindkey.
- **Known limitation (shared with Phase 1, not a regression).** A stock LYWSD03MMC
  emits **single-object** adverts (temperature, humidity, battery in separate frames),
  and the parser registers a frame only when it carries a **temperature** object
  (the ATC/BTHome contract, which keeps SAT's room-temperature input from ever
  flickering to 0). The decrypt + walker locate a humidity-only frame's value
  correctly (proven by `test_mibeacon_decrypt.py`: walker offset 11, humidity 46.7),
  but such a frame was initially not registered. **Resolved in TASK-931**: a per-field
  merge now registers a frame on ANY decoded field and updates only the fields present
  (sentinel = absent), so temperature never flickers to 0 and humidity/battery populate
  from their own adverts. The v2 BLE card also gained a per-slot bindkey input + an
  "add encrypted sensor" form (the key is write-only; discovery exposes only `has_key`).
- **Neutral.** mbedtls was already present in the toolchain; this is its first use in
  the firmware.

## References

- ADR-153 (plaintext MiBeacon; deferred this phase).
- TASK-930 Phase 2.
- ESPHome `esphome/components/xiaomi_ble/xiaomi_ble.cpp` (`decrypt_xiaomi_payload`).
- xiaomi-ble `tests/test_parser.py::test_Xiaomi_LYWSD03MMC_encrypted` (known-answer vector).
- `scripts/test_mibeacon_decrypt.py` (in-repo recipe verification).
