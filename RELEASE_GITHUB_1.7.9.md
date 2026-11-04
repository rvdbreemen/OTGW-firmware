**New option: force WiFi 802.11g, for routers where the gateway joins but gets no network.**

v1.7.9 is a small release for the 1.x (ESP8266) line. No breaking changes versus v1.7.8.

Full release notes: [RELEASE_NOTES_1.7.9.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.9.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## New

- **Force WiFi 802.11g.** On some new routers (seen on a Bouygues Bbox Wi-Fi 7) the ESP8266 associates in 802.11n but never gets an address, and the setup portal comes back after a few minutes. In 802.11g it works. Turn on "Force WiFi 802.11g" in the settings page and restart, or type `11g` in the new "WiFi mode" field of the WiFi setup portal. Off by default: nothing changes unless you turn it on. (TASK-1597)

## Notes

- Only turn it on if you need it: 802.11g is slower, and some routers move all clients down to b/g once a b/g client joins.
- Confirmed in the field on the reporter's Bbox with v1.7.9-beta.2; not tested on a bench gateway before release.
- `v1.7.9-beta.1` was tagged by mistake from a commit without this change. Skip it.

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

## Thank you

Special shoutout to **robou1337**, who documented every step on their Bouygues Bbox, found with their own debug build that 802.11g works, and confirmed the beta within an hour of its release.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
