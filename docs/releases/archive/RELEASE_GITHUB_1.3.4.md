v1.3.4 fixes MQTT throttle slot suppression, adds Debug Info tooltips, renames "OTGW Connected" to "OpenTherm Active", and adds thermostat-only MQTT support.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.4.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md)

## Bug fixes

- **MQTT throttle slot fix:** Stable values like Room Temperature could become permanently suppressed after a transient publish failure. The throttle slot is now only updated after a successful publish.
- **Debug Information page tooltips:** Tooltips were defined but never wired up to the device info labels. Now visible on hover.

## Improvements

- **Renamed "OTGW Connected" to "OpenTherm Active":** Clearer label with updated tooltip describing what it actually indicates.
- **Thermostat-only MQTT support:** OTGW now stays online via MQTT when only a thermostat is connected (boiler no longer required).

## Upgrade notes

- No breaking changes vs v1.3.3.
- Flash both firmware and filesystem (frontend changes require filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5).

## Thank you

Special shoutout to **brave_quokka** (Discord) for prompting thermostat-only MQTT support with a detailed use-case report, testing v1.3.4-beta, and confirming fixes 1-3!

Thanks to everyone who contributed to this release through testing and feedback:
- **chielh** (Discord): tested v1.3.4-beta and confirmed everything works

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.3](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.3)
