export const meta = {
  name: 'implement-next-task',
  description: 'Continuously drain the actionable async-esp32s3 backlog (epic TASK-865): audit stuck/finishable In Progress + In Review tasks, then per task implement -> build/eval -> adversarial review -> commit/push -> Discord, advancing to the next immediately until none remain or a transient failure ends the run. ADRs are NOT authored per task; one end-of-loop ADR-Evaluation pass reviews every task landed this run, dedups the architectural decisions, and drafts the Proposed ADRs (maintainer accepts them later). Parameterizable per worktree/branch for parallel lanes.',
  phases: [
    { title: 'Audit', detail: 'reconcile stuck/finishable In Progress + In Review tasks; capture run start HEAD' },
    { title: 'Select', detail: 'next actionable task (deps Done/In Review)' },
    { title: 'Implement', detail: 'coding agent, build + evaluator gated' },
    { title: 'Review', detail: 'adversarial check vs ACs + binding ADRs' },
    { title: 'Land', detail: 'status + (bump) + commit + push (no ADR in the per-task commit)' },
    { title: 'Announce', detail: 'alpha-channel Discord report' },
    { title: 'ADR Evaluation', detail: 'end-of-run: enumerate architectural decisions across ALL landed tasks, JS-number them, draft Proposed ADRs, guard, one docs commit, announce' },
  ],
}

const A = (typeof args === 'object' && args) ? args : {}
// Branch model changed 2026-06-20: the 2.0.0 async line was promoted to `dev`
// (the former feature-2.0.0-esp32s3-async). dev IS the default working line now;
// the old async worktree is gone. otgw-1.x.x carries 1.x maintenance.
const REPO = A.repo || 'D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware'
const BRANCH = A.branch || 'dev'
const SKIP_BUMP = !!A.skipBump              // parallel lanes defer the prerelease bump to serial integration
// ADR-Evaluation defers to serial integration on parallel lanes so two lanes never
// race on ADR numbers. Defaults to skipBump (lanes set skipBump:true and inherit it);
// pass skipAdrEval explicitly to override either way.
const SKIP_ADR_EVAL = (typeof A.skipAdrEval === 'boolean') ? A.skipAdrEval : SKIP_BUMP
const TARGET = (A.targetTask || '').trim()  // pin a specific task for a parallel lane; '' = auto-pick lowest
const MAX_TASKS = A.maxTasks || 25          // safety cap on tasks drained per run

const RULES =
  `Project rules (CLAUDE.md, binding):\n` +
  `- Run ALL commands in the worktree ${REPO} (branch ${BRANCH}). Never touch dev or feature-dev-2.0.0-otgw32-esp32-sat-support.\n` +
  `- PROGMEM: all string literals via F()/PSTR()/snprintf_P; strcmp_P/strcasecmp_P; memcmp_P for binary.\n` +
  `- No ArduinoJson. No String in hot paths (ADR-004). char[] + strlcpy/snprintf_P.\n` +
  `- No raw #if(def) ESP8266/ESP32/ARDUINO_ARCH_ESP*/BOARD_NODOSHOP_ESP* outside the allowlisted abstraction files (platform*.h, boards.h, OTGW-ModUpdateServer*). Add a platformXxx() shim or HAS_* flag FIRST, then call it unguarded.\n` +
  `- Vendored src/libraries/SimpleTelnet/** and src/libraries/OTGWSerial/OTGWSerial.cpp are OUT OF SCOPE unless the task says otherwise.\n` +
  `- PIC commands via addOTWGcmdtoqueue(); never raw Serial writes. feedWatchDog() in long loops.\n` +
  `- Use the backlog CLI for task status; never edit backlog/tasks/*.md directly.\n` +
  `- Architecture references (read-only): other-projects/EMS-ESP32-dev (async+FreeRTOS+espMqttClient blueprint), other-projects/OT-Thing-OTGW32. Never modify other-projects/.\n` +
  `- build.py masks per-env failures: ALWAYS grep the build log for the per-env SUCCESS line; do not trust exit 0 alone.`

// --- shared cleanup for transient agent deaths (rate limits) ----------------
const cleanup = async (taskId, why) => {
  log(`CLEANUP ${taskId}: ${why} -> revert partial edits + reset To Do (so a later run retries clean).`)
  await agent(
    `A workflow iteration for ${taskId} aborted mid-run (${why}). In ${REPO}: run \`git -C ${REPO} checkout -- src/ backlog/tasks/\` to revert partial tracked edits; remove partial NEW untracked source files this attempt created under src/OTGW-firmware or src/libraries (find \`??\` via \`git -C ${REPO} status --porcelain\`; do NOT remove other-projects/ or the copied OpenTherm/SimpleTelnet lib dirs); then \`backlog task edit ${taskId} -s "To Do" -a @claude\`. Confirm src/ and backlog/tasks/ are clean.`,
    { label: `cleanup:${taskId}`, phase: 'Audit' }
  )
}

// ============================ AUDIT (once per run) ==========================
phase('Audit')

// Capture the run's start commit BEFORE the audit runs. The audit + every Land
// status edit auto-commit in the dev tree (backlog CLI auto-commits there), so a
// `completed[i].commit^` range would be fragile. `startHead..HEAD` is the only
// stable range for the end-of-loop ADR-Evaluation pass. (TASK-928, advisor trap 2.)
const HEAD = { type: 'object', required: ['head'], properties: { head: { type: 'string', description: 'output of git rev-parse HEAD' } } }
const startCap = await agent(
  `In ${REPO}: run \`git -C ${REPO} rev-parse HEAD\` and return ONLY the resulting 40-char commit hash as {head}. Do not change anything.`,
  { label: 'capture-start-head', phase: 'Audit', schema: HEAD }
)
const startHead = (startCap && startCap.head ? startCap.head : '').trim()
if (startHead) log(`Run start HEAD: ${startHead}`)
else log(`WARNING: could not capture start HEAD; the ADR-Evaluation pass will fall back to reviewing each landed task's commit individually.`)

const AUDIT = {
  type: 'object',
  required: ['summary', 'movedToDone', 'resetToToDo', 'adrOrphans'],
  properties: {
    summary: { type: 'string' },
    movedToDone: { type: 'array', items: { type: 'string' }, description: 'task ids advanced to Done this audit' },
    resetToToDo: { type: 'array', items: { type: 'string' }, description: 'stuck/abandoned task ids reset to To Do this audit' },
    adrOrphans: { type: 'array', items: { type: 'string' }, description: 'task ids whose notes carry ADR-PENDING but NOT ADR-EVALUATED -> a prior run landed them but its end-of-loop ADR pass died before drafting their ADR; the end-of-loop pass this run must sweep them up' },
  },
}
const audit = await agent(
  `Audit the async ESP32-S3 migration backlog (label async-esp32s3, parent TASK-865) in ${REPO}.\n${RULES}\n\n` +
  `List backlog/tasks/task-865.*.md and read each status + notes via \`backlog task <id> --plain\`. For every task that is "In Progress" or "In Review", decide:\n` +
  `- STUCK (CLEAN worktree ONLY): status In Progress AND \`git -C ${REPO} status\` shows NO uncommitted src/ edits -> a lane that was working it died leaving no partial work (or never started). Reset it: \`backlog task edit <id> -s "To Do" -a @claude\`. CRITICAL: if there ARE uncommitted src/ edits for an In Progress task, a lane is ACTIVELY working it right now (or a died lane's edits await the launching session's manual cleanup) -> LEAVE IT UNTOUCHED. NEVER \`git checkout\`/discard or reset a task whose worktree is dirty; you would clobber a running lane.\n` +
  `- FINISHABLE: status In Progress/In Review where EVERY remaining acceptance criterion is build-verifiable or evaluator-verifiable (NO hardware/field-validation AC left) AND the work is committed + green. Move it to Done: \`backlog task edit <id> -s "Done"\`. Get this right by reading the ACs — do NOT move a task to Done while a field-validation / hardware-soak AC is open (those stay In Review).\n` +
  `- CORRECT: In Review purely because a field-validation / hardware AC remains -> leave it (cannot self-certify hardware).\n` +
  `ALSO — ADR-ORPHAN SWEEP (do not change anything, just detect): for every task-865.* read its notes; collect the ids whose notes contain the token "ADR-PENDING" but NOT "ADR-EVALUATED". At THIS point (run start) such a task can only come from a PRIOR run whose end-of-loop ADR pass died before drafting its ADR. Return them as adrOrphans. (Empty list if none.)\n` +
  `Return a one-line summary, the ids you moved to Done, the ids you reset to To Do, and adrOrphans. Make the status changes; do not ask permission.`,
  { label: 'audit', phase: 'Audit', schema: AUDIT }
)
if (audit) log(`Audit: ${audit.summary}${audit.movedToDone && audit.movedToDone.length ? ' | Done: ' + audit.movedToDone.join(',') : ''}${audit.resetToToDo && audit.resetToToDo.length ? ' | reset: ' + audit.resetToToDo.join(',') : ''}`)

// ============================ CONTINUOUS DRAIN LOOP =========================
const SEL = {
  type: 'object',
  required: ['none', 'taskId', 'title', 'body', 'buildTargets', 'srcTouched', 'fieldValidationRemains', 'alsoActionable'],
  properties: {
    none: { type: 'boolean' },
    taskId: { type: 'string' },
    title: { type: 'string' },
    body: { type: 'string', description: 'full task description markdown' },
    buildTargets: { type: 'array', items: { type: 'string' } },
    srcTouched: { type: 'boolean' },
    fieldValidationRemains: { type: 'boolean' },
    alsoActionable: { type: 'array', items: { type: 'string' }, description: 'other task ids currently actionable + file-disjoint from this one (candidates for a parallel worktree lane)' },
  },
}
const IMPL = {
  type: 'object',
  required: ['summary', 'filesChanged', 'buildPassed', 'evalPassed', 'acsMetBuildEval', 'blockers'],
  properties: {
    summary: { type: 'string' }, filesChanged: { type: 'array', items: { type: 'string' } },
    buildPassed: { type: 'boolean' }, evalPassed: { type: 'boolean' },
    acsMetBuildEval: { type: 'string' }, blockers: { type: 'string' },
  },
}
const REV = { type: 'object', required: ['pass', 'issues'], properties: { pass: { type: 'boolean' }, issues: { type: 'string' } } }
const LAND = {
  type: 'object', required: ['committed', 'commitHash', 'pushed', 'newStatus', 'prereleaseTag', 'featureSummary', 'note'],
  properties: {
    committed: { type: 'boolean' }, commitHash: { type: 'string' }, pushed: { type: 'boolean' },
    newStatus: { type: 'string', description: 'In Review or Done' },
    prereleaseTag: { type: 'string', description: 'e.g. alpha.182 if bumped, else empty' },
    featureSummary: { type: 'string' }, note: { type: 'string' },
  },
}

const selPrompt =
  `Select the next actionable async ESP32-S3 migration task in ${REPO}.\n${RULES}\n\n` +
  (TARGET
    ? `PINNED LANE: select EXACTLY ${TARGET}. Verify it is "To Do" and every dependency is "Done"/"In Review"; set none=false and return it. Do NOT auto-pick another task and do NOT apply a single-flight guard (this is an intentional parallel lane).\n`
    : `List backlog/tasks/task-865.*.md (label async-esp32s3, parent TASK-865); read status + dependencies via \`backlog task <id> --plain\`. SINGLE-FLIGHT GUARD (this worktree only): if ANY task-865.* is already "In Progress", another lane owns this worktree -> return none=true. Otherwise pick the LOWEST dotted seq (865.1<865.2<...<865.13) that is "To Do" AND every dependency is "Done"/"In Review". Skip the epic + anything In Review/Done.\n`) +
  `READ-ONLY: do NOT run \`backlog task edit\` or change ANY task's status here — only the Implement phase claims a task by setting it In Progress. Just read and report.\n` +
  `Return none + the task's id/title/full Description body/build targets/srcTouched(touches src/**)/fieldValidationRemains(any hardware AC)/alsoActionable (other To-Do tasks whose deps are met AND whose files are disjoint from this one).`
const implPrompt = (sel, extra) =>
  `Implement this backlog task end-to-end in ${REPO} (branch ${BRANCH}).\n${RULES}\n\n` +
  `TASK ${sel.taskId}: ${sel.title}\n\n${sel.body}\n\n` +
  `Steps: (1) \`backlog task edit ${sel.taskId} -s "In Progress" -a @claude\`. (2) Implement per the ## Acceptance Criteria, re-grepping the cited anchors first (they may have drifted). (3) Build \`python build.py --target <each of ${JSON.stringify(sel.buildTargets)}>\` and GREP the log for the per-env SUCCESS line. (4) \`python evaluate.py --quick\` (no NEW failures). (5) Do NOT commit/bump/change status (Land owns that), and do NOT author any ADR (the end-of-loop ADR-Evaluation pass owns that). Satisfy only build/evaluator ACs; field-validation ACs are out of scope (no hardware).\n` +
  (extra || '') + `Return what changed, build+eval pass, ACs met with evidence, blockers.`
const revPrompt = (sel) =>
  `Adversarially review the working-tree diff in ${REPO} for TASK ${sel.taskId}.\n${RULES}\n\nTASK body (contract):\n${sel.body}\n\n` +
  `\`git -C ${REPO} diff\`/\`status\`. Check: (a) every build/evaluator AC actually met (re-run build/evaluate if unsure; grep the SUCCESS line); (b) no binding ADR/CLAUDE rule violated; (c) scope discipline (only this task's files; no stray edits). Default pass=false if any build/evaluator AC is unproven. Be specific.`
const landPrompt = (sel) =>
  `Land the reviewed work for TASK ${sel.taskId} in ${REPO} (branch ${BRANCH}).\n${RULES}\n\nsrcTouched=${sel.srcTouched}; fieldValidationRemains=${sel.fieldValidationRemains}; skipBump=${SKIP_BUMP}.\n` +
  `NOTE: ADRs are NOT authored or committed here. The end-of-loop ADR-Evaluation pass drafts all Proposed ADRs for the run. Do NOT create, stage, or commit any docs/adr/*.md file in this step.\n` +
  `Steps (ORDER MATTERS — status must be INSIDE the commit so a checkout cannot revert it):\n` +
  `1. Set final status FIRST: if fieldValidationRemains "In Review", else "Done". \`backlog task edit ${sel.taskId} -s "<status>" --append-notes "<what landed + remaining field-validation ACs>. ADR-PENDING (end-of-loop ADR-Evaluation drafts any ADR and stamps ADR-EVALUATED)."\`. The ADR-PENDING token is mandatory — it is the persistent marker the end-of-loop pass (and the NEXT run's Audit, if this run dies) uses to know this task still needs ADR evaluation.\n` +
  (SKIP_BUMP
    ? `2. Do NOT bump the prerelease (parallel lane; the bump+tag happens at serial integration when this sub-branch merges). prereleaseTag="".\n`
    : `2. If srcTouched, run \`bin/bump-prerelease.sh\` (stages version.h + banners); record the new tag (e.g. alpha.182) as prereleaseTag. Else prereleaseTag="".\n`) +
  `3. Stage ONLY this task's changed paths via \`git add <explicit paths>\` (NEVER \`git add -A\`): its source/doc files, the bump files (if any), AND the task's own backlog/tasks/*.md (now carrying the new status). Do NOT stage any docs/adr/*.md, sibling task notes, or other in-flight work.\n` +
  `4. Commit subject \`<type>(<scope>): <imperative> (${sel.taskId})\`. No em dashes. Co-author trailer. \`git push origin ${BRANCH}\` with retry.\n` +
  `5. featureSummary: 1-2 plain sentences on the user-facing feature/improvement (for #alpha-testing). Empty if nothing committed.\n` +
  `Return committed/commitHash/pushed/newStatus/prereleaseTag/featureSummary/note.`
const announcePrompt = (sel, land) =>
  `Report TASK ${sel.taskId} to Discord per the alpha-channel policy (memory feedback_discord_alpha_channel). Use mcp__discord-mcp__send_message (params: channelId, message). English, facts only.\n` +
  `1. Post a one-line completion to #dev-sat-mqtt (channel_id 1105556725714649128): "✅ ${sel.taskId} (${land.newStatus}) — <one-line what landed>. Commit ${land.commitHash}${land.prereleaseTag ? ' (' + land.prereleaseTag + ')' : ''} on ${BRANCH}." plus the hardware gate if In Review.\n` +
  (land.prereleaseTag
    ? `2. SEMVER STEP: also post a short feature note to #alpha-testing (channel_id 1514720723980259460): "🔧 2.0.0-${land.prereleaseTag} (dev milestone) — ${land.featureSummary}". If it returns "Missing Access", note that the bot lacks access and continue (do NOT retry).\n`
    : `2. No prerelease bump -> no #alpha-testing post.\n`) +
  `Never #beta-testing. Return a one-line confirmation of what was posted.`

const completed = []
let endReason = 'drained (no actionable task left)'
for (let n = 0; n < MAX_TASKS; n++) {
  phase('Select')
  const sel = await agent(selPrompt, { label: `select#${n + 1}`, phase: 'Select', schema: SEL })
  if (!sel) { endReason = 'select agent died (transient)'; break }
  if (sel.none || !sel.taskId) { endReason = 'drained (no actionable task left)'; break }
  log(`[${n + 1}] ${sel.taskId}: ${sel.title}${sel.alsoActionable && sel.alsoActionable.length ? '  (also actionable, parallelizable: ' + sel.alsoActionable.join(',') + ')' : ''}`)

  phase('Implement')
  let impl = await agent(implPrompt(sel), { label: `impl:${sel.taskId}`, phase: 'Implement', schema: IMPL })
  if (!impl) { await cleanup(sel.taskId, 'implement agent died (transient)'); endReason = `transient abort on ${sel.taskId}`; break }

  phase('Review')
  let rev = await agent(revPrompt(sel), { label: `review:${sel.taskId}`, phase: 'Review', schema: REV })
  if (!rev) { endReason = `review agent died AFTER a successful impl on ${sel.taskId} — IMPL PRESERVED (worktree left dirty, task left In Progress); needs manual review+land, do NOT discard.`; break }
  if (!rev.pass) {
    log(`Review issues on ${sel.taskId} — one fix pass: ${rev.issues}`)
    impl = await agent(implPrompt(sel, `A prior review FAILED — fix these specifically:\n${rev.issues}\n`), { label: `fix:${sel.taskId}`, phase: 'Implement', schema: IMPL })
    if (!impl) { await cleanup(sel.taskId, 'fix agent died (transient)'); endReason = `transient abort on ${sel.taskId}`; break }
    rev = await agent(revPrompt(sel), { label: `re-review:${sel.taskId}`, phase: 'Review', schema: REV })
    if (!rev) { endReason = `re-review agent died AFTER a fix on ${sel.taskId} — IMPL PRESERVED for manual review+land, do NOT discard.`; break }
    if (!rev.pass) { await cleanup(sel.taskId, `review failed twice: ${rev.issues}`); endReason = `${sel.taskId} failed review twice (needs attention): ${rev.issues}`; break }
  }

  phase('Land')
  const land = await agent(landPrompt(sel), { label: `land:${sel.taskId}`, phase: 'Land', schema: LAND })
  if (!land) { await cleanup(sel.taskId, 'land agent died (transient)'); endReason = `transient abort on ${sel.taskId}`; break }
  if (!land.committed) { endReason = `land did not commit ${sel.taskId}: ${land.note}`; break }

  phase('Announce')
  const ann = await agent(announcePrompt(sel, land), { label: `announce:${sel.taskId}`, phase: 'Announce' })
  log(`Announced ${sel.taskId}: ${ann || 'posted'}`)

  completed.push({ task: sel.taskId, title: sel.title, body: sel.body, status: land.newStatus, commit: land.commitHash, tag: land.prereleaseTag, srcTouched: sel.srcTouched })
  log(`Landed ${sel.taskId} -> ${land.newStatus}${land.tag ? ' (' + land.tag + ')' : ''}. Advancing immediately to the next task.`)
}

// ===================== END-OF-LOOP ADR EVALUATION ===========================
// ADRs are deferred out of the per-task path (maintainer directive 2026-06-24,
// TASK-928): one pass at the END of the drain reviews EVERY task landed this run,
// dedups the architectural decisions across tasks, and drafts the Proposed ADRs.
// This fires on every loop exit (normal drain or `break`) because it sits after
// the for-loop guarded by completed.length — it always ADRs the tasks that DID
// land this run. Acceptance stays the maintainer's manual checkpoint; the
// self-accept governance guard (the loop's original-sin fix, bug-036's cousin)
// travels here intact and runs over every newly drafted ADR.
const ENUM = {
  type: 'object', required: ['decisions', 'maxAdrNumber', 'summary'],
  properties: {
    decisions: {
      type: 'array',
      items: {
        type: 'object', required: ['title', 'kebab', 'rationale', 'taskIds', 'supersedes'],
        properties: {
          title: { type: 'string' }, kebab: { type: 'string', description: 'kebab-case slug for the filename (no ADR number, no .md)' },
          rationale: { type: 'string', description: 'why this is a genuine architectural decision needing an ADR' },
          taskIds: { type: 'array', items: { type: 'string' }, description: 'every task this run whose diff contributed to this decision' },
          supersedes: { type: 'string', description: 'ADR-NNN it supersedes/amends, or empty' },
        },
      },
    },
    maxAdrNumber: { type: 'integer', description: 'highest existing ADR number under docs/adr/ (so the JS can assign the next free numbers)' },
    summary: { type: 'string' },
  },
}
const ADRFILE = { type: 'object', required: ['files'], properties: { files: { type: 'array', items: { type: 'string' } }, note: { type: 'string' } } }
const ADRGUARD = {
  type: 'object', required: ['changed', 'note'],
  properties: { changed: { type: 'array', items: { type: 'string' }, description: 'files where Accepted->Proposed or a premature back-reference revert was applied; empty if all already Proposed' }, note: { type: 'string' } },
}
const ADRLAND = { type: 'object', required: ['committed', 'commitHash', 'pushed', 'note'], properties: { committed: { type: 'boolean' }, commitHash: { type: 'string' }, pushed: { type: 'boolean' }, note: { type: 'string' } } }

let adrsDrafted = []
// Process-death recovery (TASK-928): every Land stamps the task note "ADR-PENDING";
// the end-of-loop pass stamps "ADR-EVALUATED" once it has confirmed-drafted (or
// confirmed there was nothing to draft). If this run's eval dies before stamping,
// the marker survives in git and the NEXT run's Audit returns the task in adrOrphans
// — so this pass sweeps up orphans from prior dead runs alongside its own tasks.
const orphans = (audit && audit.adrOrphans && audit.adrOrphans.length) ? audit.adrOrphans : []
const haveAdrWork = completed.length > 0 || orphans.length > 0
if (haveAdrWork && SKIP_ADR_EVAL) {
  log(`ADR Evaluation SKIPPED (parallel lane; defer to serial integration). ${completed.length} landed + ${orphans.length} orphan(s) stay ADR-PENDING for the integrating run to draft.`)
} else if (haveAdrWork) {
  phase('ADR Evaluation')
  const coveredTaskIds = Array.from(new Set([...completed.map(c => c.task), ...orphans]))
  const thisRunList = completed.length ? completed.map(c => `${c.task} (${c.title}) -> commit ${c.commit}`).join('\n') : '(none landed this run)'
  const orphanList = orphans.length ? orphans.join(', ') : '(none)'
  const range = startHead ? `${startHead}..HEAD` : `(no start HEAD captured; inspect each task's landing commit individually via git log --grep)`
  let evalSucceeded = false

  // 1. ENUMERATE — one agent reads the whole-run diff + task bodies, returns a
  //    dedup'd decision list mapped to task ids. Writes NO files. Returns the current
  //    max ADR number so the JS (not an LLM) assigns the next free numbers.
  const ev = await agent(
    `End-of-loop ADR evaluation for the async ESP32-S3 drain run in ${REPO}.\n${RULES}\n\n` +
    `Tasks LANDED THIS run (diff them as a batch via \`git -C ${REPO} diff ${range}\`):\n${thisRunList}\n\n` +
    `ORPHAN tasks from a PRIOR run whose ADR pass died (still marked ADR-PENDING) — evaluate these TOO; find each one's landing commit with \`git -C ${REPO} log --oneline --grep "(<taskId>)"\` and inspect it via \`git show <hash>\`: ${orphanList}\n\n` +
    `For full intent read each task body via \`backlog task <id> --plain\`. Identify EVERY distinct ARCHITECTURAL decision across ALL these tasks — a new pattern/dependency, an API/MQTT/topic contract change, a concurrency/threading model change, or a supersession of an existing ADR. DEDUP: one decision several tasks made is ONE entry (list all contributing task ids). Routine bug fixes / refactors within an existing pattern are NOT decisions — omit them.\n` +
    `IDEMPOTENCY (a prior run may have drafted some ADRs before dying): for EACH candidate decision, FIRST grep docs/adr/*.md for an ADR that already documents it — search the contributing task ids in ADR References sections, or the decision topic. If an ADR already covers it, SKIP it (do NOT duplicate). Only return decisions that have NO existing ADR.\n` +
    `If nothing remains, return decisions=[]. Do NOT write any file. Also return maxAdrNumber = the highest existing ADR number (glob docs/adr/ADR-*.md). For each decision return: title, a kebab-case filename slug (no number/no .md), a one-line rationale, the contributing taskIds, and supersedes (ADR-NNN or empty).`,
    { label: 'adr-enumerate', phase: 'ADR Evaluation', schema: ENUM }
  )

  if (!ev) {
    // Enumerate agent died: do NOT stamp ADR-EVALUATED. Markers stay ADR-PENDING so
    // the next run's Audit re-flags these as orphans and retries the whole pass.
    log(`ADR Evaluation: enumerate agent unavailable (transient) — tasks stay ADR-PENDING; next run will retry. Covered ids: ${coveredTaskIds.join(', ')}`)
  } else if (!ev.decisions || !ev.decisions.length) {
    // Nothing architectural (or all already ADR'd): evaluation IS complete.
    log(`ADR Evaluation: ${ev.summary || 'no new architectural decision'} — no Proposed ADRs needed this run.`)
    evalSucceeded = true
  } else {
    // 2. JS assigns the numbers deterministically (kills the "next free number"
    //    collision at the root — advisor KISS steer).
    let next = (typeof ev.maxAdrNumber === 'number' && ev.maxAdrNumber > 0 ? ev.maxAdrNumber : 0) + 1
    const assigned = ev.decisions.map(d => ({ ...d, num: next++ }))
    log(`ADR Evaluation: ${assigned.length} decision(s) -> drafting ADR-${assigned.map(a => a.num).join(', ADR-')} (Proposed).`)

    // 3. AUTHOR each ADR at its pre-assigned number (sequential: tiny N, keeps the
    //    shared docs/adr/README.md index race-free). Authors write ONLY their own ADR
    //    file; the README index + commit happen once, in Land.
    const created = []
    for (const d of assigned) {
      const taskRefs = d.taskIds.join(', ')
      const thisRunCommits = completed.filter(c => d.taskIds.includes(c.task)).map(c => `${c.task}=${c.commit}`).join(', ')
      const commits = thisRunCommits || `(orphan task(s); find each landing commit via \`git -C ${REPO} log --oneline --grep "(<taskId>)"\`)`
      const a = await agent(
        `Author ONE Architecture Decision Record per adr-kit for a decision made by the async drain run in ${REPO}.\n${RULES}\n\n` +
        `Write the file docs/adr/ADR-${d.num}-${d.kebab}.md. Use EXACTLY ADR number ${d.num} (already assigned — do NOT pick your own number, do NOT glob for the next free one).\n` +
        `Decision: ${d.title}\nRationale: ${d.rationale}\nContributing tasks: ${taskRefs}\n${d.supersedes ? `Supersedes/amends: ${d.supersedes}\n` : ''}` +
        `Read the relevant diff for context: \`git -C ${REPO} diff ${range}\` and the task bodies (\`backlog task <id> --plain\`).\n` +
        `CRITICAL — Status MUST be "Proposed" (NEVER "Accepted"; acceptance is the maintainer's manual checkpoint). The adr-kit:adr-generator template DEFAULTS to "Accepted" for already-implemented decisions (agents/adr-generator.md:52,174) — you MUST OVERRIDE that: emit Status "Proposed" and a SINGLE status_history entry (status: Proposed, changed_by: Agent, changed_via: adr-kit). NEVER fabricate a maintainer/manual acceptance entry.\n` +
        `If this supersedes/amends an EXISTING Accepted ADR, write it as a NEW ADR and do NOT touch the old ADR's body/Status (no "Superseded by"/"Amended by" back-reference while this one is still Proposed).\n` +
        `Include Context / Decision / Alternatives(>=2) / Consequences / Related. The References section MUST list the contributing tasks with their landed commit hashes: ${commits}. (This replaces the per-task atomicity lost by deferring the ADR out of the code commit.)\n` +
        `Do NOT edit docs/adr/README.md and do NOT git add/commit — Land stages the index + commits all ADRs together. Return the file path(s) you wrote.`,
        { label: `adr-author:ADR-${d.num}`, phase: 'ADR Evaluation', schema: ADRFILE, agentType: 'adr-kit:adr-generator' }
      )
      if (a && a.files && a.files.length) created.push(...a.files)
    }

    if (!created.length) {
      log(`ADR Evaluation: ${assigned.length} decision(s) enumerated but NO ADR file was authored (author agents unavailable). Tasks stay ADR-PENDING; next run retries.`)
    } else {
      // 4. GOVERNANCE GUARD (loop self-accept fix, 2026-06-15 / relocated 2026-06-24).
      //    The adr-kit:adr-generator DEFAULTS to "Accepted" and pre-fills
      //    `status: Accepted` (agents/adr-generator.md:52,174). A prompt does not
      //    reliably beat that default, so this DETERMINISTIC post-step forces every
      //    drafted/amended ADR back to Proposed regardless of what the generator did.
      const guard = await agent(
        `MECHANICAL GOVERNANCE GUARD in ${REPO}. NO judgment, NO content authoring, ONLY enforce ADR status. For EACH ADR file in ${JSON.stringify(created)}:\n` +
        `1. In its \`## Status\` section: if the status is "Accepted" (or anything other than Proposed / "Superseded by ADR-*" / "Amended by ADR-*"), REWRITE it to "Proposed, <the date already in the file>." Leave a genuine Superseded-by/Amended-by status of a DIFFERENT, older ADR alone.\n` +
        `2. In \`## Status History\`: DELETE every entry whose \`status:\` is \`Accepted\`, and any fabricated maintainer entry (e.g. \`changed_via: manual\` with a human \`changed_by\`). Keep exactly the Proposed entry (status: Proposed, changed_by: Agent, changed_via: adr-kit); if none exists, write one with today's date.\n` +
        `3. If a new ADR supersedes or amends an EXISTING Accepted ADR and the generator already wrote a "Superseded by ADR-NNN"/"Amended by ADR-NNN" back-reference onto that OLD ADR (or flipped its Status), REVERT the old ADR: \`git -C ${REPO} diff <oldAdrFile>\` then restore it. Back-references are applied ONLY on real human acceptance, never while the new ADR is Proposed.\n` +
        `4. Fix docs/adr/README.md: for each new ADR row, if it says "Accepted", set it to "Proposed".\n` +
        `Do not reword or improve any other content. Report each file changed and what you changed (empty list if all were already correctly Proposed). The loop NEVER accepts an ADR; acceptance is the maintainer's manual checkpoint.`,
        { label: 'adr-guard', phase: 'ADR Evaluation', schema: ADRGUARD }
      )
      if (guard && guard.changed && guard.changed.length) log(`ADR self-accept guard forced Proposed: ${guard.changed.join('; ')}`)

      // 5. LAND the ADRs — one docs commit, citing TASK-865 (the epic file is
      //    tracked, so commit-msg pass 1 + pass 2 are satisfied; docs-only = no bump).
      const adrLand = await agent(
        `Land the end-of-loop Proposed ADRs in ${REPO} (branch ${BRANCH}). These document architectural decisions from this drain run: ${assigned.map(a => `ADR-${a.num} (${a.title})`).join('; ')}.\n${RULES}\n\n` +
        `1. Ensure docs/adr/README.md has a Proposed row for each new ADR (${created.join(', ')}); add any missing rows.\n` +
        `2. Stage ONLY: the new ADR files ${JSON.stringify(created)} and docs/adr/README.md. NEVER \`git add -A\`. Do NOT stage src/ or any task file.\n` +
        `3. Commit (docs-only, NO prerelease bump) with subject \`docs(adr): draft Proposed ADRs for drain run (TASK-865)\`. The TASK-865 reference satisfies the commit-msg hook (epic file is tracked); no [no-task] needed. No em dashes. Co-author trailer.\n` +
        `4. \`git push origin ${BRANCH}\` with retry.\n` +
        `Return committed/commitHash/pushed/note.`,
        { label: 'adr-land', phase: 'ADR Evaluation', schema: ADRLAND }
      )
      adrsDrafted = assigned.map(a => ({ adr: `ADR-${a.num}`, title: a.title, file: created.find(f => f.includes(`ADR-${a.num}-`)) || '', taskIds: a.taskIds }))
      if (adrLand && adrLand.committed) {
        evalSucceeded = true
        log(`ADR Evaluation landed ${created.length} Proposed ADR(s): ${adrsDrafted.map(a => a.adr).join(', ')} — commit ${adrLand.commitHash}. Awaiting maintainer acceptance.`)
        // 6. ANNOUNCE the drafted ADRs to #dev-sat-mqtt for the maintainer to review/accept.
        await agent(
          `Post ONE maintainer-facing note to Discord #dev-sat-mqtt (channelId 1105556725714649128) via mcp__discord-mcp__send_message (params: channelId, message). English, facts only. NEVER #beta-testing.\n` +
          `Message: "📋 ${created.length} Proposed ADR(s) drafted this drain run for review: ${adrsDrafted.map(a => a.adr + ' (' + a.title + ')').join(', ')}. Commit ${adrLand.commitHash} on ${BRANCH}. Acceptance is the maintainer's call." Return a one-line confirmation.`,
          { label: 'adr-announce', phase: 'ADR Evaluation' }
        )
      } else {
        // ADRs authored but the commit did not land: leave tasks ADR-PENDING so the
        // next run retries. The idempotency grep in enumerate skips any ADR that DID
        // land, so a partial failure does not double-draft.
        log(`ADR Evaluation: drafted ${created.length} ADR(s) but the docs commit did NOT land: ${adrLand ? adrLand.note : 'land agent unavailable'}. Tasks stay ADR-PENDING; next run retries (idempotent dedup prevents duplicates).`)
      }
    }
  }

  // 7. STAMP ADR-EVALUATED on every covered task — but ONLY on confirmed completion
  //    (evalSucceeded). On any transient failure above we deliberately skip this so
  //    the ADR-PENDING marker survives and the next run's Audit re-sweeps the task.
  if (evalSucceeded && coveredTaskIds.length) {
    const coverMap = JSON.stringify(adrsDrafted.map(a => ({ adr: a.adr, taskIds: a.taskIds })))
    await agent(
      `MECHANICAL marker step in ${REPO}. For EACH task id [${coveredTaskIds.join(', ')}], append a backlog note so the next run's Audit does not re-flag it as an ADR orphan: \`backlog task edit <id> --append-notes "ADR-EVALUATED: <comma-list of ADR-NNN that cover this task, or 'none'>"\`. ADR coverage map (task -> ADRs): ${coverMap}. A task NOT in that map gets 'none'. Do not change status or anything else. Report how many tasks you marked.`,
      { label: 'adr-mark-evaluated', phase: 'ADR Evaluation' }
    )
    log(`ADR Evaluation: stamped ADR-EVALUATED on ${coveredTaskIds.length} task(s) — they will not be re-swept.`)
  }
}

return { done: true, completed, count: completed.length, endReason, adrsDrafted, adrOrphansSwept: orphans }
