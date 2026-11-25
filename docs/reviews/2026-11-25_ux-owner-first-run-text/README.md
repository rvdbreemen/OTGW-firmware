# English a non-technical owner trips over: README first run and docs/guides (2026-11-25)

Author: otgw-ux-designer. Asked by the coordinator; findings only, no text changed (R1 waits on Robert). Read at origin/dev 667854417: `README.md` lines 1-200, `docs/guides/FLASH_GUIDE.md`, `WIFI_RECOVERY_TRIPLE_RESET.md`, `WIFI_FIRMWARE_UPDATE.md`; for cross-checks `docs/manuals/en/ch02`, `ch03`, `ch06`, `ch07` by grep only, not read in full.

Reader: owner of an OTGW32 or an OTGW Classic who can plug in a USB cable and use a web page, but does not know what a filesystem, safeboot, mDNS, PIC or an MQTT broker is.

## Must fix: a wrong instruction

**1. The name of the setup network contradicts itself.** The code builds `<hostname>-<MAC with colons>`, default hostname `OTGW2`, for example `OTGW2-A4:CF:12:21:B4:F8` (`networkStuff.ino:321-332`, `OTGW-firmware.h:136`). README:45, manual ch02:239 and ch06:11 say so. But:

| Where | Says | Problem |
|---|---|---|
| `WIFI_RECOVERY_TRIPLE_RESET.md:36-37` and recovery step 6 | `<hostname>-XXXXXX`, default hostname `OTGW`, example `OTGW-21B4F8` | wrong format and wrong default hostname; this is the page a person reads when WiFi is already broken |
| `ch03-web-interface.md:98` (Reset WiFi) | AP `OTGW-XXXXXX`, "derived from the chip ID" | the firmware has one setup portal only (`startConfigPortal`, `networkStuff.ino:442`), so Reset WiFi opens the same `<hostname>-<MAC>` network; the `OTGW-XXXXXX` form with password `otgw1234` is the separate fallback network (`networkStuff.ino:503-513`, ch06:36) |

An owner who looks for `OTGW-21B4F8` in the phone's WiFi list finds nothing. Fix: one sentence everywhere, the same example, and say once that the fallback network is a different thing. The flash tool text (TASK-2968) names the same network.

## Should fix: the first-run path has gaps

**2. No way to find the unit's address.** README Quick Start step 3: `http://<device-ip>/` or `http://otgw2.local/`. Nothing says where the IP comes from, and `.local` names do not resolve on every phone or network. Suggest: "Find the address in your router's list of connected devices (look for OTGW2). If `otgw2.local` does not open, use that address." I did not verify whether the OLED or the web UI shows the IP, so I name only the router.

**3. First USB flash: the hard part is not described in the README or the guides.** README step 1 gives the commands, but nothing about which USB port, whether a driver is needed, or what to do when the tool finds no board. FLASH_GUIDE.md covers drivers (CP210x, CH340) for the ESP8266 only. Manual ch02:152 describes the first flash; I did not read it past that line. Suggest: one short troubleshooting block next to step 1, or a link to ch02:152 by name.

**4. Step 4 (MQTT) assumes knowledge.** "broker details" and "HA discovery prefix" mean nothing to someone who clicked "install Mosquitto" in Home Assistant. Step 4 should link to the section "Setting Up MQTT with Home Assistant" further down in the README and say in one line what a broker is ("the program in Home Assistant that passes messages between HA and the gateway").

**5. "Unit already on 2.0.0: update over WiFi from the web UI".** The web UI has no obvious place for it; the guide says to open `http://<ip>/update`. Put that address in the README step.

**6. "Which firmware do you need?" asks the owner to know their module.** "Wemos D1 mini or NodeMCU (ESP8266)" versus "LOLIN S3 Mini" is only usable if the owner knows what sits in the socket. Add how to tell: the printing on the module, or the board version in the web UI. I do not know what the web UI shows for 1.x, so I did not name it.

## Should fix: words

**7. Jargon without a line of explanation** (README unless stated): *filesystem* (disclaimer, "a power cut during a filesystem update"), *flash layout* and *USB migration* (step 1 and the highlights), *safeboot updater* (highlights), *captive portal* (step 2), *access point* (step 2), *mDNS* (`WIFI_FIRMWARE_UPDATE.md` section 2), *PIC*, *ESP* (same guide), *WiFiManager* and *NVS*-style wording (recovery guide). Suggested plain words: "web pages and settings storage" for filesystem; "one-time USB step to change how the memory is divided" for migration; "a small recovery program that keeps the unit reachable when an update fails" for safeboot; "the unit's own WiFi network for setup" for access point and captive portal (the page that opens by itself).

**8. Term drift.** gateway / unit / device / board; web UI / web interface / Web UI; Wi-Fi (README) against WiFi (guides). Pick one each: "gateway" for the product, "unit" only in flashing steps; "web UI"; "WiFi" to match the web UI and the setup network wording.

## Should restructure

**9. `WIFI_FIRMWARE_UPDATE.md` is an engineering reference with an owner's title.** The owner needs sections 1 to 3 (about 40 lines). Between them sit task and ADR numbers, byte caps, heap sizes, a cross-version API contract and "(this PIC behaviour is not measured on the bench)". Suggest two files: an owner guide of one page (migrate once, update from `/update`, what to do when it fails) and the engineering reference kept as is. Also move the one fact an owner must know before pressing Update to the top of section 2 in plain words: **the heating may run on the boiler's own settings for 1 to 2 minutes during an update** (now in item 4 of section 2, in engineering words).

**10. `WIFI_RECOVERY_TRIPLE_RESET.md`: steps first.** The numbered "Recovery steps" come after four paragraphs of mechanism (reset reason, ADR-238). An owner in trouble reads the top. Move "Recovery steps" and the warning "a power supply that drops out three times looks exactly like this" to the top, the mechanism to the end.

**11. `FLASH_GUIDE.md` is for the 1.x ESP8266 only.** The title says "ESP8266", and a top note redirects, but the file sits among the 2.0.0 guides. Suggest the title "Flashing the 1.x ESP8266 (not for 2.0.0)" or a move to the 1.x line.

## What works (keep)

- README "Which firmware do you need?" gives a clear fork at the top.
- The disclaimer says plainly what a beta is and what to save first.
- The setup-network example with a real MAC format (README:45) is the right kind of example.
- The recovery guide's "What does not count" and "Be aware" paragraphs are honest and useful; only their position is wrong.

## Not verified

- No non-technical reader read these texts; the jargon list is my judgement from the text, not a test.
- I did not open the web UI or a unit: nothing about what the UI shows (item 2, 6) is checked.
- Manuals ch02, ch03, ch06, ch07 only by grep for the network name; their other first-run text is not reviewed. otgw-docs owns the changes; R1 (README landing) waits for Robert.
