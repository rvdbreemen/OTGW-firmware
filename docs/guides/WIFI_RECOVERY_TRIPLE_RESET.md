# WiFi Recovery: Force Config Portal with Triple Reset

This guide documents the reset-only recovery flow on the 2.x firmware (ESP32-S3 boards: the OTGW32 and the OTGW
Classic with an ESP32-S3 module). It clears the stored WiFi credentials and opens the WiFiManager configuration
portal, so you can reach a gateway whose WiFi network or password has changed.

## The gesture: three resets in a row

Reset the gateway **three times in a row**, each reset within **10 seconds** of the board coming back up. A reset is
either of these, and they may be mixed:

- press and release the **reset button**, or
- **pull the plug** (or the USB cable that powers it) and put it back.

The firmware boots between resets, so wait for the board to come back up, then reset it again. On the third reset it
boots into the WiFiManager configuration portal instead of connecting to your saved network.

Why both count: the ESP32-S3 reports a press of the reset button and a power-on with the same reset reason, so the
firmware cannot tell them apart (ADR-238). Pulling the plug three times is therefore a supported route, which helps
on boards without a reachable reset button.

What does not count: a software restart (an update, a reboot from the web UI, the firmware's own WiFi recovery
restart), a crash or watchdog reset, and a reset from a computer over USB. These never start the recovery.

If a reset comes more than 10 seconds after the board came up, the counter is cleared and the sequence starts over.
This window is what keeps an ordinary single reboot or a single power cut from entering recovery.

**Be aware:** a power supply that drops out three times in quick succession (each within 10 seconds of the
previous return) looks exactly like this gesture and clears the WiFi credentials too. The gateway then waits in the
configuration portal until you set the WiFi again.

## What happens on the 3rd reset

The device boots straight into the WiFiManager configuration portal:

1. It starts an access point named `<hostname>-XXXXXX`, where `<hostname>` is the configured device hostname
   (default `OTGW`) and `XXXXXX` is the last three bytes of the MAC address, for example `OTGW-21B4F8`.
2. Connect to that access point from a phone or laptop.
3. A captive portal page opens automatically, or browse to `http://192.168.4.1/` manually.
4. Enter your WiFi SSID and password and save. The new credentials replace the old ones, and the device reboots and
   connects to the new network.

The triple reset clears the stored WiFi credentials (ADR-043, ADR-238) and then opens the config portal. If the
portal times out (240 seconds) without a save, the device reboots with no stored network: it re-enters the config
portal (or, on prerelease builds, the AP fallback) rather than reconnecting to the old network.

## Recovery steps

1. Power the gateway and wait for it to finish booting.
2. Press the reset button, or pull the plug and put it back. Wait for the board to boot again.
3. Do it a second time. Wait for it to boot again.
4. Do it a third time, each within roughly 10 seconds of the board coming back up.
5. The device boots into WiFi config mode.
6. Connect to the access point (`<hostname>-XXXXXX`).
7. Open the portal and configure the new WiFi network.

## Notes

- The trigger is only for manual recovery and does not affect a normal single reboot or power cut.
- If the resets are too slow (outside the 10-second window), forced recovery is not triggered.
- The Web UI reset path (`/ResetWireless`) and the Telnet `resetwifi` command both clear the stored WiFi credentials
  directly, which is a different mechanism from the triple-reset trigger.
- On the 1.x firmware (ESP8266) only the reset button counts; a power cycle does not. See that line's documentation.

## Troubleshooting

- If the portal does not appear, repeat the three resets with shorter intervals so each falls inside the
  10-second window.
- A reset from a computer over USB does not count; use the button or the power plug.
- After a successful save, the device returns to normal WiFi startup behaviour.
