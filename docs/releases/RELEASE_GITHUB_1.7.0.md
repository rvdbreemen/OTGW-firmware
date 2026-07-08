v1.7.0 crash-proofs the heap-fragmentation reboot on long-running ESP8266 devices and reclaims about 6.6 KB of static RAM, restoring roughly the free-memory headroom the firmware had at 1.3.5.

## Highlights

- **Heap-fragmentation crash-proofing.** Random reboots under sustained load are fixed at the source: MQTT, WebSocket, and HTTP serving now gate on the largest contiguous heap block (not just total free) and back off gracefully, the ESP8266 core's `streamFile` NULL-write fault is patched at build time (ADR-084), and the main loop is back on its field-proven `delay(1)` pacing.
- **RAM / heap-headroom optimization pass.** The OpenTherm message-name table moved into flash (PROGMEM) plus about 25 verified buffer and integer-width trims: roughly 6.6 KB of static RAM reclaimed, the largest-contiguous-block floor under load up from about 4.9 KB to about 11 KB. No feature behaviour changed.
- **Active gateway overrides over MQTT and Web UI** (ADR-082): override state set on the PIC is now visible to Home Assistant and the dashboard.
- **WiFi signal quality** (percentage and label) in the telnet welcome banner.
- New diagnostic counters `mqtt_fragskips`, `ws_fragskips`, `http_fragskips` on telnet and in MQTT stats.

## Bug fixes

- MQTT broker ports above 32767 can now be saved (field was `int16_t`, now `uint16_t`).
- S0 pulses-per-kWh above 65535 no longer wrap (validator now matches the field range).
- Debug Information tab renders friendly labels for the runtime WiFi rows.

## Upgrade notes

Standard upgrade, no settings migration needed. No breaking changes versus v1.6.1. Full notes: [RELEASE_NOTES_1.7.0.md](RELEASE_NOTES_1.7.0.md). Breaking-changes log: [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md).

## Thank you

This release was driven by community field testing. Special thanks to **GeorgeZ83**, whose multi-day diagnostic captures made the heap-fragmentation bisect possible and validated the optimization pass.

Thanks also to **Richard_HA**, **crashevans**, **Ties**, **Remco**, **Mattie**, and **Andre Brait** (static IP feature request, #561) for testing and feedback.

Join the community: https://discord.gg/zjW3ju7vGQ
