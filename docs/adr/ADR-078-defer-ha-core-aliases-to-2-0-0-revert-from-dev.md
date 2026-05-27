# ADR-078: Defer HA-Core Capability-Flag Aliases — Ship on 2.0.0 Only, Revert from `dev`

## Status

Accepted, 2026-05-21.

## Context

ADR-077 (accepted earlier on 2026-05-21) added 37 HA-core-style alias MQTT topics under a default-off settings toggle (`settings.mqtt.bPublishHaCoreAliases`). The implementation shipped in `dev` commit `8050df3d` (1.6.0-beta.12). Within hours of acceptance the maintainer reconsidered the *shape* of the feature for the next major release:

1. **Single source of truth.** Publishing both the legacy OT-spec-derived names AND the new self-describing names simultaneously (with an opt-in toggle for the new set) doubles the discovery footprint and per-update MQTT chatter for affected bits, and creates two parallel entity sets in HA's registry. The maintainer's preferred model is mutually exclusive: either legacy names OR new names, never both.

2. **Breaking-change appetite.** The 1.x.x maintenance line is conservative; renaming/replacing established topic labels is breaking and inappropriate for a maintenance line that exists to ship bug fixes and small improvements without churn. The 2.0.0 feature line is already a breaking-change release (ESP32, SAT, three-device HA discovery split, new OTGW32 hardware support); making the new self-describing names the default on 2.0.0 *and* dropping the legacy names by default is consistent with the 2.0.0 scope.

3. **Migration support without dual-publish.** A `settings.mqtt.bUseLegacyOtTopics` toggle on 2.0.0 lets users keep their existing automations by flipping back to legacy mode. This gives the migration path the maintainer wants without paying the dual-publish cost ADR-077 designed for.

The redesign and the reasoning behind it land in **ADR-106 on the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch**, which supersedes the 2.0.0-side ADR-105.

### Why not ship both shapes (dev keeps dual-publish, 2.0.0 ships exclusive)?

- Two parallel mental models for "what does the alias setting do" across releases would confuse users moving between branches.
- The 2.0.0 design (exclusive new-default + legacy-toggle) is strictly more conservative on the wire than ADR-077's dual-publish — you publish 37 fewer topics by default, and the user explicitly opts into the legacy set when they need migration continuity.
- The dev firmware ships under a 1.5.x→1.6.x maintenance window; users on dev who want HA-core-style names can upgrade to 2.0.0 once it stabilises (the 2.0.0 feature window is explicitly the right place for naming-contract changes).
- Keeping dev at the pre-ADR-077 contract means dev users see no surprise entities, no opt-in toggle confusion, and no "feature-flag that does subtly different things in 1.x vs 2.x" foot-gun.

### Constraints

- ADR-077 is `Accepted, 2026-05-21.` and per project policy (CLAUDE.md > ADR lifecycle) its content is immutable. The only permitted edit is the Status line, which this ADR updates to `Superseded by ADR-078, 2026-05-21.`
- The implementation commit `8050df3d` (and its 24-file prerelease-bump cascade) needs a clean revert so dev returns to the exact pre-ADR-077 behaviour. Field testers on the `dev` beta channel must not see beta.12's 37 alias entries lingering — that would create a confusing intermediate state.
- The 37 ADR-077 PROGMEM strings + 37 struct rows + flag bit + settings field + publisher modifications all need to come back out. Reverting via `git revert <commit>` produces the exact diff inversion.
- Field-test cleanup: any deployment that already ran beta.12 will have published 37 retained discovery configs for the aliases. They need a one-shot manual purge (the user purges with `mosquitto_pub -t '<haPrefix>/binary_sensor/<gw>/<label>/config' -n -r` for each alias topic). Documented in the beta release notes; not handled in firmware.

## Decision

1. **Revert** the ADR-077 implementation on `dev` via `git revert 8050df3d --no-edit` (single revert commit). The revert restores the pre-implementation state of:
   - `src/OTGW-firmware/OTGW-firmware.h` — drops `bPublishHaCoreAliases` field.
   - `src/OTGW-firmware/MQTTstuff.h` — drops `MQTT_HA_FLAG_IS_HA_CORE_ALIAS = 0x10` flag bit + `MQTT_HA_BINSENSOR_INDEXED_COUNT` constant.
   - `src/OTGW-firmware/mqtt_configuratie.cpp` — drops 37 alias PROGMEM label/name strings + 37 struct rows; restores `MQTT_HA_BINSENSOR_COUNT = 53`.
   - `src/OTGW-firmware/MQTTstuff.ino` — drops the alias-tail discovery iteration.
   - `src/OTGW-firmware/OTGW-Core.ino` — drops the `aliasLabel` parameter on `publishStatusBitMQTT` / `publishStatusVHBitMQTT` / `publishGatedBitMQTT`, drops the alias publishes at MsgID 2/3/74/101 direct-publish sites.
   - `src/OTGW-firmware/settingStuff.ino` — drops JSON serializer/parser rows for `MQTTpublishHaCoreAliases` and the debug-show row.
   - `src/OTGW-firmware/handleDebug.ino` — drops the debug echo.
   - `src/OTGW-firmware/restAPI.ino` — drops the `mqttpublishhacorealiases` setting object and the `settings.mqtt.ha_aliases` REST entry.
   - ADR-077 Status flip from `Accepted` back to `Proposed` (the revert undoes the status edit that 8050df3d also contained).

2. **Re-flip ADR-077 Status** to `Superseded by ADR-078, 2026-05-21.` This is the only edit this ADR makes to ADR-077's text. All other ADR-077 content (Context, Decision, Alternatives, Consequences, Related, References, Enforcement) stays exactly as written — it remains the historical record of what was almost shipped on `dev`.

3. **Bump dev prerelease** past the revert. The revert commit + Status-flip commit + this ADR all touch `src/OTGW-firmware/**` (the revert) and `docs/adr/**` (this ADR + ADR-077 status), so per project policy the firmware bump applies. New prerelease: `beta.13`. Field testers see `1.6.0-beta.13` as the version that drops the alias feature; the beta.12 → beta.13 step in the changelog explains why.

4. **No new ADR for the 2.0.0 design here.** The 2.0.0 design lives in ADR-106 on the 2.0.0 worktree (cross-branch reference only; this ADR does not pre-decide 2.0.0).

5. **Document the cleanup path for beta.12 testers.** Add a note in the dev `CHANGELOG.md` / release notes pointing testers at the manual `mosquitto_pub -n -r` purge for the 37 alias topics. (This is operational documentation, not firmware code.)

### Why this choice

- **Cleanest possible revert.** `git revert` produces an exact inversion of the implementation commit. No surgery, no mistakes — the only way to err is if the revert produces conflicts, which it doesn't because nothing else on dev has touched those files since 8050df3d.
- **Preserves ADR-077's historical content.** Per the immutability rule, the decision record stays — anyone reading ADR-077 in six months can see what was tried, what the categorisation analysis was, what the audit found. The Status line directs them to ADR-078 for the resolution.
- **Honest beta channel.** Field testers running beta.12 already saw the 37 alias entries. Reverting to beta.13 with a documented cleanup is honest about what changed. Pretending nothing happened (e.g. carrying the implementation forward as "deprecated" but published) would leave a confusing live feature.
- **No code-rot on dev.** The 37 PROGMEM strings + struct rows + flag bit don't sit in `dev` as dead code waiting for a future "maybe we'll ship it" decision. If 2.0.0's design proves successful and a backport request emerges later, a new ADR will own that decision explicitly.

## Alternatives Considered

### Alternative 1 — Leave the implementation in place, only flip ADRs to "deprecated"

Keep beta.12's alias code shipping on dev (default-off, harmless). Document in ADR-078 that the design is being reworked on 2.0.0 and dev's implementation is "deprecated but operational".

**Rejected:** the user explicitly chose "Full revert + ADR-078 supersession" when presented this option. The reasoning is sound: dev shouldn't carry a half-life feature whose successor will live on 2.0.0 with different semantics. A user who reads about HA-core aliases in the 2.0.0 release notes and tries to enable them on dev would get the *old* dual-publish behaviour, which contradicts the 2.0.0 design. Cleaner to revert.

### Alternative 2 — Soft-flip: change dev's default semantics to match 2.0.0 (new names default, legacy via toggle) without supersession

Implement the 2.0.0 design on dev too, with a breaking-change beta. Avoids the divergence between branches.

**Rejected:** dev is a maintenance branch, not a breaking-change vehicle. Inflicting a topic-naming switch on dev users with no major-version bump would violate the 1.x.x conservatism contract. Users on dev for the bug-fix stability deserve naming stability too.

### Alternative 3 — Multi-commit revert (one revert per logical piece)

Revert in chunks: first revert the publishers, then the settings, then the discovery rows. Easier to bisect if some piece needs to come back later.

**Rejected:** the ADR-077 implementation is a coherent feature with a single justification — there's no scenario where partial revert is desirable. A single revert commit produces a one-line audit trail and a deterministic state. If a future ADR re-introduces *some* of the changes (e.g. just the flag bit, just the settings field), it can do so explicitly without inheriting bits from a stale partial revert.

## Consequences

### Positive

- Dev returns to the exact pre-ADR-077 contract. No new MQTT topics, no new settings field, no new flag bit, no behavioural surprises.
- ADR-077 remains as historical record explaining the original design space and why it was reconsidered. Future maintainers reviewing the topic-naming question have full context (ADR-077 → ADR-078 → ADR-106).
- The 2.0.0 design (ADR-106) inherits the bit-level mapping work from ADR-077's audit without competing implementation in flight on two branches.

### Negative

- Beta.12 field testers who pulled the alias feature lose it on beta.13. Documented in release notes with the manual cleanup recipe.
- ADR-077's enforcement block (`llm_judge: true`) is now pointed at a feature that won't ship — Sonnet will still review commits but won't find violations because there's no implementation. Once ADR-077 is `Superseded`, the judge can demote it; however the current `adr-judge` script doesn't yet ignore superseded ADRs in its llm_judge pass. Acceptable tooling friction; if it becomes noisy, a follow-up issue can teach the judge to skip superseded ADRs in llm mode.
- Two ADR numbers (077 + 078) get spent on what is effectively one decision lifecycle. The supersession path keeps the audit trail clean, so this is the right cost.

### Risk

The revert touches the same 30 files that the implementation touched; the build was verified green after the revert. No behavioural surface remains. The only risk is if a downstream commit between 8050df3d and the revert had touched those files — there is none (`git log dev 8050df3d..HEAD --oneline` shows only the revert itself).

## Related Decisions

- **ADR-077** — original HA-core alias design on dev. Superseded by this ADR; content immutable, Status line updated to point here.
- **ADR-105** (2.0.0 branch) — sibling of ADR-077 on 2.0.0. Itself superseded by ADR-106 (the new exclusive design).
- **ADR-106** (2.0.0 branch, future) — new design: new self-describing topic names as default, `bUseLegacyOtTopics` toggle for migration. Breaking change for 2.0.0; not backported to dev per this ADR's Decision step 4.
- **TASK-648** — v2.1.0 three-device split. The 2.0.0 topic-naming decision (per ADR-106) will be the foundation TASK-648 builds on.

## References

- `src/OTGW-firmware/` — restored to pre-`8050df3d` state by the revert commit `bdaf82e9`.
- `docs/adr/ADR-077-mqtt-ha-core-capability-flag-aliases.md` — historical record of the original design; Status now `Superseded by ADR-078, 2026-05-21.`
- Commit `bdaf82e9` — the revert (single commit, 30 files, –285 / +80 net inversion of `8050df3d`).
- Commit `8050df3d` (now reverted) — the implementation. Permanent in git history but not in the source tree.

## Enforcement

```json
{
  "forbid_pattern": [
    { "pattern": "MQTT_HA_FLAG_IS_HA_CORE_ALIAS", "path_glob": "src/OTGW-firmware/**", "message": "ADR-078: HA-core alias flag bit is reverted on dev. The naming design lives on 2.0.0 (ADR-106). Do not re-introduce on dev." },
    { "pattern": "bPublishHaCoreAliases", "path_glob": "src/OTGW-firmware/**", "message": "ADR-078: bPublishHaCoreAliases settings field is reverted on dev. Use ADR-106's bUseLegacyOtTopics on 2.0.0 if you mean the new naming-mode toggle." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```

Declarative-only enforcement is sufficient here: any commit that re-introduces either identifier on dev is almost certainly an accidental backport from 2.0.0 and should be blocked.
