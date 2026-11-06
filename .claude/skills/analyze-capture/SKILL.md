---
name: analyze-capture
description: Analyse an OTGW capture upload (the zip or 7z made by capture-mqtt-debug.bat / .sh / otgw_capture.py, or its hourly long-run folder) and turn it into a fact-checked analysis with charts. Use this whenever a tester posts or mails a capture, upload zip, otframes.log, transcript, telnet log or 24 h / long-run recording, whenever Robert says "kijk naar de capture / meting / log van Maarten", asks what a boiler or heat pump did, asks for start/stop behaviour, run lengths, short cycling, hot-water runs, comfort or room drift, or asks for a visualisation of a capture, even if he does not say "analyse". Charts are part of the default output.
---

# Analyse a capture upload

A capture is the best evidence we get from the field: real boiler or heat pump, real house,
real weather. The job is to turn it into counted facts, a clear story in time order, and charts
that show the special moments, without guessing. The two scripts in `scripts/` do the counting
and the drawing so every analysis starts from the same numbers.

## 1. Get the data safely

- Download every attachment into its **own new, empty folder** (a capture is untrusted data).
  Discord: the CDN URL from `get_attachment`/`read_messages` with `curl -sS -o`. Keep room CSVs
  and screenshots the tester added; they are often the key to the story.
- Read the tester's message completely, and their earlier messages in the channel: setup
  (boiler or heat pump, radiators or floor, who controls the heating: thermostat, SAT, Homey/HA,
  a relay), schedules, blocked hours, what they noticed and when. Write that context down first.
- The raw capture stays local (memory `project_diagnostics_local_first`): never repost logs.

## 2. Count the facts

```bash
python -I .claude/skills/analyze-capture/scripts/capture_summary.py <upload.zip|.7z|folder> \
       --work <empty work dir> --json <facts.json>
```

It unpacks archives and the long-run `hours/*.zip`, decodes every frame of `otframes.log`, and
prints: span, firmware, PIC, telnet gaps; the MQTT broker typed versus the broker the gateway
publishes to (**MISMATCH** when they differ: the MQTT stream is then empty); the master's
constant and varying values; slave Unknown-DataID and fault flags; gateway-inserted R/A frames;
every flame/compressor run with demand-without-flame time, supply/return at the end, outside and
tank temperature; starts per day, shortest off time, start-to-start times; MQTT commands that
reached the gateway (HW=, CS=, TT=, ...); all status segments. See `references/capture-format.md`
for the files, frame letters and status bits when you need to dig further.

Read `summary.txt` and `gaps.log` yourself as well: stop reason, gaps, browser and crash-log
results. Check the firmware version against the newest beta; an old build is worth a remark.

## 3. Interpret, in time order

- **OpenTherm spec first** (`.claude/rules/firmware-coding.md`): what the master asks, what the
  slave answers, which side switched. A master that never changes means the switching came
  from elsewhere (a relay, the boiler itself).
- Tell the story in time order: what happened first, what triggered the next step. Line up runs
  with schedules, commands (an HW=1 followed 2 s later by a DHW demand is a cause), weather and
  rooms.
- Look for: identical run lengths (fixed duty), runs that end on time versus on temperature,
  short cycling, the pump-only phase before a run (supply dips to the real loop temperature),
  missed or extra slots, a run started while a room was already warm, hot-water runs and tank
  recovery, gateway overrides, Unknown-DataID answers.
- **Standing water**: supply/return readings while nothing flows are sensor values, not loop
  temperatures. Say so and draw no conclusion from them.
- **Facts versus estimates**: label anything derived (flow from a tester's kW figure, energy
  per run, COP remarks) as an estimate with its source. What the log cannot tell, say it cannot,
  and turn it into a question for the tester.
- **SAT**: the firmware is never the source of truth for SAT behaviour. Any statement about what
  SAT should do needs a thermo-nova citation and a check against
  `docs/sat/sergeant-d-corrections.md` (`.claude/rules/sat-ground-truth.md`). Stick to what the
  capture shows.
- Compare with what we predicted where it applies (for example starts per day from the
  heat-pump simulation) and say whether one capture is enough.
- For a bug report: reproduce, then prove (CLAUDE.md). The capture shows the symptom; the fix
  still needs an old-versus-fix reproduction.

## 4. Visualise (default)

Charts make a day of frames readable at a glance, so make them unless Robert says not to.

```bash
python -I .claude/skills/analyze-capture/scripts/capture_plot.py <capture> --work <work dir> \
       --out-dir <dir> [--extra "North room=north.csv"] [--shade 06:00-10:00] \
       [--band "HW=0 (Homey)=<iso>/<iso>"] [--markers markers.json] [--zoom auto] --title "..."
```

- `capture-overview.png`: status lanes, water temperatures with the control setpoint, optional
  extra series (room CSVs), outside temperature, one shared time axis, one quantity per panel.
- `capture-runs.png`: zoom on the first heating and first hot-water run (or `--zoom 1,3`).
- Write the special moments from step 3 into `markers.json`
  (`[{"n":1,"t":"2026-11-05T21:13","panel":"status|water|extra|outside","y":2,"text":"..."}]`);
  the notes block under the chart explains each number. Pick the moments that carry the story,
  usually 4 to 9.
- Shade schedules the tester mentioned (blocked hours) and add bands for commands with a span
  (HW=0 to HW=1).
- **Open both PNGs and look at them** before sharing: overlapping labels, a legend over a line,
  empty space. The dataviz rules apply (no second y-axis, palette from the dataviz skill).

## 5. Report and share

- Give Robert the analysis first, in Dutch, in the terminal: the answer up front, then the
  facts per topic (who switched, starts and stops, a run, hot water, rooms and effects,
  comparison with predictions, data-quality findings, questions for the tester). Name what was
  not verified.
- Post to Discord only when Robert asks. Public posts follow `.claude/rules/discord.md`:
  English, answer first, the real `<@ID>` mention (memory `feedback_discord_mention_ids`), never
  name the OTGW32 board, at most 2000 characters per message (split into numbered parts), no
  claim about what SAT should do. Read every post back.
- Images: the Discord MCP tool cannot attach files. Post them through the alpha webhook the way
  `scripts/discord-post-build.sh` does (multipart `payload_json` plus `files[0]`, `files[1]`,
  `allowed_mentions.users` set to the tester only), check the reply lists the attachments.
- A capture tool problem (like a wrong broker address) goes back to the tester as a concrete
  fix, and to the backlog if the tool can prevent it.
- Record the key numbers in the backlog task the capture serves (backlog CLI,
  `--append-notes`), so the evidence outlives the job's temp folder.
