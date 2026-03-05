v1.3.0 brings focused reliability, usability, and memory improvements to the OTGW-firmware.

There are no breaking changes in this release. All APIs, MQTT topic structures, and settings formats remain perfectly backward compatible.

## What's new?
* **Configurable MQTT Publishing Interval:** Use OTPublishGate to configure a minimum publishing interval (in seconds) for OpenTherm variables. This throttles rapid state changes, vastly reducing MQTT broker load.
* **Triple-Reset WiFi Recovery:** Triple-clicking the hardware reset button within 10 seconds now triggers AP mode, making it simple to recover network connectivity without reflashing.
* **Web UI OTGW Commander:** We built a new command bar right on the Monitor page. Type raw OTGW PIC commands (TT=20.5, GW=R) and observe the response in real-time.
* **PS=1 Full Automation:** PS=1 (Print Summary) mode is now fully parsed! Fields are pushed to MQTT and accurately auto-discovered in Home Assistant!
* **OTGW Event Reporting:** Core events from the OpenTherm Gateway PIC are directly pushed over MQTT and WebSockets.
* **Enhanced Memory Profiling:** View free heap and max block sizing straight from the GET /api/v2/device/info API, or observe memory health straight from the Web UI upgrade page.

## Under the Hood
* Replaced ArduinoJson with a custom memory-bounded JSON writer. Flash savings and dramatically less heap fragmentation!
* Smashed multiple bugs, including:
  - An issue causing immediate spuriously service restarts loop-drops upon boot.
  - A bug targeting dot-stripping on the wrong config-pointer during hostname generation.
  - A bug clipping large Webhook payloads during settings initialization.
* Automated our internal workflow heavily with the Github Copilot Agent for evaluating ADR consistency!

📘 **Full detailed documentation:** See the [Release Notes for v1.3.0](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.0.md) on our repository!
