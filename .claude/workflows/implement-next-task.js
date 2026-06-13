export const meta = {
  name: 'implement-next-task',
  description: 'Implement the next actionable async-esp32s3 backlog task: select -> implement -> build/eval -> adversarial review -> bump+commit+push',
  phases: [
    { title: 'Select', detail: 'pick lowest-seq To Do task whose deps are Done/In Review' },
    { title: 'Implement', detail: 'one coding agent, build + evaluator gated' },
    { title: 'Review', detail: 'adversarial check vs ACs + binding ADRs' },
    { title: 'ADR', detail: 'document architectural decisions as a Proposed ADR' },
    { title: 'Land', detail: 'bump (if src) + commit (incl. ADR) + push + set status' },
  ],
}

const REPO = 'D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-esp32s3-async'
const BRANCH = 'feature-2.0.0-esp32s3-async'

const RULES =
  `Project rules (CLAUDE.md, binding):\n` +
  `- Run ALL commands in the worktree ${REPO} (branch ${BRANCH}). Never touch dev or feature-dev-2.0.0-otgw32-esp32-sat-support.\n` +
  `- PROGMEM: all string literals via F()/PSTR()/snprintf_P; strcmp_P/strcasecmp_P; memcmp_P for binary.\n` +
  `- No ArduinoJson. No String in hot paths (ADR-004). char[] + strlcpy/snprintf_P.\n` +
  `- No raw #if(def) ESP8266/ESP32/ARDUINO_ARCH_ESP*/BOARD_NODOSHOP_ESP* outside the allowlisted abstraction files (platform*.h, boards.h, OTGW-ModUpdateServer*). Add a platformXxx() shim or HAS_* flag FIRST, then call it unguarded.\n` +
  `- Vendored src/libraries/SimpleTelnet/** and src/libraries/OTGWSerial/OTGWSerial.cpp are OUT OF SCOPE (do not strip their internal ESP8266 ifdefs) unless the task explicitly says otherwise.\n` +
  `- PIC commands via addOTWGcmdtoqueue(); never raw Serial writes. feedWatchDog() in long loops.\n` +
  `- Use the backlog CLI for task status; never edit backlog/tasks/*.md directly.\n` +
  `- Architecture references (read-only): other-projects/EMS-ESP32-dev (async+FreeRTOS+espMqttClient blueprint), other-projects/OT-Thing-OTGW32. Never modify other-projects/.\n` +
  `- build.py masks per-env failures: ALWAYS grep the build log for the per-env SUCCESS line; do not trust exit 0 alone.`

// ---- Phase: Select ---------------------------------------------------------
phase('Select')
const SEL = {
  type: 'object',
  required: ['none', 'taskId', 'title', 'body', 'buildTargets', 'srcTouched', 'fieldValidationRemains'],
  properties: {
    none: { type: 'boolean', description: 'true if no actionable task remains' },
    taskId: { type: 'string', description: 'e.g. TASK-865.1, empty if none' },
    title: { type: 'string' },
    body: { type: 'string', description: 'the full task description markdown (## Why ... ## Acceptance Criteria ...)' },
    buildTargets: { type: 'array', items: { type: 'string' }, description: 'pio/build targets the ACs require, e.g. ["esp32","esp32-classic"]' },
    srcTouched: { type: 'boolean', description: 'true if the task changes src/OTGW-firmware/** or src/libraries/** (=> needs a prerelease bump)' },
    fieldValidationRemains: { type: 'boolean', description: 'true if the task has any field-validation AC that cannot be closed without hardware (=> end state is In Review, not Done)' },
  },
}
const sel = await agent(
  `Select the next actionable backlog task for the async ESP32-S3 migration.\n${RULES}\n\n` +
  `In ${REPO}: list backlog/tasks/task-865.*.md (these carry label async-esp32s3, parent TASK-865). Read each frontmatter (id, status, dependencies) via the backlog CLI: \`backlog task <id> --plain\`.\n` +
  `SINGLE-FLIGHT GUARD (check FIRST): if ANY task-865.* already has status "In Progress", a prior iteration is still running in this shared worktree — return none=true immediately. Do NOT start a concurrent iteration (two agents editing the same worktree corrupt the git index and .pio build).\n` +
  `Otherwise pick the LOWEST dotted seq (865.1 < 865.2 < ... 865.13) whose status is "To Do" AND every dependency is "Done" or "In Review". Skip the epic TASK-865 itself and anything In Review/Done.\n` +
  `Return none=true if nothing qualifies. Otherwise return that task's id, title, the full Description body (for the implementer), the build targets its ACs name, whether it touches src/** (needs a version bump), and whether it has any field-validation AC (=> ends In Review).`,
  { label: 'select-next', phase: 'Select', schema: SEL }
)
if (sel.none || !sel.taskId) { log('No actionable task — all done or blocked on deps/field-validation.'); return { done: true } }
log(`Selected ${sel.taskId}: ${sel.title}`)

// ---- Phase: Implement ------------------------------------------------------
phase('Implement')
const IMPL = {
  type: 'object',
  required: ['summary', 'filesChanged', 'buildPassed', 'evalPassed', 'acsMetBuildEval', 'blockers'],
  properties: {
    summary: { type: 'string' },
    filesChanged: { type: 'array', items: { type: 'string' } },
    buildPassed: { type: 'boolean', description: 'all required pio targets show the per-env SUCCESS line' },
    evalPassed: { type: 'boolean', description: 'python evaluate.py --quick has no NEW failures vs baseline' },
    acsMetBuildEval: { type: 'string', description: 'per-AC: which build/evaluator-verifiable ACs are met, with evidence' },
    blockers: { type: 'string', description: 'what is unresolved; empty if clean' },
  },
}
const implPrompt = (extra) =>
  `Implement this backlog task end-to-end in ${REPO} (branch ${BRANCH}).\n${RULES}\n\n` +
  `TASK ${sel.taskId}: ${sel.title}\n\n${sel.body}\n\n` +
  `Steps: (1) \`backlog task edit ${sel.taskId} -s "In Progress" -a @claude\`. (2) Implement per the ## Acceptance Criteria, reading the real code at the cited anchors first (anchors may have drifted — re-grep). (3) Build: \`python build.py --target <each of ${JSON.stringify(sel.buildTargets)}>\` and GREP the log for the per-env SUCCESS line. (4) \`python evaluate.py --quick\` — no NEW failures. (5) Do NOT commit, bump, or change task status here (the Land phase does that). Only satisfy the build/evaluator-verifiable ACs; field-validation ACs are out of scope (no hardware).\n` +
  (extra || '') +
  `Return what changed, whether build + evaluator passed, which build/evaluator ACs are met with evidence, and any blockers.`
// agent() returns null when a subagent dies on a terminal API error after retries
// (e.g. transient server rate-limiting). Don't crash and don't leave the task stuck
// In Progress: clean the tree, reset it To Do, and bail so the next tick retries.
const abortRetry = async (why) => {
  log(`ABORT (retriable): ${why}. Resetting ${sel.taskId} to To Do and discarding partial edits.`)
  await agent(
    `A workflow iteration for ${sel.taskId} aborted mid-run (${why}). Clean up in ${REPO} so the next tick can retry from a clean tree: run \`git -C ${REPO} checkout -- src/ backlog/tasks/\` to revert partial tracked edits; remove any partial NEW untracked source files this attempt created under src/OTGW-firmware or src/libraries (use \`git -C ${REPO} status --porcelain\` to find \`??\` entries; do NOT remove other-projects/ or the copied OpenTherm/SimpleTelnet lib dirs); then \`backlog task edit ${sel.taskId} -s "To Do" -a @claude\`. Confirm the src/ and backlog/tasks/ trees are clean.`,
    { label: `abort-cleanup:${sel.taskId}`, phase: 'Implement' }
  )
  return { done: false, task: sel.taskId, title: sel.title, status: 'To Do', aborted: true, reason: why }
}
let impl = await agent(implPrompt(), { label: `impl:${sel.taskId}`, phase: 'Implement', schema: IMPL })
if (!impl) return await abortRetry('implement agent died (transient API error / rate limit)')

// ---- Phase: Review ---------------------------------------------------------
phase('Review')
const REV = {
  type: 'object',
  required: ['pass', 'issues'],
  properties: {
    pass: { type: 'boolean', description: 'true if the diff satisfies the build/evaluator ACs and violates no binding ADR/CLAUDE rule' },
    issues: { type: 'string', description: 'concrete, actionable problems; empty if pass' },
  },
}
const revPrompt =
  `Adversarially review the working-tree diff in ${REPO} for TASK ${sel.taskId}.\n${RULES}\n\n` +
  `TASK body (the contract):\n${sel.body}\n\n` +
  `Run \`git -C ${REPO} diff\` (and \`git -C ${REPO} status\`). Check: (a) every build/evaluator-verifiable AC is actually met (re-run build/evaluate if unsure — build.py masks per-env failures, grep the SUCCESS line); (b) no binding ADR or CLAUDE rule is violated (PROGMEM, no-String-hot-path, raw-ifdef abstraction boundary, vendored-lib scope, no direct task-file edits); (c) scope discipline — only the task's files changed, no stray edits to other sessions' work. Default to pass=false if a build/evaluator AC is unproven. Be specific in issues.`
let rev = await agent(revPrompt, { label: `review:${sel.taskId}`, phase: 'Review', schema: REV })
if (!rev) return await abortRetry('review agent died (transient API error / rate limit)')
if (!rev.pass) {
  log(`Review found issues — one fix pass: ${rev.issues}`)
  impl = await agent(implPrompt(`A prior review FAILED with these issues — fix them specifically:\n${rev.issues}\n`), { label: `fix:${sel.taskId}`, phase: 'Implement', schema: IMPL })
  if (!impl) return await abortRetry('fix agent died (transient API error / rate limit)')
  rev = await agent(revPrompt, { label: `re-review:${sel.taskId}`, phase: 'Review', schema: REV })
  if (!rev) return await abortRetry('re-review agent died (transient API error / rate limit)')
}

// ---- Phase: ADR (document architectural decisions) ------------------------
phase('ADR')
const ADR = {
  type: 'object',
  required: ['adrAction', 'adrFiles', 'summary'],
  properties: {
    adrAction: { type: 'string', enum: ['none', 'created', 'amended', 'superseding'], description: 'what happened to the ADR corpus this iteration' },
    adrFiles: { type: 'array', items: { type: 'string' }, description: 'docs/adr/*.md paths created/changed for Land to stage; empty if none' },
    summary: { type: 'string', description: 'the decision documented, or a one-line why-no-ADR' },
  },
}
let adr = { adrAction: 'none', adrFiles: [], summary: 'skipped (review did not pass)' }
if (rev.pass) {
  adr = await agent(
    `Document the architectural decisions in the working-tree diff for TASK ${sel.taskId} in ${REPO}, per this project's adr-kit discipline.\n${RULES}\n\n` +
    `TASK body (its ACs may explicitly REQUIRE ADR work, e.g. supersede ADR-108/011/047/048/058 or add a new ADR):\n${sel.body}\n\n` +
    `Run \`git -C ${REPO} diff\`. Apply the adr.review.md checks: did this change make or alter an architectural decision (new pattern, new dependency, API/MQTT/topic contract change, concurrency/threading model, supersession of an Accepted ADR)?\n` +
    `- If YES (or the task ACs require it): author a NEW ADR (or amend a still-Proposed one) at docs/adr/ADR-NNN-kebab.md using adr-kit conventions: Status MUST be "Proposed" (NEVER flip to Accepted; that is the maintainer's human checkpoint), with Context / Decision / Alternatives Considered (>=2) / Consequences (positive+negative) / Related / References. To supersede an Accepted ADR, write a NEW superseding ADR; do NOT edit the old body, and do NOT change its Status line (the maintainer sets "Superseded by ADR-NNN" only after accepting the new one).\n` +
    `- If NO architectural decision: adrAction="none" with a one-line reason.\n` +
    `Use the next free ADR number (check docs/adr/). Return the action, the file paths for Land to stage, and a one-line summary.`,
    { label: `adr:${sel.taskId}`, phase: 'ADR', schema: ADR, agentType: 'adr-kit:adr-generator' }
  )
  if (!adr) adr = { adrAction: 'none', adrFiles: [], summary: 'ADR agent unavailable (transient); skipped this iteration' }
  if (adr.adrAction !== 'none') log(`ADR ${adr.adrAction}: ${adr.adrFiles.join(', ')}`)
}

// ---- Phase: Land -----------------------------------------------------------
phase('Land')
const LAND = {
  type: 'object',
  required: ['committed', 'commitHash', 'pushed', 'newStatus', 'note', 'featureSummary'],
  properties: {
    committed: { type: 'boolean' },
    commitHash: { type: 'string' },
    pushed: { type: 'boolean' },
    newStatus: { type: 'string', description: 'In Review or Done' },
    note: { type: 'string' },
    featureSummary: { type: 'string', description: '1-2 plain-language sentences on the user-facing feature/improvement this task delivered, for an #alpha-testing announcement; empty if nothing committed' },
  },
}
const land = await agent(
  `Land the completed work for TASK ${sel.taskId} in ${REPO} (branch ${BRANCH}).\n${RULES}\n\n` +
  `Pre-state: build/evaluator review pass=${rev.pass}; issues=${rev.issues || 'none'}. srcTouched=${sel.srcTouched}; fieldValidationRemains=${sel.fieldValidationRemains}.\n` +
  `Steps (ORDER MATTERS — the status must land INSIDE the commit so a later git checkout cannot revert it):\n` +
  `1. If review did NOT pass: do NOT commit. Set the task to "To Do", append a note with the blocking issues, return committed=false, newStatus="To Do".\n` +
  `2. Set the final status FIRST (before staging): if fieldValidationRemains set "In Review" (hardware/user-sign-off ACs cannot be self-certified), else (docs/build-config only) set "Done". \`backlog task edit ${sel.taskId} -s "<status>" --append-notes "<what landed + remaining field-validation ACs>"\`.\n` +
  `3. If srcTouched, run \`bin/bump-prerelease.sh\` (it stages version.h + banners).\n` +
  `4. Stage ONLY this task's changed paths via \`git add <explicit paths>\` (NEVER \`git add -A\`): its source/doc files, the bump files, any ADR files (${JSON.stringify(adr.adrFiles)}), AND the task's own backlog/tasks/*.md now carrying the new status. Do NOT stage sibling task notes or other in-flight work.\n` +
  `5. Commit: subject \`<type>(<scope>): <imperative> (${sel.taskId})\` (satisfies the commit-msg hook). No em dashes. Co-author trailer. \`git push -u origin ${BRANCH}\` with retry.\n` +
  `6. Produce featureSummary: 1-2 plain-language sentences on the user-facing feature/improvement (for an #alpha-testing semver-step announcement). Empty if nothing committed.\n` +
  `Return commit hash, push result, the new status, a one-line note, and featureSummary.`,
  { label: `land:${sel.taskId}`, phase: 'Land', schema: LAND }
)
if (!land) return { done: false, task: sel.taskId, title: sel.title, status: 'In Progress', landFailed: true, reason: 'land agent died (transient API error / rate limit) after commit-ready work; needs manual land + status set' }

return { done: false, task: sel.taskId, title: sel.title, build: impl.buildPassed, eval: impl.evalPassed, reviewPass: rev.pass, adr: adr.adrAction, adrFiles: adr.adrFiles, status: land.newStatus, commit: land.commitHash, pushed: land.pushed, srcTouched: sel.srcTouched, featureSummary: land.featureSummary, note: land.note, blockers: impl.blockers }
