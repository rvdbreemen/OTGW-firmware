**The PIC temperature sensor reaches Home Assistant, and two tools on one computer share port 25238 again.**

v1.7.7 is a small feature and fix release for the 1.x (ESP8266) line. No breaking changes versus v1.7.6.

Full release notes: [RELEASE_NOTES_1.7.7.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.7.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## New

- **The temperature sensor wired to the PIC can reach MQTT and Home Assistant.** Turn on **PIC Temperature Sensor** in the PIC section of the settings page (off by default). The gateway then reads it every 3 minutes and publishes it to `otgw-pic/temperature_reading`, with a Home Assistant sensor **PIC Temperature Reading**. Without a sensor nothing is published and no entity appears. (TASK-1147)

## Bug fixes

- **Two tools on the same computer no longer knock each other off port 25238** when one of them reconnects over a stale connection, for example Domoticz next to OTmonitor. The gateway now replaces the connection that stopped taking data. (TASK-1170)
- **A Telegraf scrape is no longer refused for as long as a web interface tab is open.** The OpenTherm poll budget allows a burst of 2 at the same sustained rate. In a simulation against the gateway's own limiter, scrapes answered went from 0.6% to 100%. (TASK-1188, ADR-098)

## Behaviour change

- The two rate-limited endpoints (`/api/v2/otgw/otmonitor` with its `telegraf` alias, and `/api/v2/device/time`) allow a burst of 2. A 429 now also carries `retry_after` in its JSON body, readable by a web page on another origin. (TASK-1188, ADR-098)

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

## Thank you

indigo_light asked for the PIC sensor and confirmed it on their gateway. iandury_ reported the port 25238 takeover and confirmed the fix. Schelte Bron explained what `PR=E` reports.
