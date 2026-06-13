export const meta = {
  name: 'implement-next-task',
  description: 'Continuously drain the actionable async-esp32s3 backlog (epic TASK-865): audit stuck/finishable In Progress + In Review tasks, then per task implement -> build/eval -> adversarial review -> Proposed ADR -> commit/push -> Discord, advancing to the next immediately until none remain or a transient failure ends the run. Parameterizable per worktree/branch for parallel lanes.',
  phases: [
    { title: 'Audit', detail: 'reconcile stuck/finishable In Progress + In Review tasks' },
    { title: 'Select', detail: 'next actionable task (deps Done/In Review)' },
    { title: 'Implement', detail: 'coding agent, build + evaluator gated' },
    { title: 'Review', detail: 'adversarial check vs ACs + binding ADRs' },
    { title: 'ADR', detail: 'Proposed ADR for architectural decisions' },
    { title: 'Land', detail: 'status + (bump) + commit + push' },
    { title: 'Announce', detail: 'alpha-channel Discord report' },
  ],
}

const A = (typeof args === 'object' && args) ? args : {}
const REPO = A.repo || 'D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-esp32s3-async'
const BRANCH = A.branch || 'feature-2.0.0-esp32s3-async'
const SKIP_BUMP = !!A.skipBump              // parallel lanes defer the prerelease bump to serial integration
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
const AUDIT = {
  type: 'object',
  required: ['summary', 'movedToDone', 'resetToToDo'],
  properties: {
    summary: { type: 'string' },
    movedToDone: { type: 'array', items: { type: 'string' }, description: 'task ids advanced to Done this audit' },
    resetToToDo: { type: 'array', items: { type: 'string' }, description: 'stuck/abandoned task ids reset to To Do this audit' },
  },
}
const audit = await agent(
  `Audit the async ESP32-S3 migration backlog (label async-esp32s3, parent TASK-865) in ${REPO}.\n${RULES}\n\n` +
  `List backlog/tasks/task-865.*.md and read each status + notes via \`backlog task <id> --plain\`. For every task that is "In Progress" or "In Review", decide:\n` +
  `- STUCK (CLEAN worktree ONLY): status In Progress AND \`git -C ${REPO} status\` shows NO uncommitted src/ edits -> a lane that was working it died leaving no partial work (or never started). Reset it: \`backlog task edit <id> -s "To Do" -a @claude\`. CRITICAL: if there ARE uncommitted src/ edits for an In Progress task, a lane is ACTIVELY working it right now (or a died lane's edits await the launching session's manual cleanup) -> LEAVE IT UNTOUCHED. NEVER \`git checkout\`/discard or reset a task whose worktree is dirty; you would clobber a running lane.\n` +
  `- FINISHABLE: status In Progress/In Review where EVERY remaining acceptance criterion is build-verifiable or evaluator-verifiable (NO hardware/field-validation AC left) AND the work is committed + green. Move it to Done: \`backlog task edit <id> -s "Done"\`. Get this right by reading the ACs — do NOT move a task to Done while a field-validation / hardware-soak AC is open (those stay In Review).\n` +
  `- CORRECT: In Review purely because a field-validation / hardware AC remains -> leave it (cannot self-certify hardware).\n` +
  `Return a one-line summary plus the ids you moved to Done and reset to To Do. Make the changes; do not ask permission.`,
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
const ADR = {
  type: 'object', required: ['adrAction', 'adrFiles', 'summary'],
  properties: { adrAction: { type: 'string', enum: ['none', 'created', 'amended', 'superseding'] }, adrFiles: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
}
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
  `Steps: (1) \`backlog task edit ${sel.taskId} -s "In Progress" -a @claude\`. (2) Implement per the ## Acceptance Criteria, re-grepping the cited anchors first (they may have drifted). (3) Build \`python build.py --target <each of ${JSON.stringify(sel.buildTargets)}>\` and GREP the log for the per-env SUCCESS line. (4) \`python evaluate.py --quick\` (no NEW failures). (5) Do NOT commit/bump/change status (Land owns that). Satisfy only build/evaluator ACs; field-validation ACs are out of scope (no hardware).\n` +
  (extra || '') + `Return what changed, build+eval pass, ACs met with evidence, blockers.`
const revPrompt = (sel) =>
  `Adversarially review the working-tree diff in ${REPO} for TASK ${sel.taskId}.\n${RULES}\n\nTASK body (contract):\n${sel.body}\n\n` +
  `\`git -C ${REPO} diff\`/\`status\`. Check: (a) every build/evaluator AC actually met (re-run build/evaluate if unsure; grep the SUCCESS line); (b) no binding ADR/CLAUDE rule violated; (c) scope discipline (only this task's files; no stray edits). Default pass=false if any build/evaluator AC is unproven. Be specific.`
const adrPrompt = (sel) =>
  `Document architectural decisions in the diff for TASK ${sel.taskId} in ${REPO}, per adr-kit.\n${RULES}\n\nTASK body (may REQUIRE ADR work, e.g. supersede ADR-108/011/047/048/058):\n${sel.body}\n\n` +
  `\`git -C ${REPO} diff\`. If the change makes/alters an architectural decision (new pattern/dependency, API/MQTT/topic contract, concurrency model, supersession): author a NEW (or amend a still-Proposed) ADR at docs/adr/ADR-NNN-kebab.md, Status MUST be "Proposed" (NEVER Accepted; maintainer's checkpoint), with Context/Decision/Alternatives(>=2)/Consequences/Related/References; to supersede an Accepted ADR write a NEW one and do NOT touch the old body/Status. Else adrAction="none". Use the next free ADR number. Return action + file paths for Land to stage + one-line summary.`
const landPrompt = (sel, adr) =>
  `Land the reviewed work for TASK ${sel.taskId} in ${REPO} (branch ${BRANCH}).\n${RULES}\n\nsrcTouched=${sel.srcTouched}; fieldValidationRemains=${sel.fieldValidationRemains}; skipBump=${SKIP_BUMP}.\n` +
  `Steps (ORDER MATTERS — status must be INSIDE the commit so a checkout cannot revert it):\n` +
  `1. Set final status FIRST: if fieldValidationRemains "In Review", else "Done". \`backlog task edit ${sel.taskId} -s "<status>" --append-notes "<what landed + remaining field-validation ACs>"\`.\n` +
  (SKIP_BUMP
    ? `2. Do NOT bump the prerelease (parallel lane; the bump+tag happens at serial integration when this sub-branch merges). prereleaseTag="".\n`
    : `2. If srcTouched, run \`bin/bump-prerelease.sh\` (stages version.h + banners); record the new tag (e.g. alpha.182) as prereleaseTag. Else prereleaseTag="".\n`) +
  `3. Stage ONLY this task's changed paths via \`git add <explicit paths>\` (NEVER \`git add -A\`): its source/doc files, the bump files (if any), ADR files (${JSON.stringify(adr.adrFiles)}), AND the task's own backlog/tasks/*.md (now carrying the new status). Do NOT stage sibling task notes or other in-flight work.\n` +
  `4. Commit subject \`<type>(<scope>): <imperative> (${sel.taskId})\`. No em dashes. Co-author trailer. \`git push origin ${BRANCH}\` with retry.\n` +
  `5. featureSummary: 1-2 plain sentences on the user-facing feature/improvement (for #alpha-testing). Empty if nothing committed.\n` +
  `Return committed/commitHash/pushed/newStatus/prereleaseTag/featureSummary/note.`
const announcePrompt = (sel, land) =>
  `Report TASK ${sel.taskId} to Discord per the alpha-channel policy (memory feedback_discord_alpha_channel). Use mcp__discord-mcp__discord_post_message. English, facts only.\n` +
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

  phase('ADR')
  let adr = await agent(adrPrompt(sel), { label: `adr:${sel.taskId}`, phase: 'ADR', schema: ADR, agentType: 'adr-kit:adr-generator' })
  if (!adr) adr = { adrAction: 'none', adrFiles: [], summary: 'ADR agent unavailable (transient); skipped' }
  if (adr.adrAction !== 'none') log(`ADR ${adr.adrAction}: ${adr.adrFiles.join(', ')}`)

  phase('Land')
  const land = await agent(landPrompt(sel, adr), { label: `land:${sel.taskId}`, phase: 'Land', schema: LAND })
  if (!land) { await cleanup(sel.taskId, 'land agent died (transient)'); endReason = `transient abort on ${sel.taskId}`; break }
  if (!land.committed) { endReason = `land did not commit ${sel.taskId}: ${land.note}`; break }

  phase('Announce')
  const ann = await agent(announcePrompt(sel, land), { label: `announce:${sel.taskId}`, phase: 'Announce' })
  log(`Announced ${sel.taskId}: ${ann || 'posted'}`)

  completed.push({ task: sel.taskId, title: sel.title, status: land.newStatus, commit: land.commitHash, tag: land.prereleaseTag, adr: adr.adrAction, srcTouched: sel.srcTouched })
  log(`Landed ${sel.taskId} -> ${land.newStatus}${land.tag ? ' (' + land.tag + ')' : ''}. Advancing immediately to the next task.`)
}

return { done: true, completed, count: completed.length, endReason }
