# WiFi recovery: reset the gateway three times to open the setup network

Use this when the gateway can no longer reach your WiFi, for example because you changed your router or your WiFi password. It works on 2.x gateways with an ESP32-S3 module. It clears the stored WiFi login and opens the gateway's own setup network, so you can pick your WiFi again.

## Recovery steps

1. Power the gateway and wait for it to finish starting.
2. Press the reset button, or pull the plug (or the USB cable that powers it) and put it back. Wait for the board to start again.
3. Do it a second time. Wait for it to start again.
4. Do it a third time. Each reset must come within about 10 seconds of the board coming back up.
5. The gateway now opens its setup network. On your phone or laptop, connect to the network named `<hostname>-<MAC address>`: the gateway's name (default `OTGW2` on 2.0.0), a dash, and the board's MAC address with colons, for example `OTGW2-A4:CF:12:21:B4:F8`. It has no password.
6. A setup page opens by itself, or open `http://192.168.4.1/` yourself.
7. Choose your WiFi network, enter its password and save. The gateway restarts and joins that network.

> **Be aware:** a power supply that drops out three times in quick succession (each within 10 seconds of the previous return) looks exactly like this gesture and clears the WiFi login too. The gateway then waits in the setup page until you set the WiFi again.

## If it does not work

- If the setup network does not appear, repeat the three resets with shorter intervals, so each falls inside the 10-second window.
- A reset sent from a computer over USB does not count. Use the button or the power plug.
- If you do not save within 240 seconds, the gateway restarts without a stored network and opens the setup network again. It does not go back to the old network.
- After a successful save, the gateway starts normally again.

## Two different networks

A gateway can open two different WiFi networks of its own (an access point is a network a device creates itself). Do not mix them up:

| | Setup network | Fallback network |
|---|---|---|
| Name | `<hostname>-<MAC address>`, for example `OTGW2-A4:CF:12:21:B4:F8` | `OTGW-<last 3 bytes of the MAC>`, for example `OTGW-21B4F8` |
| Password | none | `otgw1234` |
| When | first start, **Reset WiFi** in the web UI, the triple reset | the gateway has a stored WiFi network but cannot reach it |
| What you do | pick your home network on the setup page | open `http://192.168.4.1/`; the gateway keeps trying your network |

## How it works

Reset the gateway **three times in a row**, each within **10 seconds** of the board coming back up. A reset is either of these, and they may be mixed: press and release the **reset button**, or **pull the plug** and put it back. The firmware starts between resets, so wait for the board to come up before the next one. On the third reset it starts the setup network (the WiFiManager configuration portal) instead of connecting to your saved network.

Why both count: the ESP32-S3 reports a press of the reset button and a power-on with the same reset reason, so the firmware cannot tell them apart (ADR-238). Pulling the plug three times is therefore a supported route, which helps on boards without a reachable reset button.

What does not count: a software restart (an update, a restart from the web UI, the firmware's own WiFi recovery restart), a crash or watchdog reset, and a reset from a computer over USB. These never start the recovery.

If a reset comes more than 10 seconds after the board came up, the counter is cleared and the sequence starts over. This window keeps an ordinary single restart or a single power cut from entering recovery.

The triple reset clears the stored WiFi credentials (ADR-043, ADR-238) and then opens the setup network.

Other routes clear the stored WiFi credentials directly, which is a different mechanism from the triple reset: the web UI reset (`/ResetWireless`, the **Reset WiFi** button) and the Telnet `resetwifi` command. On the 1.x firmware (ESP8266) only the reset button counts; a power cycle does not. See that line's documentation.
