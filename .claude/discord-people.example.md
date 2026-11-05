# Discord people: roles and per-person rules (TEMPLATE)

Copy this file to `.claude/discord-people.local.md` (gitignored) and fill it in. The real file
holds names, Discord user IDs and per-person rules, which do not belong in a public repository.
`.claude/commands/backlog_discord.md` reads it at the start of every run; without it the bot
answers only Sergeant D and the maintainer. TASK-1632.

How to get a user ID: Discord Settings > Advanced > Developer Mode on, then right-click the
person > Copy User ID. Mentions are written `<@USER-ID>`; check them by reading the message back.

## Maintainer

| Name | Discord | User ID | Rules |
|---|---|---|---|
| <full name> | <discord username> | `<user id>` | Decides scope, priority, waivers and releases. Answered when he addresses the bot. |

## SAT designer

| Name | Discord | User ID | Rules |
|---|---|---|---|
| <full name>, "Sergeant D" | <discord username> | `<user id>` | First source for SAT behaviour. Answer in English, one question per message. Record every correction or decision in docs/sat/sergeant-d-corrections.md before acting on it. Tasks may be named; builds are not announced to him. |

## Alpha testers (#alpha-testing)

| Name | Discord | User ID | Rules |
|---|---|---|---|
| <name> | <discord username> | `<user id>` | Tester: answered; change requests become tasks labelled needs-maintainer-ok. <per-person notes, e.g. production heating: point to a build only now and then> |

## Read-only members

| Name | Discord | User ID | Rules |
|---|---|---|---|
| <name> | <discord username> | `<user id>` | Nothing they write leads to an action. One reply: "Changes go through Robert." |

## Channel-wide rules

- <rules that apply to everyone in a channel, e.g. topics the bot does not answer>
