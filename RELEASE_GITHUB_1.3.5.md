v1.3.5 fixes the WiFi reconnection regression reported since v1.3.0 and adds MQTT uptime/version publishing.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.5.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md)

## Bug fixes

- **WiFi reconnection regression (#530):** The WiFi state machine timeout (5s) was too short for ESP8266 association (5-10s+), causing repeated connection cancellations. Timeout increased to 30s, matching v1.2.0 behavior. Devices that went offline periodically since v1.3.0 should now stay connected.

## Improvements

- **MQTT uptime and version publishing:** Firmware now publishes uptime and version info to MQTT on connect for better device visibility.

## Upgrade notes

- No breaking changes vs v1.3.4.
- Flash firmware only (no filesystem changes required).
- If your OTGW went offline periodically since v1.3.0, this release fixes that.

## Thank you

Special shoutout to **tjfs** (Discord) for reporting the WiFi disconnect issue that had been affecting devices since v1.3.0, testing the v1.3.5-beta overnight, and confirming the fix!

Thanks to everyone who contributed to this release through bug reports, testing, and feedback.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.4](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.4)
