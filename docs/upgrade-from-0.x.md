# Upgrading from 0.9.x / 0.10.y to 1.3.x

**For users who are happy with their current setup.**

Your gateway works, your boiler heats, your thermostat talks. That's great. This page explains what the 1.x firmware line brings to the table so you can make an informed decision about whether and when to upgrade.

---

## Stability and reliability improvements

These are fixes for problems that may not be obvious day-to-day but affect long-term reliability.

**Memory management**
Hundreds of string literals have been moved from RAM to flash (PROGMEM). On an ESP8266 with roughly 40 KB of usable RAM, this reduces heap fragmentation and lowers the chance of unexpected reboots after days of uptime. The ArduinoJson library was removed entirely in v1.3.0, and the `String` class was eliminated from all frequently-executed code paths. The result is more predictable memory usage over time.

**WiFi reconnection**
The old firmware used a blocking 30-second loop to reconnect WiFi. During that time, nothing else ran -- no MQTT, no OpenTherm processing, no watchdog feeding. In v1.3.0 this was replaced with a non-blocking state machine. Your heating continues to work while WiFi recovers in the background.

**MQTT reconnection**
Reconnection logic after broker restarts (common during Home Assistant updates) has been made more robust and reliable.

**Crash fixes**
Several known crash paths were fixed: binary data handling during PIC firmware uploads, buffer overflows in MQTT, and a `strstr_P` argument inversion that caused Exception (2) on certain PIC command lookups.

---

## Ser2net and OTmonitor compatibility

If you use OTmonitor or another ser2net client on TCP port 25238, [v1.3.1](../RELEASE_NOTES_1.3.1.md) introduced proper coordination between the firmware's internal command queue and ser2net traffic (see [ADR-059](adr/ADR-059-ser2net-queue-awareness.md)). Previously, time-sync commands (SC=) and date commands (SR=) bypassed the command queue entirely, which could collide with commands sent from OTmonitor.

Now all commands go through a single queue, and the firmware detects ser2net activity and briefly pauses its own queued commands to avoid conflicts on the PIC serial bus. The `NTPsendtime` setting lets you disable time synchronization to the gateway if your ser2net workflow handles time independently.

---

## Home Assistant auto-discovery

This is probably the most visible improvement for Home Assistant users.

- **[v1.0.0](../RELEASE_NOTES_1.0.0.md)** introduced MQTT auto-discovery for the most common heating and DHW sensors.
- **[v1.2.0](../RELEASE_NOTES_1.2.0.md)** expanded this to cover the full OpenTherm protocol: **309 discovery configurations** across 80+ message IDs. Cooling, solar thermal, ventilation (heat recovery), CH2, operational counters, fault diagnostics -- all auto-discovered without manual YAML.

If you have a cooling-capable boiler, solar collectors, or a ventilation unit connected via OpenTherm, those were invisible in older firmware. They now appear automatically in Home Assistant.

---

## Documented REST API and MQTT reference

The 1.x firmware comes with comprehensive API documentation that was not available before:

- **[REST API reference](api/README.md)** -- all v2 endpoints documented with examples, rate-limiting guidance, and integration samples for Home Assistant, Python, and JavaScript. An [OpenAPI 3.0 specification](api/openapi.yaml) is included for tools like Swagger UI or Postman.
- **[MQTT topic reference](api/MQTT.md)** -- full namespace documentation, birth/LWT messages, per-source topic separation (optional), and details on all 309 HA discovery configurations.

If you build your own automations or integrations, the API surface is now well-defined and documented rather than reverse-engineered from the source.

---

## Real-time graphs and Dallas sensor labels

**Graphs ([v1.0.0](../RELEASE_NOTES_1.0.0.md))**
Built-in ECharts-based visualization of boiler temperatures, setpoints, water pressure, and modulation level. Dallas temperature sensors are included in the graph with a 16-color palette. No external tools like Grafana needed for a quick overview. See the [graph implementation details](features/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md).

**Dallas sensor labels ([v1.1.0](../RELEASE_NOTES_1.1.0.md))**
Name your temperature sensors instead of working with raw hardware addresses like `28-0517b2a1c3ff`. Click to rename directly in the Web UI. Labels are stored on the filesystem and survive firmware updates. See the [Dallas sensor documentation](features/dallas-temperature-sensors.md) and [label API reference](api/DALLAS_SENSOR_LABELS_API.md).

---

## PIC gateway settings visibility ([v1.3.0](../RELEASE_NOTES_1.3.0.md))

All 15 PIC configuration registers are now readable via the Web UI, REST API, and MQTT. Setpoint override mode, GPIO functions, LED assignments, thermostat detection, voltage reference, and more -- without connecting a serial terminal. Settings are read from the PIC automatically at boot and on demand.

---

## WiFi recovery without reflashing ([v1.3.0](../RELEASE_NOTES_1.3.0.md))

Three quick resets of the ESP8266 reopen the WiFi captive portal. If you change your WiFi SSID or password and lose access to the gateway, you no longer need a USB cable and a reflash to recover.

---

## Optional HTTP Basic Auth ([v1.3.0](../RELEASE_NOTES_1.3.0.md))

Password protection can be enabled for settings and maintenance endpoints. It is off by default. Useful if the gateway is on a shared network segment.

---

## Webhook support ([v1.2.0](../RELEASE_NOTES_1.2.0.md))

The firmware can fire an HTTP request when a status bit changes -- boiler flame on, fault detected, DHW active, etc. Useful for triggering external automations without going through MQTT. See the [webhook documentation](features/webhook.md).

---

## What to expect when upgrading

Upgrading from 0.9.x or 0.10.y to 1.3.x is a larger jump than between 1.x versions. A few things to be aware of:

- **GPIO default changed**: The default GPIO for Dallas sensors moved to GPIO 10. Verify this matches your hardware, or set it explicitly in settings after upgrade.
- **REST API v0 and v1 removed**: These endpoints return 410 Gone. If you have scripts or integrations calling `/api/v0/...` or `/api/v1/...`, update them to `/api/v2/...`. See the [REST API reference](api/README.md) for the full v2 endpoint list.
- **MQTT topic spelling corrections** ([v1.2.0](../RELEASE_NOTES_1.2.0.md#mqtt-topic-renames-home-assistant-entity-cleanup-needed)): A handful of typos in topic names were fixed (e.g., `eletric_production` became `electric_production`). After upgrading, delete orphaned entities in Home Assistant and let discovery recreate them correctly.
- **Settings format**: Auto-migration is attempted. Verify your settings after the upgrade.

---

## Summary

The 1.x firmware line is a meaningful step forward in stability, Home Assistant integration, API documentation, and diagnostic visibility. None of it requires you to change how your heating system operates -- it is the same OpenTherm gateway underneath. The improvements are in how the firmware manages memory, recovers from network issues, exposes data, and documents its interfaces.
