# MQTT Source-Specific Topics (v1.2.0)

## Summary

This release adds source-specific MQTT topics for OpenTherm values while keeping the existing topic model intact.

When enabled, the firmware publishes:

- Legacy topic: `<topTopic>/value/<node-id>/<sensor>`
- Source topic(s):
  - `<topTopic>/value/<node-id>/<sensor>_thermostat`
  - `<topTopic>/value/<node-id>/<sensor>_boiler`
  - `<topTopic>/value/<node-id>/<sensor>_gateway`

Setting:

- Web UI/API key: `mqttseparatesources` / `MQTTSeparateSources`
- Default: `true`

## Why This Was Added

A single topic previously mixed thermostat requests and boiler responses. With source separation, diagnostics and automation can distinguish requested values from provided values.

Example:

- Thermostat request: `TSet_thermostat = 20.5`
- Boiler response: `TSet_boiler = 20.0`
- Legacy topic still exists: `TSet = 20.0` (last observed value)

## Backward Compatibility

- Existing MQTT topic names are unchanged and still published.
- Existing Home Assistant entities based on legacy topics keep working.
- Feature is additive; old consumers do not need immediate changes.

## Home Assistant Discovery Behavior

With source topics enabled, additional HA discovery entities are created for source-specific sensors.

Notes:

- This increases entity count in HA.
- If you disable `MQTTSeparateSources`, source topic publishing stops, but previously discovered HA entities may remain until manually removed or cleaned up from retained discovery topics.

## Release Rollout Checklist

1. Flash firmware and filesystem as usual for `v1.2.0`.
2. Confirm `MQTTSeparateSources=true` in settings (default state).
3. Verify both legacy and source topics are present.
4. Verify source-specific HA entities are discovered.
5. Run the validation matrix in:
   - `docs/testing/ADR-040-source-separation-test-matrix.md`

## Validation Commands

Subscribe and inspect traffic:

```bash
mosquitto_sub -h <broker> -v -t '<topTopic>/#'
```

Minimum checks:

- Legacy topic still updates.
- `_thermostat`, `_boiler`, `_gateway` topics appear when applicable.
- No source topics are emitted for parity-error messages.

## Rollback / Mitigation

If deployment shows broker or HA load concerns:

1. Set `MQTTSeparateSources=false`.
2. Reboot or let MQTT reconnect cycle complete.
3. Keep legacy topics only.

This rollback keeps all prior integrations functional because legacy topics are unchanged.

## Related Docs

- ADR: `docs/adr/ADR-040-mqtt-source-specific-topics.md`
- Test matrix: `docs/testing/ADR-040-source-separation-test-matrix.md`
