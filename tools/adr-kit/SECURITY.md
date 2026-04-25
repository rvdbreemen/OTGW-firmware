# Security policy

## Scope

`adr-kit` is a documentation toolkit. It does not handle secrets, network requests to external services, or sensitive user data on its own. Its scope is markdown files, plugin manifests, and slash-command instructions for AI coding agents.

The most realistic security concern is that the `setup` skill writes to a project's `CLAUDE.md` and the `adr-generator` agent writes new files under `docs/adr/`. These are constrained writes inside the user's own project directory. No external network calls.

## Reporting a vulnerability

If you discover a security-relevant issue, please:

1. **Do not** open a public GitHub issue with exploit details.
2. Email the maintainer at <robert@vandenbreemen.net> with the subject line `adr-kit security: <short summary>`.
3. Include reproduction steps, the affected version (`adr-kit--vX.Y.Z`), and your proposed fix if you have one.

You will get an acknowledgement within 7 days. A fix and a coordinated disclosure timeline will follow.

If you are not sure whether something qualifies, email anyway. We would rather hear about a non-issue than miss a real one.

## Supported versions

Only the latest minor release line is supported with security fixes. Older versions get a fix only if the maintainer judges the impact severe enough.

| Version | Status |
|---|---|
| `v0.6.x` (latest) | Supported. |
| `v0.5.x` and earlier | No security backports. |

## Acknowledgement

Reporters who follow this policy will be credited in the release notes of the fix release, unless they prefer to remain anonymous.
