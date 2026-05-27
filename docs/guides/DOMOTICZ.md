# Using OTGW-firmware with Domoticz

This guide explains how to integrate the OpenTherm Gateway with [Domoticz](https://www.domoticz.com/) over MQTT.

## TL;DR — no firmware change needed

OTGW-firmware already publishes **Home Assistant MQTT Discovery** messages. Modern Domoticz can ingest those messages directly through its **MQTT Auto Discovery Client** hardware type. No OTGW firmware change and no per-device mapping is required — point Domoticz at the same broker and discovery prefix that OTGW uses.

> **Do NOT use the legacy `domoticz/in` path.** Domoticz also has an older native MQTT protocol that publishes/consumes `domoticz/in` and `domoticz/out` with per-device numeric `idx` JSON. OTGW does **not** publish to `domoticz/in`, and you should not try to bridge it there. That path is stateful, requires manually creating every device and mapping `idx` values by hand, and is entirely superseded by the Auto Discovery Client. Use the discovery path described below.

## Prerequisites

- A running MQTT broker (e.g. Mosquitto) reachable by both OTGW and Domoticz.
- OTGW MQTT enabled and connected (Settings → MQTT). Confirm with `mosquitto_sub -h <broker> -v -t '<haprefix>/#'` — you should see retained `.../config` messages.
- A recent Domoticz release (the *MQTT Auto Discovery Client Gateway* hardware type, which supports the Home Assistant discovery convention).

## Step 1 — Add the Auto Discovery Client hardware

In Domoticz: **Setup → Hardware**, add:

> **Type:** `MQTT Auto Discovery Client Gateway with LAN interface`

Fill in:

- **Remote Address / Port**: your MQTT broker.
- **Username / Password**: if your broker requires them.
- **Auto Discovery Prefix**: the topic root Domoticz watches (default `homeassistant`).

## Step 2 — Match the discovery prefix

Domoticz's *Auto Discovery Prefix* must equal OTGW's discovery prefix, the setting **`settings.mqtt.sHaprefix`** (Web UI: *MQTT → HA prefix*; REST/JSON key `mqtthaprefix` / `MQTThaprefix`).

| Side | Setting | Default |
|------|---------|---------|
| OTGW | `settings.mqtt.sHaprefix` | `homeassistant` |
| Domoticz | Auto Discovery Client → *Auto Discovery Prefix* | `homeassistant` |

With defaults on both sides, no change is needed. If you customised either, set both to the same string.

## Step 3 — Enable the discovered devices

After the hardware is added and the broker connects, OTGW's entities appear under **Setup → Devices**. Domoticz keeps newly discovered devices inactive until you enable them — find the OTGW entities and click the green arrow to add each one you want, then use them on the Dashboard / in scenes.

## How the topics line up

OTGW publishes discovery configs to (see [`docs/api/MQTT.md`](../api/MQTT.md) → *Home Assistant Auto-Discovery*):

```
{haprefix}/{component}/{node_id}/{object_id}/config
```

- `{haprefix}` = `settings.mqtt.sHaprefix` (default `homeassistant`)
- `{component}` = `sensor`, `binary_sensor`, `climate`, `number`
- `{object_id}` is a flat, sibling-suffixed identifier matching `[a-zA-Z0-9_-]+` (ADR-071)

Entity **availability** uses the MQTT LWT/birth message (`avty_t` → `online`/`offline`); see [`MQTT_LWT.md`](MQTT_LWT.md). Discovery configs are **retained** on the broker (ADR-062/ADR-073), so Domoticz rediscovers everything after a Domoticz restart without an OTGW republish.

## Caveats

- **Subset support.** Domoticz's Auto Discovery Client implements a subset of the Home Assistant discovery spec. OTGW stays within the common, standard subset, so its entities map; any future HA-only construct would simply not produce a Domoticz device.
- **Retained configs are required.** If retained topics are cleared on the broker, trigger a republish from OTGW (Web UI / `POST /api/v2/otgw/discovery`) so Domoticz can rediscover.
- **One integration path only.** Use the Auto Discovery Client *or* nothing — never additionally try to wire OTGW into `domoticz/in`. There is no OTGW support for it by design (YAGNI: the discovery path already covers every entity).
- **Security.** Plain-MQTT, trusted-LAN integration only. OTGW uses HTTP/MQTT and assumes a local, non-internet-exposed network; use a VPN for remote access.

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| No OTGW devices appear | Prefix mismatch — confirm `settings.mqtt.sHaprefix` equals the *Auto Discovery Prefix*. Verify configs exist: `mosquitto_sub -v -t '<haprefix>/#'`. |
| Devices appear but show no value | They exist as discovery configs but the value hasn't been published yet (JIT discovery, ADR-073) — values arrive once the MsgID is seen on the OpenTherm bus. |
| Device shows as "off"/unavailable | Availability topic not received — check the LWT/birth topic publishes `online` (see `MQTT_LWT.md`). |
| Duplicate / stale devices | Old retained configs from a previous topic shape. Clear zombie/orphan retained configs per [`docs/api/MQTT.md` — Migration note (zombie discovery configs from beta.21, ADR-071)](../api/MQTT.md#migration-note-zombie-discovery-configs-from-beta21-adr-071), then republish. |

## Related

- [`docs/api/MQTT.md`](../api/MQTT.md) — authoritative MQTT topic & discovery reference
- [`MQTT_LWT.md`](MQTT_LWT.md) — availability (online/offline) mechanism
- [`OPENHAB.md`](OPENHAB.md) — the equivalent guide for openHAB
- Domoticz docs: [MQTT Auto Discovery](https://www.domoticz.com/wiki/MQTT)
