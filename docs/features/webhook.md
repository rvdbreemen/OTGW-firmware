# Webhook Feature

## Overview

The OTGW firmware can call an HTTP endpoint whenever a monitored OpenTherm status bit changes
state. This makes it straightforward to trigger actions in other devices or services — turn a
Shelly relay on when the boiler starts, send state data to a local Home Assistant webhook, update
an OpenHAB item, alert a Node-RED flow, and so on.

The webhook fires **once on each state change** (OFF → ON or ON → OFF), not repeatedly.

---

## Quick start for Home Assistant

These four settings are all you need for a basic HA integration. Replace
`homeassistant.local:8123` with your HA address and `otgw_ch_state` with your webhook ID.

```
WebhookEnabled      : true
WebhookTriggerBit   : 1
WebhookURLon        : http://homeassistant.local:8123/api/webhook/otgw_ch_state
WebhookURLoff       : http://homeassistant.local:8123/api/webhook/otgw_ch_state
WebhookPayload      : {"state":"{state}","tboiler":{tboiler},"tr":{tr},"relmod":{relmod},"flame":{flameon}}
WebhookContentType  : application/json
```

In Home Assistant, create an automation with a **Webhook** trigger (webhook ID `otgw_ch_state`).
Access the received values in your action with `{{ trigger.json.state }}`,
`{{ trigger.json.tboiler }}`, etc.

Both URLs point to the same webhook — the `"state"` field in the payload tells HA whether
heating turned on or off.

---

## Settings

| Setting key          | Type   | Default            | Description                                   |
|----------------------|--------|--------------------|-----------------------------------------------|
| `WebhookEnabled`     | bool   | false              | Master switch — set to `true` to enable       |
| `WebhookURLon`       | string | (empty)            | URL called when trigger bit turns ON (max 100)|
| `WebhookURLoff`      | string | (empty)            | URL called when trigger bit goes OFF (max 100)|
| `WebhookTriggerBit`  | int    | 1                  | Status-flag bit index to watch (0–15)         |
| `WebhookPayload`     | string | (empty)            | Body template for POST; empty = use GET       |
| `WebhookContentType` | string | `application/json` | Content-Type header used with POST requests   |

Settings can also be updated via the REST API:

```
PUT /api/v2/settings
Content-Type: application/json

{"webhookpayload":      "{\"state\":\"{state}\",\"tboiler\":{tboiler}}",
 "webhookcontenttype":  "application/json"}
```

---

## HTTP method: GET vs POST

The method is selected automatically based on whether `WebhookPayload` is configured:

| `WebhookPayload` | Method | Content-Type header            |
|------------------|--------|--------------------------------|
| empty (default)  | GET    | —                              |
| any string       | POST   | value of `WebhookContentType`  |

`WebhookContentType` is ignored for GET requests.

---

## Payload template variables

`{variable}` placeholders in the payload template are replaced at send time with live
OpenTherm values:

| Variable       | Source                          | Example value    |
|----------------|---------------------------------|------------------|
| `{state}`      | Current trigger-bit state       | `ON` or `OFF`    |
| `{tboiler}`    | Boiler flow water temperature   | `72.5`           |
| `{tr}`         | Room temperature                | `20.1`           |
| `{tset}`       | CH water temperature setpoint   | `75.0`           |
| `{tdhw}`       | DHW (hot water) temperature     | `55.3`           |
| `{relmod}`     | Relative modulation level (%)   | `47`             |
| `{chpressure}` | CH circuit water pressure (bar) | `1.80`           |
| `{flameon}`    | Burner flame active             | `true` / `false` |
| `{chmode}`     | Central Heating mode active     | `true` / `false` |
| `{dhwmode}`    | Domestic Hot Water mode active  | `true` / `false` |

Values are formatted with sensible precision (1 decimal for temperatures, 2 for pressure,
0 for modulation). Booleans are lowercase JSON `true` / `false`. Unknown `{placeholders}`
are passed through unchanged — useful for catching typos.

---

## Trigger bit reference

The trigger bit uses the same 16-bit `Statusflags` layout as the GPIO outputs feature:

| Bit    | Flag                               | Default use                     |
|--------|------------------------------------|---------------------------------|
| 0      | Slave fault indication             | —                               |
| **1**  | **Slave CH mode (heating active)** | **Default — fires on CH on/off**|
| 2      | Slave DHW mode (hot water active)  | —                               |
| 3      | Slave flame status (burner on)     | —                               |
| 4      | Slave cooling mode                 | —                               |
| 5      | Slave CH2 mode                     | —                               |
| 8      | Master CH enable                   | —                               |
| 9      | Master DHW enable                  | —                               |
| 10     | Master cooling enable              | —                               |

Bit 1 (slave CH mode) is the default: it goes `ON` when the boiler is actively heating
and `OFF` when it stops.

---

## Platform compatibility

| Platform       | Network    | Method | URL length  | Content-Type     | Works natively?      |
|----------------|------------|--------|-------------|------------------|----------------------|
| Shelly Gen1    | local HTTP | GET    | ~45 chars   | N/A              | Yes                  |
| Shelly Gen2    | local HTTP | POST   | ~60 chars   | application/json | Yes                  |
| Home Assistant | local HTTP | POST   | ~75 chars   | application/json | Yes                  |
| OpenHAB        | local HTTP | POST   | ~55 chars   | text/plain       | Yes (set ContentType)|
| Domoticz       | local HTTP | GET    | ~80 chars   | N/A              | Yes                  |
| Discord        | HTTPS      | POST   | ~120 chars  | application/json | No (relay needed)    |

Discord requires HTTPS and a public internet URL. Both are blocked by the local-only security
policy (ADR-003/ADR-032): the device is a local-network appliance and BearSSL TLS would consume
20–30 KB of heap (>50% of available RAM). Use a local relay — see the Discord example below.

---

## URL restrictions

For security the firmware only targets **local-network hosts** via plain `http://`.

**Allowed:**

- `http://192.168.x.x/...` — RFC 1918 private range
- `http://10.x.x.x/...` — RFC 1918 private range
- `http://172.16.x.x/...` to `http://172.31.x.x/...` — RFC 1918 private range
- `http://169.254.x.x/...` — link-local
- `http://hostname.local/...` — local hostnames resolved via LAN DNS

**Blocked:**

- `https://...` — HTTPS is not supported (ADR-003: TLS memory cost)
- Public IPv4 addresses (non-RFC1918)
- Loopback `127.x.x.x`

---

## Testing

Fire a test webhook without waiting for a real state change:

```
POST /api/v2/webhook/test?state=on
POST /api/v2/webhook/test?state=off
```

The test uses the same URL, payload, and Content-Type as a real trigger.

---

## Integration examples

### Home Assistant — local webhook with boiler state data

See [Quick start for Home Assistant](#quick-start-for-home-assistant) at the top.

For richer HA automations with all boiler data:

```
WebhookEnabled      : true
WebhookTriggerBit   : 1
WebhookURLon        : http://homeassistant.local:8123/api/webhook/otgw_boiler
WebhookURLoff       : http://homeassistant.local:8123/api/webhook/otgw_boiler
WebhookPayload      : {"state":"{state}","tboiler":{tboiler},"tr":{tr},"tset":{tset},"relmod":{relmod},"pressure":{chpressure},"flame":{flameon},"ch":{chmode},"dhw":{dhwmode}}
WebhookContentType  : application/json
```

Access in HA automation actions:

```yaml
action:
  - service: notify.mobile_app
    data:
      message: "Boiler {{ trigger.json.state }}: {{ trigger.json.tboiler }}°C"
```

### Shelly Gen1 — relay follows CH mode

```
WebhookEnabled    : true
WebhookTriggerBit : 1
WebhookURLon      : http://192.168.1.10/relay/0?turn=on
WebhookURLoff     : http://192.168.1.10/relay/0?turn=off
WebhookPayload    : (empty)
```

### Shelly Gen2 — relay via RPC

```
WebhookEnabled    : true
WebhookTriggerBit : 1
WebhookURLon      : http://shelly-plus.local/rpc/Switch.Set?id=0&on=true
WebhookURLoff     : http://shelly-plus.local/rpc/Switch.Set?id=0&on=false
WebhookPayload    : (empty)
```

### OpenHAB — update an item state

OpenHAB's REST API accepts plain-text commands, not JSON. Set `WebhookContentType` to
`text/plain` and use `{state}` as the entire payload (it expands to `ON` or `OFF`, which
is exactly what OpenHAB expects for Switch and String items):

```
WebhookEnabled      : true
WebhookTriggerBit   : 1
WebhookURLon        : http://openhab.local:8080/rest/items/BoilerCH
WebhookURLoff       : http://openhab.local:8080/rest/items/BoilerCH
WebhookPayload      : {state}
WebhookContentType  : text/plain
```

To also push the boiler temperature to a Number item, point a second webhook at
`http://openhab.local:8080/rest/items/BoilerTemperature` with payload `{tboiler}`.

### Domoticz — switch a virtual device

```
WebhookEnabled    : true
WebhookTriggerBit : 1
WebhookURLon      : http://domoticz.local:8080/json.htm?type=command&param=switchlight&idx=5&switchcmd=On
WebhookURLoff     : http://domoticz.local:8080/json.htm?type=command&param=switchlight&idx=5&switchcmd=Off
WebhookPayload    : (empty)
```

Replace `idx=5` with your device index from the Domoticz device list.

### Node-RED — all boiler data on flame-state change

```
WebhookEnabled      : true
WebhookTriggerBit   : 3
WebhookURLon        : http://192.168.1.5:1880/otgw/flame
WebhookURLoff       : http://192.168.1.5:1880/otgw/flame
WebhookPayload      : {"flame":{flameon},"ch":{chmode},"dhw":{dhwmode},"tboiler":{tboiler},"tr":{tr},"tset":{tset},"relmod":{relmod},"pressure":{chpressure}}
WebhookContentType  : application/json
```

### Discord via a local relay

Discord requires HTTPS and a public internet URL, which the firmware cannot call directly.
Use a local Node-RED flow or Home Assistant automation as a relay.

**Node-RED** — receive the OTGW webhook, forward to Discord:

```
[HTTP in]  POST /otgw/discord
  → [Function]  build Discord payload from msg.payload.state / msg.payload.tboiler
  → [HTTP request]  POST https://discord.com/api/webhooks/YOUR_ID/YOUR_TOKEN
```

Configure OTGW to POST to the Node-RED listener:

```
WebhookURLon       : http://192.168.1.5:1880/otgw/discord
WebhookURLoff      : http://192.168.1.5:1880/otgw/discord
WebhookPayload     : {"state":"{state}","tboiler":{tboiler},"tr":{tr}}
WebhookContentType : application/json
```

**Home Assistant** — use a webhook automation that sends a Discord notification via the
HA Discord integration (`notify.discord`).
