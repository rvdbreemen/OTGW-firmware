# OTGW-firmware v1.7.7 Release Notes

**Release date:** 2026-10-04
**Previous release:** v1.7.6
**Platform:** ESP8266 (1.x maintenance line)

## Overview

v1.7.7 is a small feature and fix release for the 1.x (ESP8266) line. The temperature sensor wired to the PIC can now reach MQTT and Home Assistant. Two tools on the same computer no longer knock each other off port 25238 when one of them reconnects. And a Telegraf scrape is no longer refused while a web interface tab is open.

No breaking changes versus v1.7.6. One behaviour change: the two rate-limited REST endpoints allow a burst of 2. Details below.

## New features

### The PIC temperature sensor reaches MQTT and Home Assistant (TASK-1147)

The PIC reports the temperature sensor wired to its GPIO port through `PR=E`, but the firmware never asked for it, so the value only showed up in the raw OpenTherm log. A new setting, **PIC Temperature Sensor** (off by default, in the PIC section of the settings page), makes the gateway send `PR=E` once every 3 minutes and publish the reading to `otgw-pic/temperature_reading`, with a Home Assistant temperature sensor named **PIC Temperature Reading**.

- The entity is announced on the first valid reading only. A PIC without a sensor answers `-`, and then nothing is published and no entity appears, so a missing entity means no sensor, not a broken build.
- It is separate from the existing **PIC Temp Sensor** diagnostic entity, which shows the sensor's configured function (`PR=D`), not a temperature.
- With the setting off, the gateway never sends `PR=E`. On a bench gateway: 0 `PR=E` in 200 seconds with the setting off, and 3 in 400 seconds with it on, with no effect on OpenTherm traffic.

Requested by indigo_light, whose boiler has no outdoor probe, and confirmed on their own gateway: the reading arrives in Home Assistant and matches the web interface.

## Bug fixes

### Port 25238: two tools on one computer no longer evict each other (TASK-1170)

Port 25238 lets a new connection from an address that already holds a slot replace that slot, so a tool that crashed gets its place back. With both slots held by one computer, for example Domoticz and OTmonitor, it replaced the first slot with that address, which could be the other, healthy tool. The two tools then kept replacing each other while the stale connection stayed.

It now replaces the connection with the most unacknowledged data waiting, which is the one whose peer has stopped taking data. When there is no difference, it behaves as before. Measured on a bench gateway with a hung old connection: the two tools evicted each other for over 50 seconds before, 17.5 seconds after. The remaining time is how long a hung tool's own receive buffer keeps acknowledging data.

Reported and confirmed by iandury_.

### A Telegraf scrape is no longer refused while a web interface tab is open (TASK-1188, ADR-098)

The OpenTherm poll (`/api/v2/otgw/otmonitor` and its alias `/api/v2/otgw/telegraf`) is served from one budget shared by every client, and that budget allowed one request per 1500 ms window. An open tab polls every 2 seconds, so a scrape that arrived within 1500 ms of the tab's last poll was refused. Telegraf collects at a fixed phase while the tab only moves its phase when it is refused itself, so the two settled into the arrangement where Telegraf was refused every time.

Simulated against the gateway's own limiter code, a 10-second scrape beside one open tab got 0.6% of its scrapes answered, and in 750 of 1000 starting phases none at all in 10 minutes. The budget now allows a burst of 2 and keeps the sustained rate at one request per window, so the scrape and the tab are both served: 100% of the scrapes in the same simulation. The status poll (`/api/v2/device/time`) gets the same burst.

## Behaviour change

### The rate-limited endpoints allow a burst of 2, and the 429 body states the wait (TASK-1188, ADR-098)

- A client that polls as fast as it can still gets one request per window, plus the burst of 2.
- The 429 answer carries `retry_after` (in seconds) in its `application/problem+json` body, with the same value as the `Retry-After` header. A web page on another origin cannot read `Retry-After`, because it is not a CORS-safelisted response header, but it can read the body.
- `RateLimit-Policy` reports a quota of 2, and the `detail` text names the burst.

## Upgrade

Flash **both** firmware and filesystem. Settings are preserved. The new PIC Temperature Sensor setting starts off.

## Thank you

indigo_light asked for the PIC sensor and confirmed it on their gateway. iandury_ reported the port 25238 takeover with Domoticz and OTmonitor on one computer, and confirmed the fix. Schelte Bron explained what `PR=E` reports and how often it is worth asking.
