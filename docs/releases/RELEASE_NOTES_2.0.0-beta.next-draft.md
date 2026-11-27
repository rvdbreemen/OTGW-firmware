# OTGW-firmware v2.0.0-beta.NEXT Release Notes (draft skeleton)

Not a release yet. Skeleton for the beta after beta.457, built from what sits in the lane `lane-poc-b2` (tip c6766a02b) and in three web UI lanes (3bb1245fd, 4cfbfadf7, e11cdbfd2). One line per user-visible change; every line needs its evidence checked again at the release commit. Not pushed.

## What is new in this beta, in plain words

### SAT and MQTT

- **A thermostat setpoint that equals a preset selects that preset** (TASK-3032). When the thermostat setting matches the value of a SAT preset, SAT now shows and uses that preset instead of a custom target; a value that matches no preset still clears the preset. Host-tested with the real code; not run on a thermostat.
- **New MQTT commands for the weather location** (TASK-3016): `sat/weather_lat` and `sat/weather_lon` can be set over MQTT, and the values are published as retained state. Host-tested; not run against a broker.
- **New retained flag `thermostat/override_refused`** (TASK-3033). After a `TT=` or `TC=` override the gateway watches whether the thermostat follows it; if the thermostat still differs 300 seconds later the flag becomes true. It is observed behaviour, not something the thermostat announces. Host-tested; not run on a thermostat.
- **Zones: `zone_count` counts extra rooms only** (TASK-3102). The default is 0, the value is limited to 0 to 4, and an installation that has no stored value keeps one zone. Zone topics (`sat/zone/<n>/...`) publish from one extra room with data, `sat/zone_count` is a retained state, and `/api/v2/sat/status` carries a `zones` array. Host-tested; no unit run yet.

### OTGW32

- **Better pin defaults on the OTGW32** (TASK-3120). The Dallas temperature sensor default is pin 4 (the old default 10 is reserved on this board, so sensors never started), the S0 pin stays unset until the digital-input pad is confirmed, and the GPIO output is hidden on this board. Not verified: no OTGW32 unit run with these defaults yet.

### Web UI

- **Help popover stays inside its card; chart axis label no longer touches the axis** (TASK-3058, TASK-3052). Checked in a real browser layout at 375 px and 1400 px with synthetic screenshots.
- **Clearer English settings help** (TASK-3118): `ethdns` names the router, and the PWM cycle help gives minutes like the dropdowns. Other languages follow the translation sync.
- **The page reconnects politely** (TASK-3124): the web socket retries after 5, 10, 20, 40 and then 60 seconds with some jitter, pauses after three sockets that opened and closed within a minute (a Reconnect button resumes), and no stale "page is reconnecting" hint shows while the page is paused. Tested in headless Chrome; not tried with several real browsers or a sleeping tab.

### Manual

- Manual rows for the new MQTT topics and the zone changes (TASK-3016, TASK-3102, TASK-3120).

## Not verified yet, in plain words

- 3032, 3033, 3102, 3016: host tests only; no thermostat, boiler or broker run.
- 3120: no OTGW32 unit run with the new defaults.
- 3124: headless browser only.

## Known issues

(carry over from beta.457; add what the next bench shows)
