export const meta = {
  name: 'translate-webui-language',
  description: 'Add, sync or refresh one v2 web UI language pack: glossary, translate, two adversarial reviews, editor, static verify',
  whenToUse: 'Run from the translate_webui_otgw-firmware skill with args {code, name, nativeName, mode}. Web UI only; never touches firmware.',
  phases: [
    { title: 'Prepare', detail: 'English source, glossary for the language, wiring for a new language' },
    { title: 'Translate', detail: 'all keys (add/refresh) or only missing keys (sync)' },
    { title: 'Review', detail: 'two adversarial reviewers, then an editor' },
    { title: 'Verify', detail: 'static checks only, no Chrome' },
  ],
}

// args: { code: 'fr', name: 'French', nativeName: 'Français', mode: 'add' | 'sync' | 'refresh', wt: '<worktree path>', work: '<scratch dir>' }
const A = args || {}
if (!A.code || !A.name || !A.wt || !A.work) throw new Error('args need code, name, wt and work (and nativeName for mode add)')
const MODE = A.mode || 'add'
const WT = A.wt
const W = A.work
const L = { code: A.code, name: A.name, nativeName: A.nativeName || A.name }

const RULES = `Project: OTGW-firmware 2.0.0, an OpenTherm gateway for central-heating boilers, used by hobbyists with Home Assistant. Worktree ${WT}.
HARD SCOPE: the v2 WEB UI ONLY (src/OTGW-firmware/data/v2.html, v2.js, v2.css and the packs in i18n/). Never touch firmware (.ino/.h/.cpp), MQTT, Home Assistant discovery, REST/API names, telnet, docs or FSexplorer.html. Texts the firmware sends at runtime stay as received.
Keep in English: OpenTherm MsgID names and numbers, commands (TT=, CS=, GW= ...), units (°C, bar, %, kW, l/min, min), product names (OTGW, SAT, OpenTherm, Home Assistant, MQTT, PIC, BLE, Dallas/1-Wire, OT-Direct), {{placeholders}}, ids, CSS classes, JSON keys.
NO TESTS WITH CHROME: never run node --test on a page-harness test, never pass several test files or a folder to node --test, never start Chrome (a parallel webui run once spawned 638 Chrome). Validate with node -e / plain node scripts only. Never build, flash, touch a device, bump versions, commit or git add/stash/reset.`
const STYLE = `Audience: hobbyists running this on their own boiler. Tone: clear, concise, friendly, a decent technical level for labels; hints, tooltips and warnings must be understandable for regular people (no unexplained jargon, no literal translation of English idioms). Sound native, not machine-translated. Labels stay about as short as the English (at most ~130%), buttons are verbs (or the noun form the language's UIs use). Keep {{placeholders}} exactly; plurals use key_one/key_other with params.count. Follow the glossary.`

phase('Prepare')
const prep = await agent(`${RULES}

Step 1: write the English source. Extract the I18N_EN object from ${WT}/src/OTGW-firmware/data/v2.js (it is JSON between "var I18N_EN = " and the closing "\\n  };") and save it to ${W}/en.json.
Step 2: the glossary. If ${WT}/i18n/glossary.md exists, read it; add a column or section for ${L.name} (${L.code}) with the natural heating/OpenTherm/home-automation term a hobbyist in that language uses for each listed term, plus a STYLE paragraph (register, button form, decimal separator, how to refer to SAT). Do not change the other languages' entries. If it does not exist, create it from scratch with ~90 core terms in the same layout.
Step 3 (mode=${MODE}): ${MODE === 'add' ? `wire the new language: add '${L.code}' to "var I18N_LANGS" in v2.js; add a <li><a href="#" lang="${L.code}" data-lang="${L.code}"><span class="flag flag-${L.code}" aria-hidden="true"></span><span class="nm">${L.nativeName}</span><span class="code">${L.code.toUpperCase()}</span></a></li> entry to the #langMenu list in v2.html (same shape as the others); add a flag SVG under src/OTGW-firmware/data/flags/ (country flag for the language, 3:2 or the country's ratio, small hand-written SVG like the existing ones; for a language with several countries pick the most common one and say which) and a ".lang-menu .flag-${L.code}{background-image:url(flags/<file>.svg)}" rule next to the other flag rules in v2.css. Check node --check v2.js.` : 'no wiring (the language already exists); confirm it is in I18N_LANGS and the menu.'}
Return: paths, glossary summary for ${L.name}, and what you wired.`, { label: `prepare:${L.code}`, phase: 'Prepare' })

phase('Translate')
const scope = MODE === 'sync'
  ? `Only translate keys that are in en.json but missing from (or empty in) ${WT}/i18n/${L.code}.json, and remove keys that no longer exist in en.json. Leave existing translations unchanged.`
  : `Translate every key (for refresh: start from the existing ${WT}/i18n/${L.code}.json and improve it).`
const tr = await agent(`${RULES}

${STYLE}

Translate the web UI into ${L.name}. Source ${W}/en.json, glossary ${WT}/i18n/glossary.md. ${scope}
Write ${WT}/i18n/${L.code}.json: exactly the key set of en.json, nested i18next v4 JSON, UTF-8, 2-space indent. Context matters: grep each key in ${WT}/src/OTGW-firmware/data/v2.js and v2.html to see where it is used before choosing a word ('Flow', 'Set', 'Start' have several meanings). Validate with node -e (parse, key parity, placeholder parity).
Return: key count, the 15 hardest choices with reasoning, English source texts that look wrong (report only).`, { label: `translate:${L.code}`, phase: 'Translate', model: 'opus' })

phase('Review')
const lenses = [
  `NATIVE TONE + TERMINOLOGY: you are a native ${L.name} speaker who knows central heating. Read every value in ${WT}/i18n/${L.code}.json next to the English (${W}/en.json) and its use in v2.js/v2.html. Flag anything machine-like, stiff, wrong register, inconsistent with the glossary, technically wrong for heating/OpenTherm, or ambiguous out of context. Check technical claims against the code (e.g. what a setting really does in src/OTGW-firmware/*.ino) before proposing a wording. Assume there are errors.`,
  `CLARITY FOR REGULAR PEOPLE + MECHANICS: hints, tooltips and warnings understandable for a non-expert, concise; labels not much longer than English (overflow risk on buttons, tiles, tabs); {{placeholders}} identical; plural forms present and correct for ${L.name}; punctuation, quotes and decimal conventions of ${L.name}; no English left that should be translated, and product/protocol names NOT translated. Assume there are errors.`,
]
const reviews = await parallel(lenses.map((lens, j) => () => agent(`${RULES}

${STYLE}

Review lens: ${lens}
Do not edit files. Return findings: key, current text, problem, proposed text. Say "no findings" only if you truly found nothing.`, { label: `review:${L.code}:${j}`, phase: 'Review', model: 'opus' })))
const editor = await agent(`${RULES}

${STYLE}

You are the ${L.name} editor. Two adversarial reviewers reported on ${WT}/i18n/${L.code}.json:
--- reviewer A ---
${reviews[0]}
--- reviewer B ---
${reviews[1]}
Judge each finding yourself (accept, adapt, or reject with a one-line reason; reviewers can be wrong, check claims against the code). Apply the accepted ones; keep the key set identical to ${W}/en.json; validate with node -e. Return accepted/rejected counts, the rejections with reasons, and 10 before -> after examples.`, { label: `edit:${L.code}`, phase: 'Review', model: 'opus' })

phase('Verify')
const verify = await agent(`${RULES}

Static verification in ${WT} (no Chrome, one command at a time):
1. node --check src/OTGW-firmware/data/v2.js.
2. A node script: I18N_EN keys == i18n/${L.code}.json keys; placeholders identical per key; plural pairs complete; no empty values; count values identical to English and list them (they should only be product names, units, protocol terms).
3. node --test tests/webui/i18n-no-hardcoded.test.mjs (static, alone).
4. node --test tests/webui/i18n-lang.test.mjs (static: a local http server, no Chrome; alone).
5. python evaluate.py --quick (FAIL count).
6. LittleFS fit: stage the pages like build.py (scripts/webui_pages.py write_pages + stage_i18n_packs on a temp copy) and report i18n/${L.code}.json.gz size, v2.html.gz size and the total staged size against the spiffs partition in the esp32-combo partitions csv (0xC0000 = 768 KiB at the time of writing).
7. Read 40 random entries side by side with English and report anything still wrong (no edits).
Return exact counts and paths. Then list what the caller still has to do on its own: the lang-menu page test (single file, Chrome count before/after), screenshots per page (single Chrome), bump, commit, build, OTA, soak, push, alpha post.`, { label: `verify:${L.code}`, phase: 'Verify' })

return { prep, translate: tr, reviews, editor, verify }
