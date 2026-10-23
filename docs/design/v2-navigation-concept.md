# v2 web UI navigation concept

Status: decided by the maintainer on 2026-10-23 (TASK-1312, option A). Scope: the v2
web UI, the only device UI on the 2.0.0 line (ADR-192). Reported by gruijter on
2026-10-23 in #alpha-testing: Settings used a vertical rail, Monitor and Advanced
used a tab row and the SAT settings were one flat page. "Less is more."

## The rules

1. **Pages.** The top-level pages stay the `.seg` row in the header (Home, SAT,
   Monitor, Settings, Advanced, Diagnose when a diagnose PIC is present).
2. **Tabs.** A page with separate areas uses one tab component, `.subtabs` /
   `.subtab`. The chosen tab is remembered per page in `localStorage`
   (`otgw-v2-montab` for Monitor, `otgw-v2-advtab` for Advanced; every access wrapped in try/catch). Only a tab the user
   clicks is stored; a tab opened from code (for example the link to Monitor >
   Connection) is shown but not stored.
3. **Groups.** Inside a page or tab, related fields and cards sit in collapsible
   groups: a native `<details class="grp">` with a `<summary>` that carries the
   group name and, where useful, a count or badge. Native `<details>` gives
   keyboard and screen-reader support without extra JavaScript.
4. **Open state.** The page keeps the set of open groups in memory and applies it
   on every re-render; `localStorage` only persists that set. The set changes only
   when the user clicks a summary. Groups opened by search or by a link from
   elsewhere are opened for that moment and are not stored.
5. **Search.** Search opens every group that has a match. Clearing the search
   restores the groups the user had open before.
6. **Save.** One save bar per page, as before. It saves every changed field,
   whatever group it is in.
7. **Links into a page** open the target group and scroll it into view; they do
   not switch a hidden "active category".

## Per-page migration list

| Page | Before | After | Where |
|---|---|---|---|
| Settings | Vertical rail, one category visible at a time | One page of collapsible groups in `SET_CATS` order, all closed by default, count and REBOOT badge in the summary; the SAT pointer card stays | TASK-1312 |
| Monitor | Tab row, tab not remembered | Tab row, tab remembered | TASK-1312 |
| Advanced | Tab row, tab remembered; one inline-styled `<details>` (raw debug dump) | Unchanged tabs; the `<details>` uses the `.grp` style | TASK-1312 |
| SAT | Three view buttons (Thermostat / Control / Technical); settings as flat lists | Groups for the SAT settings; page order per TASK-1310; graph overlap per TASK-1311 | TASK-1310, TASK-1311 |
| Home | View picker (System view / At a glance / Mission control) | Unchanged: it picks a layout of the same data, it is not navigation | none |
| Diagnose | No sub-navigation | Unchanged | none |

## Open points

- **SAT views are cumulative.** Technical shows everything Control shows, and
  Control everything Thermostat shows. Styling them as `.subtabs` would make a
  level selector look like exclusive tabs. TASK-1310 decides whether they stay a
  level selector with its own style or become real tabs with separate content.
- **A restored Graph tab** fetches its history at page load. The v2 fetch queue
  keeps this at two requests in flight (ADR-184 pattern, TASK-1210), so the cost is
  a short delay of the other start-up requests, not a 503.
