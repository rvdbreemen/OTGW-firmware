# ADR-067 — HA Discovery State Reconciliation on OTA Upgrade

## Status: Deprecated

**Withdrawn after testing.** The wipe-on-OTA feature (automatically clearing retained HA discovery
topics on boot after a firmware upgrade) was implemented and tested but proved too complex to run
reliably within ESP8266 resource constraints: PubSubClient buffer limits, cooperative scheduling
re-entrancy, and the 4 KB CONT stack made the required ~1200 empty-retain publishes fragile.

**Decision (2026-05-04):** The feature is removed from the firmware. Users who need to clean up
stale HA discovery entities after a firmware upgrade must do so manually via an MQTT client
(e.g., MQTT Explorer) or by clearing retained topics on the broker. This is a one-time action per
device upgrade and is more reliable than an automated in-firmware wipe.

## Related

- **ADR-068:** bSeparateSources mutually exclusive base/source variants — companion ADR that still
  stands. The entity-id churn described in ADR-068 is no longer automatically cleaned up by this
  ADR; users toggling `bSeparateSources` should clear orphaned discovery topics manually.
- **ADR-040:** MQTT source-specific topics (amended by ADR-068).
