# Em-dashes and AI-sounding language in user-facing text (2026-11-26)

Author: otgw-ux-designer. Asked by Robert. Read-only research at origin/dev e986aacd4. Nothing in `src/` or `i18n/` is changed: my lane is docs and backlog, so the replacements below are a spec for otgw-webui (web UI, `i18n/*.json`) and otgw-dev (firmware debug lines).

## Rule proposed

No em-dash (U+2014) in any text a person reads. Replace by what the sentence needs: a colon when the second half explains or instructs, a period when two thoughts stand alone, a comma or parentheses for an aside. No en-dash (U+2013) as a stand-in for an em-dash. Where a lone `—` is the placeholder for "no value" (empty tile, unknown IP), use `-` or a word ("unknown", "n/a") per tile; I did not test which reads best on each tile.

## What I found (counts by script over origin/dev)

| Where | Em-dashes in text people read | Note |
|---|---|---|
| `src/OTGW-firmware/data/v2.js` (English pack and UI strings) | about 104 string lines | the main body |
| `i18n/nl.json`, `de.json`, `el.json` | 34, 34, 35 lines | same keys as English; translations copied the dash |
| `src/OTGW-firmware/data/v2.html` | 9 lines (titles, aria-labels, `—` as empty value) | |
| Firmware debug lines (telnet, `DebugTln` etc.) | about 28 in `Ethernet.ino`, `helperStuff.ino`, `MQTTstuff.ino`, `networkStuff.ino`, `OTDirect.ino`, `OTGW-Core.ino`, `OTGW-firmware.ino`, `SATcontrol.ino`, `SATweather.ino`, `settingStuff.ino` | seen by users in the telnet log and the captures they upload |
| `settingsHelp.h`, `README.md` | 0 | already clean |
| `CHANGELOG.md`, `docs/guides/*` | 7 in CHANGELOG, about 200 in guides (MQTT_API_CONTRACT.md 66, MQTT_STALE_TOPICS_CLEANUP*.md 40, OPENHAB 16, DOMOTICZ 16, OT_THING_FEATURE_PARITY 15, others) | guides are owner-facing: otgw-docs |
| Code comments in `src/` | several hundred more (I did not count them exactly) | not user-facing; leave unless Robert wants them gone |
| En-dash (U+2013) | 9 in `v2.js`, 6-7 in each i18n file | check each; many are number ranges, which are fine |

Other typographic habits that mark generated text, in `v2.js`: 377 lines with curly quotes or apostrophes (`Couldn’t`) and 38 with the one-character ellipsis `…`. These are not errors, but they are inconsistent with the plain `'` and `...` the rest of the project uses, and a mixed set is itself a tell. Decide once: plain ASCII in UI strings (my recommendation, it also avoids encoding trouble in the firmware's PROGMEM strings), and sweep.

## AI-sounding wording

Narrow list from a keyword scan for typical filler (seamless, robust, leverage, effortless, simply, please note, Let's, and similar) plus a read of the hits: the web UI text is mostly plain. Real cases:

- `setup_welcome_body`: "Two quick things and you're done — everything else is read straight from the OpenTherm bus." Chatty and padded. Proposed: "Two settings to check. The rest is read from the OpenTherm bus."
- `onb_welcome_title`: "Let's tune SAT for your system". Marketing "Let's". Proposed: "Tune SAT for your system".
- `sat_presets_hint`: "Pick a preset or set the temperature — SAT works out the boiler." Proposed: "Pick a preset or set the temperature. SAT works out the boiler's setting."
- `setup_gas_desc`, `setup_hp_desc`: noun strings stacked with `&` and "%"; fine in meaning, trim to words ("flow and return temperature, modulation, flame, hot water").
- "Smart" in "Smart power", "Smart features", "SAT Smart Thermostat": SAT is the product name (Smart Autotune Thermostat), keep it; "Smart features" and "Smart power" are vague, name what they hold. I did not open those panels, so no replacement text.

Not found in the strings I scanned: "seamless", "robust", "leverage", "comprehensive", exclamation marks. I did not read every one of the UI and firmware strings; this is a scan plus a reading of hits, not proof that none remain.

## Replacements for the English strings (the 34 pack keys; NL, DE and EL follow the same key list)

| Key (`v2.js` line) | Now | Proposed |
|---|---|---|
| `pic_flash_confirm_title` (132) | Tap again to confirm flashing {{name}} — a bad flash can brick the PIC | Tap again to confirm flashing {{name}}. A bad flash can brick the PIC. |
| `pic_flash_progress` (231) | Flashing {{name}} — {{percent}}% | Flashing {{name}}: {{percent}}% |
| `fs_usage` (246) | used {{used}} of {{total}} — {{free}} free | used {{used}} of {{total}} ({{free}} free) |
| `fs_not_authorized` (250) | Not authorized — open classic /FSexplorer to sign in first | Not authorized. Open the classic /FSexplorer and sign in first. |
| `sys_wifi_cleared` (261) | Wi-Fi cleared — rebooting into AP-mode setup… | WiFi cleared. Rebooting into setup mode... |
| `ws_exp` (272) | This page has a live link to the gateway — the log updates in real time. | This page has a live link to the gateway, so the log updates as it happens. |
| `ws_fix` (273) | Live link dropped — the page is reconnecting; data may be paused. | Live link dropped. The page is reconnecting; data may pause. |
| `mqtt_fix` (278) | Cannot reach the broker — check address, port and credentials. | Cannot reach the broker. Check address, port and credentials. |
| `wifi_fix` (281) | Weak or no signal — move the device or add a repeater. | Weak or no signal. Move the unit or add a repeater. |
| `boiler_fix` (284) | Boiler not answering — check the OT wiring / boiler power. | Boiler not answering. Check the OT wiring and the boiler's power. |
| `therm_fix` (287) | No frames from the thermostat — check the OT wiring. | No messages from the thermostat. Check the OT wiring. |
| `ot_fix` (290) | No OT interface available — check the hardware / board mode. | No OT interface available. Check the hardware and the board mode. |
| `ntp_fix` (293) | No time sync — check the NTP server and internet access. | No time sync. Check the NTP server and internet access. |
| `health_title` (301) | Connectivity health — open the Connection map | Connectivity health: open the Connection map |
| `conn_strip_title` (429) | Connectivity — tap for details | Connectivity: tap for details |
| `sim_title` (430) | Simulation active — values may be replayed, not live from the boiler | Simulation active: values may be replayed, not live from the boiler |
| `ovr_badge_title` (450) | The gateway is injecting values onto the OT bus — click for detail | The gateway is injecting values onto the OT bus. Click for detail. |
| `conn_ovr_aria` (501) | Active gateway overrides — click for detail | Active gateway overrides. Click for detail. |
| `sat_presets_hint` (582) | see above | see above |
| `status_fault` (732) | Fault reported — check the appliance display. | Fault reported. Check the boiler's display. |
| `setup_welcome_body` (819) | see above | see above |
| `setup_no_response` (838) | No response — check broker & port | No response. Check broker and port. |
| `sim_body` (909) | ... looks like a bench setup — run ... | split at the dash into two sentences; builder reads the full string |
| `boiler_unsupported_none` (947) | None — boiler supports all observed messages (or no boiler seen). | None. The boiler supports all observed messages (or no boiler was seen). |
| `title` (1055, 4321) | SAT — Smart Thermostat | SAT: Smart Thermostat |
| `ovr_sub` (1327) | Values the gateway is injecting onto the OT bus — the boiler/thermostat see these, not the original. | Values the gateway is injecting onto the OT bus. The boiler and thermostat see these, not the originals. |
| `wh_auth` (1344) | Auth required — sign in first (HTTP {{status}}) | Sign-in required (HTTP {{status}}). Sign in first. |
| `wh_disabled` (1345) | Webhook disabled — enable it above first | Webhook disabled. Enable it above first. |
| `sup_concl_tonly` (1483) | The thermostat asks for this, but the boiler never answers — your boiler may not implement it. | The thermostat asks for this, but the boiler never answers. Your boiler may not implement it. |
| `sup_concl_reserved` (1485) | Reserved / undefined in the OpenTherm spec — not used. | Reserved or undefined in the OpenTherm spec. Not used. |
| `sat_sim_unavailable` (1751) | A real boiler is connected — bench simulation is unavailable. | A real boiler is connected, so bench simulation is unavailable. |
| `sat_status_retrying` (1752) | Couldn’t load SAT status — retrying… | Could not load SAT status. Retrying... |

Notes for the builder: NL, DE and EL pack lines carry the same dashes; fix them through the translation sync, not by hand, and re-read each for the new sentence break. The word "Wi-Fi" in `sys_wifi_cleared` becomes "WiFi" to match the other UI text (README uses "Wi-Fi"; I recommended "WiFi" in the 2026-11-25 first-run review).

## Firmware debug lines (otgw-dev)

Same rule, simple replacement. Examples: `Network: switching to Ethernet — disabling WiFi` becomes `Network: switching to Ethernet, disabling WiFi`; `WiFi static IP: parse error — falling back to DHCP` becomes `WiFi static IP: parse error, falling back to DHCP`; `[flash] WARN: ... — wrong board config` becomes `[flash] WARN: ... (wrong board config)`. Log lines that tools parse: grep `scripts/` and `tests/` for the old text before changing any of them.

## Acceptance criteria (for a backlog task, not created: needs the coordinator's go)

1. No U+2014 in `v2.js` user strings, `v2.html`, `i18n/*.json` or the firmware debug strings; a test greps these files and fails on a new one (red on the old code, green after).
2. The wording changes in "AI-sounding wording" are applied or explicitly declined by Robert.
3. Curly quotes and `…` in UI strings: decision recorded, sweep applied or waived.
4. `python scripts/run_tests.py` and the web UI test files for the changed strings pass; no test or tool parses an old log line.
5. Screenshots (light and dark, 390 and 1400 px) of the Connection map, setup dialog and a fault tile show the new text without clipping.

## Not verified

- I did not run the UI; all text is read from source. The lines are as of origin/dev e986aacd4.
- The count of "about 104" lines includes strings and a few code lines with `—` as an empty-value placeholder; the exact string count needs the test in AC 1.
- I did not read the NL, DE and EL translations for AI-style phrasing, only counted dashes.
- Code comments, ADRs, plans and reviews (hundreds of dashes) are out of scope here.
- Release notes and Discord posts are not in the repo; Robert's rule already covers them.
