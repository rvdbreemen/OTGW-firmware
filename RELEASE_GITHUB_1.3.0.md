v1.3.0 focuses on safer OTA/LittleFS updates, better WiFi recovery, optional protection for admin endpoints, fuller `PS=1` support, and lower RAM pressure.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md)

## New features

- **Optional protected admin endpoints:** Settings, maintenance, file-management, reboot, and OTA routes can now be protected with HTTP Basic Auth using the Protected Endpoints Password setting.
- **Configurable MQTT publish gating:** OpenTherm and `PS=1` summary data can now be rate-limited to reduce MQTT broker load and WiFi chatter.
- **Full `PS=1` summary integration:** `PS=1` output is now parsed into the normal data pipeline, published to MQTT, and exposed through Home Assistant discovery.
- **Monitor-page command bar:** Send one-shot OTGW PIC commands such as `TT=20.5` or `GW=R` directly from the Web UI and watch the response in real time.
- **Triple-reset WiFi recovery:** Three quick hardware resets reopen the captive portal and clear stale WiFi credentials without requiring a reflash.
- **Safer OTA / LittleFS flashing:** The updater now uses `/api/v2/health` for reboot validation, supports browser backups of `settings.ini` and `dallas_labels.ini`, restores data more cleanly after filesystem flashes, and logs upload progress over telnet.
- **Richer status reporting:** Heap and simulation visibility are clearer in the UI and device-info path, settings fields now have better descriptions, and OTGW event reporting is more complete over MQTT and WebSocket.

## Fixes and stability

- **Boot-time restart cleanup:** Prevents avoidable service restarts immediately after startup.
- **Hostname normalization fix:** Dot-stripping now targets the correct hostname buffer.
- **Webhook payload fix:** Long webhook payloads no longer get truncated during settings load.
- **File deletion fix:** File Explorer delete handling works consistently again.
- **IP validation fix:** Valid addresses with an octet of `255` are no longer rejected incorrectly.
- **Filesystem flash corruption fix:** WiFi reconnect activity is suppressed during flash writes, and LittleFS erases the full partition before writing.
- **Lower heap churn:** JSON and settings persistence paths now use tighter bounded-buffer handling instead of heavier dynamic allocation patterns.

## Upgrade notes

- **No new breaking changes vs `v1.2.0`:** No new MQTT topic renames, REST API removals, or settings-format migrations were introduced in this release. The new auth feature is optional and disabled by default.
- **Flash both firmware and filesystem:** The Web UI and OTA changes are best taken together.
- **Hard-refresh the browser after flashing.**
- **If upgrading from older than `v1.2.0`:** Review the earlier MQTT and API migration notes first.

## Thank you

Thank you to everyone testing, reporting edge cases, and pushing the firmware toward a safer release. The OTA, recovery, and `PS=1` work in this release directly reflects that feedback loop.

Special thanks to: @hvxl, @sjorsjuhmaniac, @DaveDavenport, @DutchessNicole, @RobR, @GeorgeZ83, @tjfsteele, @vampywiz19, @Stemplar, @proditaki, and everyone in the Discord.
