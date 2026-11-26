# TASK-3099 bench: hot-water tap at the end of the SAT PWM ON phase (one page)

Claim under test (Sergeant D entries 499, 500): after a hot-water tap inside a PWM ON phase, the flow that the OFF phase freezes is the
flow of the last tick before the tap. A tap that heated the boiler flow must not lengthen the OFF pause.
Host proof (real PWM block): `python tests/test_sat_pwm_tap_replay.py`. This bench run is the unit check; nothing here is verified on a unit yet.

## Files (sha256 in SHA256SUMS; regenerate with `python scripts/gen_sat_pwm_tap_replay.py`)

| File | Scenario | What the SAT sees in each 1200 s period |
|---|---|---|
| C.txt | control | flame on, flow 50 C, no tap |
| A.txt | tap, flame stays on | second 192-252: DHW-mode bit set, flow climbs to 60 C; then flow falls back to 50 C over 150 s |
| B.txt | tap, flame out | same tap; from 252 the flame bit is off for 96 s while the flow cools 60 -> 54 C (post-draw wait); relight at 348 with flow 54 falling back to 50 C |

Each file replays 2 periods (2400 s) at 750 ms per line. Boiler frames: MsgID 0 (CH active + flame; DHW mode in the tap), MsgID 25 (flow),
MsgID 3 (slave config, DHW present, modulating; before ADR-251 the SAT needed a valid MsgID 3 reply to enter PWM, with ADR-251 in the build it plays no part and is harmless), MsgID 28 (return = flow - 10).
Upload as `/otgw_simulation.log` (FSexplorer `/upload`, field `upload`), keep `satsimulation` false, start with `POST /api/v2/simulate/start`
(recipe of scripts/gen_sat_pwm_replay.py; save and restore any existing otgw_simulation.log).

## SAT settings

Gas boiler, 3 cycles per hour (ON 264 s, OFF 936 s at duty 0.22), SAT enabled and in PWM. Duty = (request - base) / (flow - base), base 27.2 C
(radiators): duty 0.22 at flow 50 C needs a request of about 32.2 C. The request comes from SAT itself (heating curve, PID, room, outside),
open loop in a replay, so tune room temperature / target until `pwm_duty` reads about 0.22 in the C run. If that is not reproducible, do
not chase 0.22: compare each run with the C run of the same session (below). Setting names: confirm them on the unit; I did not look them up.

## What to read (GET /api/v2/sat/status, written by satSendStatusJSON(), SATcontrol.ino:4921)

- `pwm_duty` (:5065): the duty now. Read it in the OFF phase after the tap.
- `pwm_phases` (:5029-5047): newest first, ONE entry per ON phase: `age_s` (seconds since it started), `len_s`, `cmd_on_s`, `end`.
  There is no OFF-length field: OFF_s = age_s(older phase) - len_s(older phase) - age_s(newer phase), both from ONE response.
- telnet SATTICK (mode=2) and MQTT `sat/pwm_duty` as a cross-check.

## Aligning the tap with the end of an ON phase (open-loop replay)

The unit's ON phase does not start at a fixed second of the replay. Tap window of period k: replay start + k*1200 + 192...252 s. An ON phase
that began at age_s ago ends 264 s after its start; the case of interest is a tap in its last ~100 s. After the first ON phase read
`pwm_phases`, compute where the tap fell; if it did not fall in the last ~100 s of an ON phase, stop the replay and start it again shifted
by the difference (the SAT cycle is about 1200 s, as the replay period, so the alignment then repeats). Expect 1-2 attempts per scenario.

## Pass / fail (per scenario, against the C run of the same session)

- C: note OFF_s_C and the OFF duty (both should be steady from cycle to cycle).
- A and B, tap in the last ~100 s of an ON phase: PASS when OFF_s equals OFF_s_C within about 5 s (and the OFF duty equals the C duty).
- FAIL (old behaviour): OFF_s about 30-80 s longer than C and a lower OFF duty (host numbers at duty 0.22: A +76 s, B +31 s on origin/dev before the fix).
- A tap that fell in the OFF phase or early in the ON phase does not test the claim; do not count it.

Host numbers, `python tests/test_sat_pwm_tap_replay.py` (ON starts at replay second 0): OFF pause C 936.0 s; A 936.0 s with the fix, 1011.9 s on
origin/dev before it; B 936.0 s with the fix, 975.4 s on the first fix commit e7a9f4cc2 (before the post-draw-wait fix), 966.7 s on origin/dev.
Known limit (not covered, not a test target): a tap that no control tick and no flame edge sees.
