# Pull request

## Summary

[1 to 3 sentences. What does this PR change, and why?]

## Type of change

- [ ] Bug fix (non-breaking, restores documented behaviour)
- [ ] New feature (non-breaking, additive)
- [ ] Breaking change (fix or feature that changes a default or contract; explain migration below)
- [ ] Documentation only
- [ ] Tooling, CI, or repo hygiene

## Verification gates (the four named gates from SKILL.md)

For any change that materially affects user behaviour, confirm these:

- [ ] **Completeness**: the change is described in full (problem, decision, alternatives, consequences). If a new ADR-relevant pattern is introduced, an ADR is added or updated.
- [ ] **Evidence**: claims are backed by measurements, references, or code (no vague "improves performance").
- [ ] **Clarity**: a reviewer who has not seen this codebase can follow the change.
- [ ] **Consistency**: no conflict with existing accepted ADRs, manifests, or skill conventions; no duplicated tag numbers; cross-references in place.

## Checklist

- [ ] CHANGELOG.md updated under `[Unreleased]` (or under the current release section if it has not been published yet).
- [ ] If this is a release commit: plugin.json `version` bumped, top CHANGELOG entry matches.
- [ ] CI passes locally (`jq empty .claude-plugin/plugin.json`, files exist, markdownlint clean).
- [ ] Skill, agent, or instruction files added or changed are listed in README.md where relevant.
- [ ] No em dashes anywhere in the new content; English; kebab-case file names.

## Migration (only for breaking changes)

[If this PR breaks existing user installs, describe the migration step. Otherwise: "Not applicable."]

## Related issues

[Closes #..., relates to #...]
