---
id: TASK-1451
title: >-
  feat-2.0.0: port TASK-1250 (1.x) — keep last good MQTT broker address per name
  and close old session on failed rename
status: To Do
assignee: []
created_date: '2026-10-29T06:22:00Z'
labels:
  - port-from-1.x
  - mqtt
  - network
  - robustness
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 611000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Background
- 1.x TASK-1250 (commits 41608bb07, 01025ea68, 07ffbebd0, 4b3e950d2, 93549213d on otgw-1.x.x) fixed MQTT staying down after a failed broker-name lookup.
- Part of that fix is already on 2.0.0, and 2.0.0 had it first: dev TASK-873 (commit 1d91bbce2) makes MQTT_STATE_WAIT_FOR_RECONNECT re-enter MQTT_STATE_INIT every 42 s (origin/dev:src/OTGW-firmware/MQTTstuff.ino:1449-1463). Do NOT re-port that.
- Two parts of the 1.x fix are missing on 2.0.0.

Gap 1: a failed lookup destroys the last good address
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1319 calls WiFi.hostByName(CSTR(settings.mqtt.sBroker), MQTTbrokerIP), straight into the held address.
- arduino-esp32 sets the result to 0.0.0.0 before resolving (framework NetworkManager.cpp:52). A failed lookup therefore makes MQTTbrokerIP invalid, and INIT goes to ERROR.
- When the broker connection drops while DNS or mDNS (default broker homeassistant.local) does not answer, the gateway cannot reconnect until DNS is back. This holds even when the broker still sits at the same address.
- 1.x behaviour (src/OTGW-firmware/mqttBrokerAddress.h:43-52, MQTTstuff.ino:901-930):
  - Look up into a separate IPAddress.
  - Adopt the result only when isValidIP().
  - Otherwise keep the held address, but only if it was resolved for the same broker name. 1.x tracks that name in MQTTbrokerResolvedFor[sizeof(settings.mqtt.sBroker)] (MQTTstuff.ino:44), written with strlcpy when a result is adopted.
  - Log 'MQTT: lookup of [%s] failed, using the last known address %s'.
- A renamed broker must never fall back to the previous broker's address.

Gap 2: the old session stays open when a renamed broker cannot be looked up
- Trigger: a settings change runs startMQTT() (origin/dev:src/OTGW-firmware/settingStuff.ino:74-77), which forces INIT (MQTTstuff.ino:584) while the old espMqttClient session is still open.
- If the new name's lookup fails, the INIT else-branch (MQTTstuff.ino:1356-1361) goes to ERROR without disconnecting.
- The old session to the previous broker stays up. state.mqtt.bConnected follows MQTTclient.connected() (end of handleMQTT), so the UI and REST report 'connected', and subscriptions on the old broker still deliver commands.
- 1.x commit 93549213d calls doMqttDisconnect() in that branch. On dev, use the existing doMqttDisconnect() (origin/dev:src/OTGW-firmware/MQTTstuff.ino:568-571).
- Once Gap 1 is fixed, the held address can stay valid for the same name. For a different name the branch has no usable address, so it must disconnect, otherwise publishes would go to the old broker. That is exactly the 1.x regression 93549213d fixed. Gap 1 and Gap 2 must therefore land together.

Implementation guidance
- Port mqttBrokerAddress.h (header-only, pure inline decideMqttBrokerLookup(resultValid, heldValid, heldFor, name) returning {bUseResult, bUsable}) into origin/dev src/OTGW-firmware/ and include it from MQTTstuff.ino. Keep the decision host-testable.
- In MQTT_STATE_INIT (origin/dev MQTTstuff.ino:1317-1363):
  - Resolve into a local IPAddress.
  - Call the decision function.
  - On bUseResult, copy the result into MQTTbrokerIP, copy the name into a new static MQTTbrokerResolvedFor[], and rebuild MQTTbrokerIPchar.
  - On bUsable, run the existing setServer/LWT/credentials block.
  - Otherwise call doMqttDisconnect() and go to ERROR.
- Platform: hostByName is synchronous on ESP32 with no late write, so the 1.x 'static lookedUp' is not needed. espMqttClient copies the IPAddress by value, so the 1.x pointer-lifetime comment about MQTTbrokerIPchar does not apply.

Do NOT port
- The 3 s and 10 s lookup timeout changes (07ffbebd0, 4b3e950d2): ESP8266 core API, no equivalent parameter on ESP32.
- The '10 minutes' log-text change: dev already says 42 s.
- The re-resolve-after-wait change: already on dev as TASK-873.

Risks
- A held address for the same name may be stale if the broker moved while DNS is down. That is acceptable: the next successful lookup replaces it, and the alternative is no MQTT at all.
- MQTTbrokerResolvedFor costs about 64 B of RAM (sizeof settings.mqtt.sBroker); negligible on ESP32-S3.
- The host test must compile the real header, not a re-implementation of it.

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1449-1463 — WAIT_FOR_RECONNECT re-enters MQTT_STATE_INIT every cycle (dev TASK-873 'F2', commit 1d91bbce2, field-validated 2026-06-27). The core recovery is already on dev, and dev had it first.
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1319 — WiFi.hostByName(CSTR(settings.mqtt.sBroker), MQTTbrokerIP) writes straight into the held address, so a failed lookup destroys the last good address. Gap 1.
- D:/DevData/platformio/packages/framework-arduinoespressif32/libraries/Network/src/NetworkManager.cpp:52 — arduino-esp32 hostByName sets aResult = 0 before resolving, so a failed lookup leaves MQTTbrokerIP = 0.0.0.0 and isValidIP() (origin/dev:src/OTGW-firmware/helperStuff.ino:119-124) fails.
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1356-1361 — the INIT failure branch goes to MQTT_STATE_ERROR without disconnecting. Gap 2: after a broker rename whose lookup fails (startMQTT via settingStuff.ino:74-77 forces INIT while the old session is open), the old espMqttClient session stays open. state.mqtt.bConnected mirrors MQTTclient.connected() (MQTTstuff.ino:~1484), and the publish guards refuse only because MQTTbrokerIP is 0.0.0.0.
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:52 — no MQTTbrokerResolvedFor equivalent. git grep for 'MQTTbrokerResolvedFor|decideMqttBrokerLookup|mqttBrokerAddress' on origin/dev -- src returns nothing.
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:568-571 — doMqttDisconnect() exists and can be reused.
- origin/dev backlog grep 'hostByName|broker.*lookup' finds only TASK-873 (re-resolve, Done). No dev task for last-known-address retention or for closing the session on a failed rename.

## Platform notes

ESP32 (arduino-esp32 3.x via pioarduino 55.03.35): hostByName goes through getaddrinfo, is synchronous, and on failure sets the result to 0.0.0.0 (NetworkManager.cpp:52), not 255.255.255.255. There is no late-DNS-callback write into the target, so 1.x's 'static IPAddress lookedUp' is harmless but not needed. A local is fine.

espMqttClient's setServer(IPAddress, port) stores the IP by value (dev MQTTstuff.ino:1325-1326). The 1.x concern about PubSubClient keeping a pointer to MQTTbrokerIPchar does not apply.

The 1.x lookup-timeout commits (07ffbebd0 3 s, reverted by 4b3e950d2) are ESP8266 core-specific. Dev's hostByName has no timeout parameter. Do not port them.

The '10 minutes' to '42 seconds' log-text fix is already done on dev (MQTTstuff.ino:1393, 1467).

hostByName blocks the loop task. On dev it already runs once per 42 s cycle while MQTT is down (TASK-873), so this port adds no new blocking.

Dev runs MQTT async (espMqttClient); doMqttDisconnect() only acts when connected(), so it is safe to call in the INIT failure branch.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds and every cited dev line checks out. One strengthening point: on dev, a dropped connection does not first retry against the server address espMqttClient still holds. IS_CONNECTED goes straight to WAIT_FOR_RECONNECT and then INIT (MQTTstuff.ino:1421-1426, 1460). Every reconnect after a drop therefore depends on a successful lookup, which makes Gap 1 deterministic during a DNS/mDNS outage, not just a possible case. The '~1484' is exactly line 1484.

Minor fixes only:
(1) Gap 1 should state the reconnect path explicitly: IS_CONNECTED -> WAIT_FOR_RECONNECT (MQTTstuff.ino:1421-1426) -> INIT (1460). Every post-drop reconnect re-runs hostByName, so during a DNS outage the device cycles INIT->ERROR every 42 s and never reconnects.
(2) Name the publish guards that turn a zeroed IP into a silent no-publish: origin/dev MQTTstuff.ino:1622 and 1667, which check isValidIP(MQTTbrokerIP). After the port, these guards will see the held address for the same name, so AC2 (no publish to a renamed broker's old address) relies on MQTTbrokerIP NOT being updated to a valid value on a rename with a failed lookup. Spell that out.
(3) Change the 'MQTTstuff.ino:~1484' reference to the exact line, 1484.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A failed broker-name lookup in MQTT_STATE_INIT no longer overwrites MQTTbrokerIP: when an address for the SAME broker name is held, the state machine goes to TRY_TO_CONNECT with that address and logs that the lookup failed.
- [ ] #2 A held address is never used for a different broker name: after a rename whose lookup fails, the client does not connect or publish to the previous broker's address.
- [ ] #3 When the INIT step has no usable address, the existing MQTT session is closed (doMqttDisconnect) before entering MQTT_STATE_ERROR; after a failed rename /api/v2 device info and the Web UI no longer report MQTT connected to the old broker.
- [ ] #4 The decision logic lives in a header-only pure function on origin/dev (port of mqttBrokerAddress.h) and is covered by a host test under test/host/ that #includes the real header. The test runs red against the old dev semantics (lookup result written straight over the held address, name ignored) and green on the fix, with the old-vs-fix run counts recorded in the task notes.
- [ ] #5 Bench check on .61 OTGW32 or .63 Classic+S3 with the broker set by hostname: (a) with MQTT connected, make the name unresolvable (for example a MikroTik static DNS override or blocking mDNS), drop the broker connection, and confirm MQTT reconnects to the last known address within one 42 s cycle; (b) rename the broker to an unresolvable name and confirm the old session closes and MQTT shows disconnected. Telnet/MQTT debug transcript attached.
- [ ] #6 Re-resolve after every reconnect wait (dev TASK-873, MQTTstuff.ino:1449-1463) still works: once the name resolves again, MQTT connects without a reboot.
- [ ] #7 Build for esp32, esp32-classic and esp32-combo succeeds and python evaluate.py --quick shows no new failures.
<!-- AC:END -->
