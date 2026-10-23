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
3. **Groups.** (Settings is the exception: it keeps its rail of categories with card
   panels, see the table below.) Inside a page or tab, related fields and cards sit in collapsible
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
7. **Links into a page** open the target group and scroll it into view. On
   Settings (rail) the link selects the category and scrolls to the target card.

## Per-page migration list

| Page | Before | After | Where |
|---|---|---|---|
| Settings | Vertical rail, one category visible at a time | **Exception to rule 3, by maintainer decision 2026-10-23 (TASK-1326):** TASK-1312 turned it into collapsible groups; Robert (number3nl) asked for his original design back, which is optimized for desktop and mobile. Settings keeps the rail of categories (a horizontally scrolling row below 900 px) with the selected category's card panels, one category at a time, the search across all categories, and one save bar. Links into Settings (BLE Assign, the sensor roster) and a refused IP save select the category and scroll to the target, also when the settings GET is slow | TASK-1312, TASK-1326 |
| Monitor | Tab row, tab not remembered | Tab row, tab remembered | TASK-1312 |
| Advanced | Tab row, tab remembered; one inline-styled `<details>` (raw debug dump) | Unchanged tabs; the `<details>` uses the `.grp` style | TASK-1312 |
| SAT | Three view buttons (Thermostat / Control / Technical); settings as flat lists; Hot water and Burner cycle mode cards in the Control level | The view buttons stay a cumulative level selector. Thermostat: dial and presets, status, temperature history, then a collapsed `grp` "Hot water" (open state in `otgw-v2-satopen`, stored on a summary click only). Burner cycle mode moves to the Control level and up. Groups for the SAT settings; graph overlap per TASK-1311 | TASK-1310, TASK-1311 |
| Home | View picker (System view / At a glance / Mission control) | Unchanged: it picks a layout of the same data, it is not navigation | none |
| Diagnose | No sub-navigation | Unchanged | none |

## Open points

- **SAT views are cumulative** (resolved in TASK-1310). They stay a level selector:
  Technical shows everything Control shows, and Control everything Thermostat shows.
  They keep their own style (`.satview-btn`, with a small level hint) and are not
  `.subtabs`, because exclusive-looking tabs would mislead. Inside a level, edge-case
  controls sit in `grp` groups (Hot water on the Thermostat view).
- **A restored Graph tab** fetches its history at page load. The v2 fetch queue
  keeps this at two requests in flight (ADR-184 pattern, TASK-1210), so the cost is
  a short delay of the other start-up requests, not a 503.

- **SAT chart and Monitor graph (TASK-1311, decided).** The SAT chart stays as 'SAT control loop' (SAT target, PID output, flow, room, outside since the page opened) with a link to Monitor > Graph; merging would make Monitor poll SAT data and the SAT values are not in the stored history (ADR-193).
