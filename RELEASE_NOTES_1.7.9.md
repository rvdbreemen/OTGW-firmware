# OTGW-firmware v1.7.9 Release Notes

**Release date:** 2026-11-04
**Previous release:** v1.7.8
**Platform:** ESP8266 (1.x maintenance line)
**Compare:** [v1.7.8...v1.7.9](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.8...v1.7.9)

## Overview

v1.7.9 is a small release for the 1.x (ESP8266) line with one new option: you can make the gateway connect to WiFi in 802.11g instead of 802.11n. That is for routers where the gateway joins the network but never gets an address, as seen on a Bouygues Bbox Wi-Fi 7. With the option off, which is the default, nothing changes.

No breaking changes versus v1.7.8.

## New features

### Force WiFi 802.11g for routers where the gateway joins but gets no network (TASK-1597)

On some new routers the ESP8266 associates in 802.11n and the router shows it as connected, but it never gets an address, no traffic passes, and after a few minutes the setup portal comes back. In 802.11g the same unit works. The reporter saw this with OTGW-firmware 1.7.7, 1.7.8 and otgwmcu alike, so it is not a firmware regression; their own debug build with 802.11g connected at once.

- New setting **Force WiFi 802.11g** (`wififorce11g` in `/api/v2/settings`, `WifiForce11g` in `settings.ini`), off by default. A change on the settings page takes effect after a restart.
- The WiFi setup portal has a new **WiFi mode** field. Type `11g` there to connect in 802.11g from the very first connection, or leave `auto`. The portal stores the choice in the same setting.
- With the setting off the gateway connects in 802.11n, as before. The firmware now sets that mode explicitly on every start, so switching back to auto always takes effect.
- Only turn it on if you need it. 802.11g is slower, and some routers move all their clients down to b/g once a b/g client joins.
- Tasmota (`Wifi 3`) and ESPHome (`phy_mode: 11G`) offer the same manual option; neither falls back to 802.11g automatically, and neither does this firmware.

Confirmed in the field by robou1337 on their Bouygues Bbox Wi-Fi 7 with v1.7.9-beta.2. Not tested on a bench gateway before release.

## Bug fixes

None.

## Documentation

- The 1.x release skills (`/release`, `/beta-prerelease`) now check that a release commit actually landed before tagging, after `v1.7.9-beta.1` was tagged from a commit without the change. That beta is marked as such on its release page; use v1.7.9 or v1.7.9-beta.2.

## Breaking changes

No breaking changes versus v1.7.8. No MQTT topic renames, no REST API removals, no settings-format changes (one new setting with a default that keeps today's behaviour), and no migration on upgrade.

## Upgrade

Flash **both** firmware and filesystem (the settings page has a new label). Settings are preserved.

## Thank you

Special shoutout to **robou1337**, who wrote up every step they tried on their Bouygues Bbox, built their own debug firmware to find that 802.11g works, and confirmed the beta within an hour of its release.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
