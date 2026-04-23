# Findings Handoff: `v1.4.1..dev`

## Scope

Reviewed the current `dev` branch against the latest GitHub release, verified with:

```text
gh release list --repo rvdbreemen/OTGW-firmware --limit 10
```

Latest GitHub release is `v1.4.1` (`acd6a1c5`, published 2026-04-21). Review range was `v1.4.1..dev`, with `dev` at `b8295e4e`.

Primary review areas:

- MQTT Home Assistant discovery and publish path changes
- Heap diagnostic MQTT telemetry split
- OTA/reboot path changes
- Web UI/CSS/JS changes
- Release and upgrade documentation

## Findings

### 1. High: Stats HA discovery pseudo-ID 247 is not reachable from the async drip path

The 17 new heap/discovery stats sensor entries were added with `id = 247`, but `mqttHaSensorIndex[247]` still points to `0xFFFF`.

Evidence:

- `src/OTGW-firmware/mqtt_configuratie.cpp:995` starts the 17 `{247, ...}` stats entries.
- `src/OTGW-firmware/mqtt_configuratie.cpp:1333` has `0xFFFF, // id 247`.
- `src/OTGW-firmware/MQTTstuff.ino:1249` manually marks `OTGWheapstatsid` pending.
- `src/OTGW-firmware/MQTTstuff.ino:1469` relies on `readSensorIndex(OTid)`.
- `src/OTGW-firmware/MQTTstuff.ino:1508` returns `false` if no sensor/binary/climate/number entry was published.

Impact:

- Normal startup async discovery eventually reaches ID 247, finds no indexed entries, and returns `false`.
- The pending bit remains set, so the drip retries ID 247 every drip interval.
- The 17 stats discovery configs are not published by the normal drip path.

Validation performed:

```text
sensor entries 306 unique 119 mismatches 1
{"id":247,"val":65535,"exp":289,"count":17}
binary entries 53 unique 10 mismatches 0
```

Suggested fix:

- Set `mqttHaSensorIndex[247]` to `289` with comment `// id 247, 17 entries`.
- Add an `evaluate.py` gate that parses `mqttHaSensors[]` / `mqttHaSensorIndex[]` and fails on mismatches.

### 2. High: Stats HA discovery config topics use slashes in the discovery object ID

The new stats labels are full state-topic suffixes such as:

```text
otgw-firmware/stats/ws_drops
```

The discovery topic builder uses the label directly as the Home Assistant discovery object ID:

- `src/OTGW-firmware/mqtt_configuratie.cpp:229` defines stats labels with `/`.
- `src/OTGW-firmware/mqtt_configuratie.cpp:2027` starts `buildSensorDiscoveryTopic`.
- `src/OTGW-firmware/mqtt_configuratie.cpp:2038` emits `%s/sensor/%s/%s/config` with `labelBuf` as the object ID.
- `src/OTGW-firmware/mqtt_configuratie.cpp:2075` passes `cfg.label` into that builder.

Impact:

Even after fixing the ID 247 index, stats configs will be published to topics like:

```text
homeassistant/sensor/<nodeId>/otgw-firmware/stats/ws_drops/config
```

Home Assistant documents the discovery topic as:

```text
<discovery_prefix>/<component>/[<node_id>/]<object_id>/config
```

and states `<object_id>` may only contain `a-zA-Z0-9_-`. Official docs:

```text
https://www.home-assistant.io/docs/mqtt/#discovery-topic
```

So the current topic shape is likely ignored by HA or at least outside the supported contract.

Suggested fix:

- Separate the discovery object ID from the MQTT state topic suffix.
- Recommended shape:
  - discovery object ID: `stats_ws_drops`
  - `uniq_id`: `<nodeId>-stats_ws_drops`
  - `stat_t`: `<mqttPubTopic>/otgw-firmware/stats/ws_drops`
- Implementation options:
  - Add a second field to `MqttHaSensorCfg` for state topic suffix, or
  - Special-case/sanitize ID 247 in the discovery topic and unique ID builders while preserving `stat_t`.

### 3. High: `docs/BREAKING_CHANGES.md` still tells users to use the destructive upgrade order

The README and release notes were corrected to say filesystem first, firmware second. The cumulative breaking changes doc still says firmware first, filesystem second.

Evidence:

- `docs/BREAKING_CHANGES.md:59` starts the "Correct upgrade procedure".
- `docs/BREAKING_CHANGES.md:61` says flash firmware first.
- `docs/BREAKING_CHANGES.md:62` says flash filesystem second.
- `docs/BREAKING_CHANGES.md:64` tells v1.3.x users to wait up to 10 minutes and re-enter settings.

Impact:

Users who follow the cumulative breaking-changes doc can do exactly the upgrade order that the README and release notes say causes a 5-10 minute unresponsive boot and settings loss.

Suggested fix:

- Change the documented order to:
  1. Download both binaries.
  2. Flash the filesystem binary first.
  3. Flash the firmware binary second.
  4. Hard-refresh the browser.
- Remove the "wait up to 10 minutes and re-enter settings" line from the correct path. That belongs only in a "if you already did it wrong" recovery note.

### 4. Medium/Low: New required Web UI assets are not protected from deletion in FSexplorer

The Web UI now depends on `ds-tokens.css` and the `fonts/` assets, but FSexplorer's protected file list was not updated.

Evidence:

- `src/OTGW-firmware/data/index.html:15` loads `ds-tokens.css`.
- `src/OTGW-firmware/data/ds-tokens.css` loads `fonts/inter-400.woff2`, `fonts/inter-700.woff2`, and `fonts/jetbrains-mono-400.woff2`.
- `src/OTGW-firmware/data/FSexplorer.html:213` only protects older assets.

Impact:

A user can delete `ds-tokens.css` or the fonts from the Web UI file explorer. The UI will fall back or partially break styling, and the root cause will be non-obvious.

Suggested fix:

- Add `ds-tokens.css`, `index_dark.css`, `FSexplorer_dark.css`, `graph.js`, and firmware-owned image/font assets to the protected list.
- Prefer a generalized rule that blocks deletion of firmware-owned Web UI assets and directories.

## Verification Limitations

Could not run the normal evaluator:

```text
.\.venv\Scripts\python.exe evaluate.py --quick --no-color
```

failed with:

```text
did not find executable at 'C:\Users\rvdbr\AppData\Local\Programs\Python\Python314\python.exe': Access is denied.
```

Also:

- No global `python` command is available.
- `uv run python evaluate.py --quick --no-color` failed because the local uv cache path could not be created.
- `make` is not available in this shell.
- `arduino-cli` is not available in this shell.

So no firmware build was run during this review.

## Worktree Note

After the review, the worktree showed modified version files:

```text
src/OTGW-firmware/data/version.hash
src/OTGW-firmware/version.h
```

The diff is an autoinc-style bump from:

```text
1.4.2-beta+62fdacd / build 3161
```

to:

```text
1.4.2-beta+b8295e4 / build 3162
```

These were not intentionally edited as part of the review and were not reverted.

Git also repeatedly emitted:

```text
warning: unable to access 'C:\Users\rvdbr/.config/git/ignore': Permission denied
```

This warning did not block the review.

## Recommended Next Agent Plan

1. Fix the ID 247 index mismatch and add an evaluator check for discovery index consistency.
2. Fix the stats discovery object ID/state topic coupling so HA receives valid discovery config topics.
3. Correct `docs/BREAKING_CHANGES.md` upgrade order.
4. Harden FSexplorer protected assets.
5. Repair the local Python/build environment enough to run:
   - `evaluate.py --quick --no-color`
   - firmware build
   - filesystem build
6. Only after those pass, decide whether the pending version-file bump should be kept, updated, or reverted by the release/build process.
