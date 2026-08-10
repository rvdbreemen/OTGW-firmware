**Home Assistant entities survive a Core restart, and three Remeha message IDs decode for the first time.**

v1.7.4 is a fix release for the 1.x (ESP8266) line. No breaking changes versus v1.7.2.

There is no v1.7.3 stable release: the 1.7.3 beta line is what ships here, with the version rolled forward. If you run `1.7.3-beta.4`, this is the same code plus the bump.

Full release notes: [RELEASE_NOTES_1.7.4.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.4.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## Bug fixes

- **Entities no longer stay on "unknown" after a Home Assistant Core restart.** Discovery configs are retained so Home Assistant rebuilds the entities, but state topics are not, and most values publish only when they change. A restarted Home Assistant was waiting for data the gateway had no reason to send. The `offline` to `online` transition on `homeassistant/status` now re-publishes every tracked value as first-seen, paced by OpenTherm bus traffic instead of emitted in one burst. Discovery itself is unchanged. (TASK-1058, ADR-088)
- **`hvac_mode` and `hvac_action` no longer latch a value Home Assistant never received.** Both publishers updated their cache regardless of whether the MQTT send succeeded, so a single dropped publish stranded the topic until the thermostat mode genuinely changed. (TASK-1058)
- **`hvac_mode` and `hvac_action` now republish at least every 5 minutes.** They were the only on-change gated topics without a heartbeat. On a bench gateway, `hvac_mode` published exactly once across a ten minute capture before the fix. (TASK-1060)
- **Remeha message IDs 131, 132 and 133 now decode.** The vendor messages (dF-/dU-codes, service message, connected SCU detection) were logged as `Unknown message` and produced no sensors at all, because the internal message-id list sat three positions below the message table. Remeha qSense and Tzerra owners gain three working entities. (TASK-1064)
- **Four ventilation and heat-recovery topics refresh again.** `vh_fault`, `vh_ventilation_mode`, `vh_bypass_status` and `vh_bypass_automatic_status` published once at startup and never on their one minute heartbeat, because the master-side status fan-out reset their shared timers microseconds before the slave-side fan-out read them. (TASK-1066)

## Improvements

- An OpenTherm decode-coverage simulation fixture with a golden-baseline regression gate, run in one command against a device. It is what independently surfaced the ventilation heartbeat problem. (TASK-1062, TASK-1063, TASK-1065)
- `SHA256SUMS` and the source bundle are attached to stable releases again. (TASK-936)

## Behaviour change

The "MQTT Home Assistant Reboot Detection" setting is deprecated and no longer shown in the web interface. Detecting a Home Assistant restart now always requires observing it go offline first, which is what that setting used to switch off. The setting is still read from and written to `settings.ini`, so existing configurations load unchanged.

## Upgrade notes

Flash **both** firmware and filesystem. Settings are preserved.

To verify the main fix, restart Home Assistant yourself (Developer Tools > YAML > Restart). The OTGW entities should show real values within a few seconds and none should sit on "unknown". Do not reboot the gateway to test it; that is what used to mask the problem.

## Thank you

Special shoutout to **Nico** (Discord) for reporting this one properly and then sticking with it. The capture that pinned the cause contained a real Home Assistant Core restart, which is exactly what turned a vague "my entities go unknown" into a findable bug. Nico also confirmed the fix on a real gateway before it shipped.

Thanks to everyone who contributed to this release through bug reports, testing, and feedback:
- **Nico** (Discord) reported the entities-unknown issue after a Home Assistant Core update, ran the capture script through a real restart, and confirmed the fix on the beta
- **geo83** (Discord) helped triage the report and correctly separated an ESP reboot from a gateway reset

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose and verify.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
