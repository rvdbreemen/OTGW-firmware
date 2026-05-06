# ADR-067 — HA Discovery State Reconciliation on OTA Upgrade

## Status

Deprecated, 2026-05-04. Withdrawn after implementation testing.

## Context

The wipe-on-OTA feature (automatically clearing retained HA discovery topics on boot after a
firmware upgrade) was implemented and tested but proved too complex to run reliably within
ESP8266 resource constraints: PubSubClient buffer limits, cooperative scheduling re-entrancy,
and the 4 KB CONT stack made the required ~1200 empty-retain publishes fragile.

## Decision

The feature is removed from the firmware. Users who need to clean up stale HA discovery
entities after a firmware upgrade must do so manually via an MQTT client (e.g., MQTT Explorer)
or by clearing retained topics on the broker. This is a one-time action per device upgrade and
is more reliable than an automated in-firmware wipe.

## Alternatives Considered

- **Automatic wipe-on-OTA (rejected, this ADR's deprecation cause).** Implemented and tested;
  ESP8266 resource constraints made the ~1200 empty-retain publish sequence unreliable.
- **Manual cleanup via MQTT client (chosen).** One-time per upgrade; user runs MQTT Explorer
  or equivalent to clear retained topics. More reliable than the automated path.

## Consequences

Stale HA entities accumulate after a firmware upgrade until the user manually clears them.
This is a known one-time cost; no firmware-side reliability issue. The companion entity-id
churn from `bSeparateSources` (ADR-068) requires the same manual cleanup path.

## Related Decisions

- **ADR-068:** bSeparateSources mutually exclusive base/source variants — companion ADR that still
  stands. The entity-id churn described in ADR-068 is no longer automatically cleaned up by this
  ADR; users toggling `bSeparateSources` should clear orphaned discovery topics manually.
- **ADR-040:** MQTT source-specific topics (amended by ADR-068).

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->

