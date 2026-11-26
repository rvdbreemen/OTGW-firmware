---
id: "ADR-299"
title: "Remote telemetry is opt-in and asked: off by default, a No in the dialog is final, a switch-off in Settings is asked again only after a new release"
status: "Proposed"
date: "2026-11-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "remote-telemetry"
  - "consent"
  - "opt-in"
  - "privacy"
aliases:
  - "telemetry opt-in"
  - "ask logic"
components:
  - "remote telemetry consent rule"
symbols: []
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-299 Remote telemetry is opt-in and asked: off by default, a No in the dialog is final, a switch-off in Settings is asked again only after a new release

## Status

Proposed, 2026-11-26.

## Status History

```yaml
status_history:
  - date: 2026-11-26
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

Terms used in names and paths below: OTGW (OpenTherm Gateway), NVS (non-volatile storage, the ESP32 key-value
store), PSRAM (pseudo-static RAM, random-access memory), REST (Representational State Transfer), UI (user
interface), TASK (a backlog task id), API (application programming interface), DM
(Discord direct message), EN (English), NL (Dutch), DE
(German), EL (Greek), KB (kilobytes), POST (HTTP write request), GET (HTTP read request), HTTP (Hypertext
Transfer Protocol).

Remote telemetry sends a diagnostic recording of the gateway to a collection point of the firmware maker over
outbound TLS (Transport Layer Security, ADR-218). Robert decided on 2026-11-26 that it ships in 2.0.0 as an opt-in
that the user is *asked*, not one that is hidden in Settings.

Why this needs its own record:

- The opt-in used to be part of ADR-290 (item 3, "Off unless `/cloudrec.cfg` says `enabled=1`"). ADR-290 was marked
  Superseded by ADR-218 on 2026-11-20, but ADR-218 covers only the TLS client. Since then no Accepted or Proposed ADR
  states the consent contract. *read*: `docs/adr/ADR-290-opt-in-cloud-telemetry-from-the-gateway.md:93`,
  `docs/adr/ADR-218-*.md` (no opt-in rule).
- The rule changes a REST contract (a new `remote_telemetry.ask` field in `/api/v2/health`, a `source` marker on the
  settings POST), adds NVS state, and it is a privacy promise to users. the project instructions ask for an ADR on an interface contract change.

Evidence labels: *read* = read in a file or a message; *planned* = in the TASK-3092 plan, not yet built; *not
verified* = still to be proven.

Robert's decisions (each checked by otgw-architect through the Discord API where a DM exists):

1. 2026-11-26 about 01:45, interactive, relayed by the coordinator (no DM id; source:
   `OTGW-field-data/_comms/telemetry_optin_text.md`, header): what is sent is not changed, the text is made honest; a
   "No" in the prompt is final; the server sentence names Cloudflare.
2. 2026-11-26, via the coordinator, recorded in TASK-3092's description: ask when the setting was never set (also on
   existing installs, in a standalone dialog, not only in the wizard), or when the user switched it off in Settings
   and the firmware was flashed since; never on a board without PSRAM.
3. 2026-11-26 06:05 local, Discord DM 1575376983641301104, verbatim "B", a reply to question 4
   (DM 1575365781804294299: "hoe vaak opnieuw vragen na handmatig uitzetten?"), where B read "alleen na een nieuwe
   release (beta of stable), dus niet bij elke ontwikkelbuild".
4. 2026-11-26 06:04 local, Discord DM 1575376575451504731, verbatim "A", a reply to question 3
   (DM 1575365777526100210): the opt-in text is approved as drafted; NL and DE follow.

## Decision

Remote telemetry is off until the user says yes, and the gateway asks the user in a dialog, at most at these
moments. The device decides whether to ask; the web UI only shows the dialog.

The decision is one pure function of the stored state (*planned*, TASK-3092, in `RemoteTelemetryPolicy.h`, host
compilable):

```text
ask = psram && ( optin == never-set
              || ( optin == off && offsrc == settings && offrel != current release ) )
```

| State | Ask? |
|---|---|
| never set (first start, or an existing install that never answered) | yes |
| on | no |
| off by a "No" in the dialog (`offsrc` = prompt) | no, never again |
| off in Settings, same release | no |
| off in Settings, a new release since (`offrel` differs) | yes, once |
| board without PSRAM | no, never |

"Release" is `_SEMVER_CORE` + "-" + `_VERSION_PRERELEASE` (for example `2.0.0-beta.433`; a stable is `2.0.0`), never
the githash or the build counter, so a developer build does not ask again (Robert, decision 3).

Stored state, NVS namespace `telemetry` (*planned*): `optin` (u8: never set / off / on, exists today), `offsrc` (u8:
none / prompt / settings), `offrel` (string, at most 23 characters). Turning it on clears `offsrc` and `offrel`. A
"No" in the dialog sets `offsrc` = prompt. A switch-off in Settings sets `offsrc` = settings and `offrel` = the
running release.

REST (*planned*): `/api/v2/health` carries `remote_telemetry.ask` (bool, computed on the device). The existing
`remotetelemetry` key of `POST /api/v2/settings` takes an optional `"source": "prompt" | "settings"` (missing or any
other value = settings; it matters only for a switch-off). No new settings key.

On a board without PSRAM the firmware ignores a write that turns remote telemetry on, so REST cannot create an "on
but not running" state; the web UI disables the row there.

The setting stays visible and switchable under Settings > System > Remote telemetry; a switch-off takes effect at
once. What is sent is not changed by this ADR (decision 1).

## Decision Contract

### Must

- Keep remote telemetry off unless the user turned it on, in the dialog or in Settings.
- Compute `ask` on the device from the stored state with the one function above, and expose it as
  `remote_telemetry.ask` in `/api/v2/health`.
- Treat a "No" in the dialog as final: no dialog after it, through any release, until the user turns it on.
- Ask again after a switch-off in Settings only once a new release runs, compared by the release string, not the
  build.
- Never ask, and ignore an "on" write, on a board without PSRAM.
- Show the dialog standalone in the web UI, also on existing installs, not only in the setup wizard, with the text
  Robert approved (decision 4) and its NL, DE and EL translations.

### Must Not

- Turn remote telemetry on by default, by an update, or by a restore of settings that never held a yes from this
  user.
- Ask again because of a developer build, a githash change or a reboot.
- Change what is sent in the same change as the consent rule (decision 1).

### Exceptions

- None.

### Verification

Guideline-level per ADR-080, with a test gate once TASK-3092 is built:

- A host table test that compiles the real `rtel::askNow()` (never set, on, off by prompt, off in Settings with the
  same release, off in Settings after a new release, no PSRAM), red on code without it, with mutants (offsrc
  ignored, release compare removed, PSRAM ignored) killed (*planned*).
- A REST round-trip test: `health` has `ask`, the settings POST reads `source`, the "on" write is ignored without
  PSRAM (*planned*).
- Bench on .63 by otgw-bench-tester, with a flash between two releases: the dialog appears after a switch-off in
  Settings and a new release, and not after a "No" in the dialog (TASK-3092 AC5, *not verified*).

## Alternatives Considered

- **Opt-in only in Settings, never asked.** Simplest, and nothing can nag. Rejected by Robert's decision 2: hardly
  anyone finds the setting, so the feature would see almost no field data.
- **Ask after every flash (every build).** Literal reading of "after a flash". Rejected by decision 3 (option A of
  question 4): testers with many builds would be asked again and again.
- **Ask again after a "No" in the dialog, later.** More coverage, but it breaks the promise that a "No" is final
  (decision 1).
- **Decide in the web UI instead of on the device.** Fewer firmware changes, but every UI (v2, wizard, a future
  client) would need the same rule and the NVS state is on the device. Rejected for one device-side function with a
  host test.

## Consequences

**Positive:**

- One rule, one function and one test table for when the gateway asks; the UI cannot drift from it.
- A "No" holds; a manual switch-off is asked again at most once per release.

**Negative:**

- Three new NVS fields (about 24 B, against 20 KB of NVS) in the existing NVS partition (*read*, otgw-poc-dev).
- An existing install sees the dialog once after the update that brings it, because its state is "never set".
- The rule depends on `_VERSION_PRERELEASE` naming releases; a release with the same string as the one in
  `offrel` does not ask again.

## Open Questions

- [ ] 1. Robert: a gateway that the collection point blocks turns its own opt-in off (ADR-290 item 3,
  `RemoteTelemetryPolicy.h` `OUT_BLOCKED`). Should that count as a "No" (never ask again, recommended: the maintainer
  blocked it on purpose) or as a Settings switch-off (ask after the next release)?
- [ ] 2. Robert: acceptance of this ADR.

## Related Decisions

- **ADR-218 (outbound TLS on boards with PSRAM)**: the transport this feature uses; it does not state the consent
  rule.
- **ADR-290 (opt-in cloud telemetry, Superseded)**: held the earlier opt-in (`/cloudrec.cfg`); this ADR states the
  consent contract again for the shipped feature.
- **ADR-263 (remote telemetry buffers in PSRAM)** and **ADR-268 (the remote telemetry recorder's local ring)**: other
  parts of the same feature.
- **ADR-080 (binding rules need a gate)**: this ADR is guideline-level with a host test gate once built.

## References

- TASK-3092 (plan, notes: offrel replaces offat after Robert's question 4 answer; source key; PSRAM write rule).
- `OTGW-field-data/_comms/telemetry_optin_text.md` (approved text and the behaviour list).
- Discord DMs 1575365777526100210, 1575376575451504731, 1575365781804294299, 1575376983641301104 (questions 3 and 4
  and Robert's answers, reply references checked through the Discord API).
- `src/OTGW-firmware/RemoteTelemetryPolicy.h` on lane-poc-merge bbd3eb109 (today: backoff, reply classes, telemetry
  ID; the ask function is planned there).
- European Data Protection Board, Guidelines 05/2020 on consent (consent must be an affirmative, informed act and as
  easy to withdraw as to give): https://www.edpb.europa.eu/our-work-tools/our-documents/guidelines/guidelines-052020-consent-under-regulation-2016679_en
