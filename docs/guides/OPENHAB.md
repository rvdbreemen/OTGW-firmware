# Using OTGW-firmware with openHAB

This guide explains how to integrate the OpenTherm Gateway with [openHAB](https://www.openhab.org/) over MQTT.

## TL;DR — no firmware change needed

OTGW-firmware already publishes **Home Assistant MQTT Discovery** messages. openHAB does **not** have its own discovery format — its MQTT binding *consumes* the Home Assistant discovery convention directly. So integration is purely an openHAB-side configuration task: point openHAB at the same broker and the same discovery prefix that OTGW uses, and every entity appears automatically.

## Prerequisites

- A running MQTT broker (e.g. Mosquitto) reachable by both OTGW and openHAB.
- OTGW MQTT enabled and connected (Settings → MQTT). Confirm the device is publishing — e.g. `mosquitto_sub -h <broker> -v -t '<haprefix>/#'` shows retained `.../config` messages.
- openHAB 3.x or 4.x.

## Step 1 — Install the bindings

In openHAB, install:

1. **MQTT Binding** (`org.openhab.binding.mqtt`)
2. **HomeAssistant MQTT Components** support — bundled with the MQTT binding in OH3/OH4 (the `mqtt.homeassistant` discovery component). No separate add-on is needed on current openHAB; on older versions install it explicitly.

## Step 2 — Create the MQTT Broker Thing

Add an **MQTT Broker** Thing pointing at your broker (host, port, and credentials if used). Once it is `ONLINE`, openHAB starts listening for Home Assistant discovery messages.

## Step 3 — Match the discovery prefix

openHAB's Home Assistant discovery defaults to the base topic **`homeassistant`**. OTGW's discovery prefix is the setting **`settings.mqtt.sHaprefix`** (Web UI: *MQTT → HA prefix*; REST/JSON key `mqtthaprefix` / `MQTThaprefix`), which also defaults to `homeassistant`.

| Side | Setting | Default |
|------|---------|---------|
| OTGW | `settings.mqtt.sHaprefix` | `homeassistant` |
| openHAB | MQTT binding HA discovery base topic | `homeassistant` |

If you changed either value, set **both** to the same string. With defaults, no change is needed.

## Step 4 — Accept the discovered Things

OTGW-discovered entities appear in the openHAB **Inbox**. All entities that share the same MQTT `device.identifiers` value are grouped into a **single Thing**; each entity becomes a **Channel** (components are exposed as Channel Groups). Add the Thing from the Inbox and link its Channels to Items as usual.

## How the topics line up

OTGW publishes discovery configs to (see [`docs/api/MQTT.md`](../api/MQTT.md) → *Home Assistant Auto-Discovery*):

```
{haprefix}/{component}/{node_id}/{object_id}/config
```

- `{haprefix}` = `settings.mqtt.sHaprefix` (default `homeassistant`)
- `{component}` = `sensor`, `binary_sensor`, `switch`, `select`, `climate`, `number`
- `{object_id}` is a flat, sibling-suffixed identifier matching `[a-zA-Z0-9_-]+` (ADR-097) — the same constraint openHAB's discovery topic matcher expects, so per-source variants like `..._thermostat` (when `settings.mqtt.bSeparateSources` is enabled) map cleanly.

Entity **availability** is driven by the MQTT LWT/birth message (`avty_t` → `online`/`offline`); see [`MQTT_LWT.md`](MQTT_LWT.md). openHAB marks Channels `UNDEF`/offline accordingly.

Discovery configs are **retained** on the broker (the firmware also actively re-verifies retained configs — ADR-062), so openHAB rediscovers everything after an openHAB restart without needing OTGW to republish.

## Caveats

- **Subset support.** openHAB's Home Assistant component implementation tracks the standard, documented HA components and abbreviated keys. OTGW deliberately stays inside that common subset (`sensor`/`binary_sensor`/`switch`/`select`/`climate`/`number` with standard keys), so all OTGW entities map. If a future entity uses an HA-only construct, openHAB silently skips just that entity.
- **Retained configs are required.** If you wipe retained topics on the broker, trigger a republish from OTGW (Web UI / `POST /api/v2/otgw/discovery`) so openHAB can rediscover.
- **Security.** This is a plain-MQTT, trusted-LAN integration. OTGW uses HTTP/MQTT only and assumes a local, non-internet-exposed network; use a VPN for remote access. Do not expose the broker to the internet.

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| Nothing in the Inbox | Prefix mismatch — confirm `settings.mqtt.sHaprefix` equals openHAB's HA discovery base topic. Verify configs exist: `mosquitto_sub -v -t '<haprefix>/#'`. |
| Thing appears but Channels are offline | Availability topic not seen. Check the device's LWT/birth topic is publishing `online` (see `MQTT_LWT.md`). |
| Some entities missing | Those entities not yet seen on the OpenTherm bus (JIT discovery) — they appear once their MsgID is received. Or the entity uses a construct outside openHAB's supported subset. |
| Stale Things after a topic-shape change | Clear zombie/orphan retained configs as documented in [`docs/api/MQTT.md` — Migration note (2.0.0 topic-shape transition)](../api/MQTT.md#migration-note-200-topic-shape-transition), then republish. |

## Related

- [`docs/api/MQTT.md`](../api/MQTT.md) — authoritative MQTT topic & discovery reference
- [`MQTT_LWT.md`](MQTT_LWT.md) — availability (online/offline) mechanism
- [`DOMOTICZ.md`](DOMOTICZ.md) — the equivalent guide for Domoticz
- openHAB docs: [Home Assistant binding](https://www.openhab.org/addons/bindings/homeassistant/)
