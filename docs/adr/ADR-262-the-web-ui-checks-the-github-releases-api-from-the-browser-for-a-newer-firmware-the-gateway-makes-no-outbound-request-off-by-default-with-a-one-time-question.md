---
id: "ADR-262"
title: "The web UI checks the GitHub releases API from the browser for a newer firmware; the gateway makes no outbound request; off by default with a one-time question"
status: "Accepted"
date: "2026-11-19"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related: []
topics:
  - "web-ui"
  - "firmware-update"
  - "privacy"
  - "settings"
aliases:
  - "update check"
  - "new version available"
  - "check GitHub for new versions"
components:
  - "web UI version check (data/v2.js updParse, updCmp, updPickNewer, maybeUpdateCheck, updAsk)"
  - "device settings updatecheck, updatecheckasked, updatecheckbeta (UISection, UI_UPDATE_CHECK_DEFAULT_MODE)"
symbols:
  - "UI_UPDATE_CHECK_DEFAULT_MODE"
  - "maybeUpdateCheck"
  - "updPickNewer"
  - "bUpdateCheck"
  - "bUpdateCheckAsked"
  - "bUpdateCheckBeta"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-262 The web UI checks the GitHub releases API from the browser for a newer firmware; the gateway makes no outbound request; off by default with a one-time question

## Status

Accepted, 2026-11-19.

**Decision Maker:** User: Robert van den Breemen (maintainer), by Discord direct message (DM) relayed by the coordinator:

- "B en neem taak C op voor een latere release 2.1.0" (DM 1572585865589428318, 2026-11-18). Option B: the gateway shows
  "new version available" with a link.
- "2" (DM 1572594516429381716, 2026-11-18). Data source: the browser asks the GitHub releases API, with no host of our own.
- Default B (DM 1572847348177047675, 2026-11-19): off, with a one-time question.

The beta/stable rule is a team decision by the coordinator (TASK-2023 notes), which Robert may overrule. Acceptance is
Robert's.

## Status History

```yaml
status_history:
  - date: 2026-11-19
    status: Proposed
    changed_by: Agent (ba)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-19
    status: Accepted
    changed_by: "Agent: Claude (ba), for User: Robert van den Breemen (maintainer)"
    reason: "Accepted by Robert, Discord DM 1572896690426486836 (2026-11-19 08:49 UTC, 'A' to the coordinator's question 'ADR-262 (TASK-2023 update check): may it go to Accepted? A) Accepted now'); shipped in beta.387 (850f555bc), TASK-2023 Done"
    changed_via: adr-kit lifecycle
```

## Context

Sergeant D asked for updates straight from GitHub instead of a manual zip upload (TASK-2023,
https://discord.com/channels/812969634638725140/1105556725714649128/1572576499674914927). The gateway firmware speaks only
HTTP (Hypertext Transfer Protocol), with no TLS (Transport Layer Security) stack (ADR-003). GitHub serves its API and release assets over HTTPS
(HTTP over TLS) only, so the gateway cannot fetch anything from GitHub itself.

The browser that shows the web UI can do this. A check on 2026-11-19 with
`curl -sI -H "Origin: http://otgw.local" "https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=1"`
gave these response headers:

- `Access-Control-Allow-Origin: *`, so a page from any origin may read the answer (CORS, cross-origin resource sharing).
- `X-RateLimit-Limit: 60`: unauthenticated callers get 60 requests per hour per IP (Internet Protocol) address.

`/releases/latest` names the newest stable release of the whole repository. That is the 1.x line (branch `main`), and
every 2.x beta has `latest=false` (`gh release list`). The answer must therefore come from the release list, filtered to
the running major version.

The web UI keeps at most 2 device API (application programming interface) requests in flight (ADR-184). A request to
another origin does not go through that queue (`isApi()` in `data/v2.js`), so it never takes a device REST
(representational state transfer) slot.

## Decision

The web UI (user interface), running in the user's browser, checks the GitHub releases list for a newer firmware and shows a notice with a
link to the release page. The gateway only stores three booleans and sends nothing to the internet. The check is off
until the user says yes to a one-time question.

- **Data.** `GET https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=30`, sent with
  `credentials: 'omit'` and `referrerPolicy: 'no-referrer'`, so no cookies and not the gateway's LAN (local area network)
  address go to GitHub.
  - Tags must match `vX.Y.Z` or `vX.Y.Z-beta.N` with the running major version; drafts are dropped.
  - The highest version wins by semantic versioning: a stable `X.Y.Z` ranks above every `X.Y.Z-beta.N`.
  - The running version comes from the existing `device/info` snapshot.
- **Beta/stable rule.** A beta (or alpha) build is told about newer betas and newer stable releases. A stable build is
  told only about newer stable releases, unless `updatecheckbeta` is on.
- **When.** Once, about 30 s after page start. The answer is cached in the browser for 6 h. A failure is cached too
  (HTTP 403 or 429 rate limit, a network error, CORS, or an answer that is not a list), so it shows no notice and does
  not retry before 6 h.
- **Display.** The notice "New version available: vX.Y.Z" sits next to the version chip. The link is built from the
  checked tag, and all text goes through `textContent`; `html_url` and `body` from the answer are not used. The × hides
  that tag until a newer one appears.
- **Settings (device, all browsers).**
  - `updatecheck`: on or off.
  - `updatecheckasked`: hidden; the one-time question was answered.
  - `updatecheckbeta`: a stable build is also told about betas.
  - The defaults come from one constant, `UI_UPDATE_CHECK_DEFAULT_MODE` in `UItypes.h`: 0 = off and silent, 1 = off with
    the one-time question (chosen), 2 = on without asking.
  - The question waits for the first-time wizard and never covers another dialog. Switching the check on in Settings
    marks the question as answered.

## Decision Contract

### Must

- Make the GitHub request from the browser only, with `credentials: 'omit'` and `referrerPolicy: 'no-referrer'`.
- Make no request to GitHub while `updatecheck` is off, and none before the user said yes (mode 1).
- Filter releases to the running major version and compare by semantic version, never by list order or
  `/releases/latest`.
- Cache answers and failures for 6 h, and fail quietly.
- Render tag and link from the validated tag with `textContent`.

### Must Not

- Put any network code in the firmware that contacts GitHub.
- Show or execute any HTML (HyperText Markup Language) or text taken from the GitHub answer other than the tag name.
- Send the GitHub request through the device API queue.

### Exceptions

- None.

### Verification

- `tests/webui/update-check-github.test.mjs` runs the real `data/v2.js` functions and the ADR-184 queue against a fetch
  stub, 13 tests:
  - the semantic version and major filter, the beta/stable rule and drafts;
  - setting off: 0 requests;
  - the one-time question, the order of its two settings writes, and its wait for the wizard;
  - one request with `credentials: 'omit'` and `no-referrer`, and queue `inFlight()` 0 during it;
  - the link and the 6 h cache;
  - dismiss;
  - 403, 429, network error and non-list answers: quiet, and no retry before 6 h.
  
  Red 0/13 on the earlier `data/v2.js`. Three mutants each turn it red: no major filter, no off switch, no referrer
  policy.
- `tests/test_update_check_settings.py` checks:
  - the struct defaults derive from the constant;
  - each key is in the settings file, in dispatch, in `GET /api/v2/settings` and in the `POST` whitelist;
  - the help texts and the hidden row;
  - no firmware source names `api.github.com`.
  
  Red 4/6 on the earlier sources.

## Alternatives Considered

- **A: a plain-HTTP mirror we host (option B, hosting 1).** The gateway could read a version file itself. It needs a host
  and a sync step per release; Robert chose hosting option 2 instead.
- **Default on (mode 2).** Users see updates without doing anything, but every browser contacts GitHub without being
  asked. Robert chose the one-time question.
- **Default off and silent (mode 0).** Fully local, but few users find the setting, so testers on old builds are not told
  about new ones.
- **A per-browser switch in localStorage.** No firmware change, but every new browser would contact GitHub before the user
  can switch it off, and the manual would have to say "per browser".
- **HTTPS self-update on the gateway (option C).** Reverses ADR-003 and costs flash; filed for 2.1.0 as its own task.

## Consequences

**Positive:**

- Users learn about new firmware without the gateway leaving the LAN; ADR-003 stays intact.
- Nothing reaches GitHub without consent, and the answer applies to every browser that opens this gateway.
- The GitHub request takes no device REST slot (ADR-184) and adds no load on the gateway.

**Negative:**

- With the check on, GitHub sees the user's internet IP address at most every 6 h per browser. The manual and the setting
  help say so, and the referrer and cookies are not sent.
- A notice can be up to 6 h late, and a failed check waits 6 h before the next try.
- Three more settings keys in the settings file and REST, and one more dialog at first use per device.
- A fork or a renamed repository needs a change to `UPD_API` and `UPD_PAGE` in `data/v2.js`.

## Open Questions

- None.

## Related Decisions

- **ADR-003 (HTTP only, no HTTPS):** kept; the browser, not the gateway, uses HTTPS.
- **ADR-184 (2 API requests in flight):** kept; the GitHub request is not a device API request.
- **ADR-213 (opt-in local diagnostic recorder):** the same opt-in, local-first line for anything that leaves the device.
- **ADR-218 (outbound TLS on boards with an external memory chip (PSRAM); Proposed, parked):** the route the 2.1.0 option C (an update straight
  from GitHub on the gateway) would need; this ADR does not depend on it.

## References

- TASK-2023 (plan and decisions in the task notes); manual text in `docs/manuals/en/ch03-web-interface.md` and
  `docs/manuals/nl/h03-webinterface.md` (TASK-2023 AC5).
- `src/OTGW-firmware/data/v2.js` (`UPD_API`, `updPickNewer`, `maybeUpdateCheck`, `updAsk`, `updShowNotice`),
  `src/OTGW-firmware/UItypes.h` (`UI_UPDATE_CHECK_DEFAULT_MODE`), `settingStuff.ino`, `restAPI.ino`, `settingsHelp.h`.
- GitHub REST API, list releases: https://docs.github.com/en/rest/releases/releases#list-releases

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "api\\.github\\.com",
      "path_glob": "src/OTGW-firmware/**/*.ino",
      "message": "The gateway makes no request to GitHub (ADR-262, ADR-003): the web UI in the browser does the version check."
    }
  ],
  "forbid_import": [],
  "require_pattern": []
}
```
