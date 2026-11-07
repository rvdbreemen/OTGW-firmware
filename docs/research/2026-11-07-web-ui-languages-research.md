# Multi-language web UI for OTGW-firmware 2.x: research report

Date: 2026-11-07. Sources: six research reports, each checked by an independent verifier, plus one local measurement of the current UI text (section 6.6). Where a verifier corrected a claim, this report uses the correction. Claims nobody checked are labelled **(unchecked)**. Anything marked **Opinion** is my judgement, not a sourced fact.

---

> **Update 2026-11-07, after this report:** Robert decided to revisit ADR-003 so that the device itself may fetch packs over HTTPS (option d). A proof of concept for an outbound TLS client on the ESP32-S3 is running under TASK-1689 (branch poc-tls-1689). Its flash, heap and stack numbers will decide whether (d) joins (a) to (c); until then the statements on (d) below describe the situation without a device TLS client.

## 1. Answer first

1. **Use no i18n library.** Write a small `t(key, params)` helper in v2.js. Language packs are flat `key -> string` JSON files, and plurals use the built-in `Intl.PluralRules`. Every library that works without a bundler adds 1 to 20 KB gz to v2.html.gz and gives this UI nothing it needs.
2. **English stays built into v2.js** as the source of truth and as the fallback for each key. A missing, late or broken pack can never break the UI. It only shows English.
3. **Ship NL and DE as separate gzipped files on LittleFS** (estimate: about 13 to 16 KB gz per language including settings help, so about 26 to 32 KB for both). They cost no app-slot flash, and the main user base never depends on the internet.
4. **Packs on GitHub work, but only through the browser.** Fetch from jsDelivr pinned to the firmware's commit or tag, with an SRI hash built into the firmware. The device cannot fetch them itself: GitHub and jsDelivr answer plain HTTP with a 301 to HTTPS, and the device has no TLS. Use GitHub for long-tail languages (FR, PL, ...) and offer an "install on device" button: the browser fetches the pack and POSTs it to LittleFS.
5. **Version each pack against the firmware with a key-set hash** computed by build.py. Add a CI check that reports coverage per language, so packs cannot quietly fall behind the way OpenDTU-OnBattery's did.
6. **Translators: Hosted Weblate, libre plan** (free; the GPL-3.0 public repo qualifies). Keep the catalogs outside `src/` so bot PRs do not trigger the prerelease-bump rule.
7. **Translate only the web UI.** REST, MQTT and HA discovery stay English (the Tasmota decision, section 2).

---

## 2. How other firmwares do it

| Project | Mechanism | How the language is chosen | Pack delivery | Flash cost | Sources |
|---|---|---|---|---|---|
| **EMS-ESP32** (web UI, typesafe-i18n 5.27.1) | One TS module per locale (12 locales incl. nl, de), lazy-loaded chunk | localStorage `lang`, else browser language, else `en`; per browser | All 12 compiled into the image (gzip level 9 C arrays). The build-time allow-list `VITE_APP_LOCALES` exists, but CI does not use it | About 6 KB gz per locale (proxy: gzipped source files, not a build) | [App.tsx](https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/interface/src/App.tsx), [progmem-generator.js](https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/interface/progmem-generator.js), [dev_release.yml](https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/.github/workflows/dev_release.yml) |
| **EMS-ESP32** (entity names in firmware) | One C macro row per string, 12 columns, in PROGMEM | Device setting, separate from the UI language | Compiled in | High enough that every 4 MB board builds with `-DEMSESP_EN_ONLY`; test builds keep 2 languages "to save flash memory" | [emsesp_common.h](https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/src/core/emsesp_common.h), [platformio.ini](https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/platformio.ini) |
| **OpenDTU** (vue-i18n) | en/de/fr bundled; other languages as runtime `*.lang.json` packs | `navigator.language` base tag, then localStorage `locale`; fallback `en` per key, whole UI falls back if the fetch fails | Packs kept in the repo `lang/`. The user downloads one and uploads it (UI writes `pack.lang.json`, then reboots). Through the API several packs can coexist. The device lists `*.lang.json` and streams them with an md5 ETag | 0 app flash; about 30 to 60 KB LittleFS per pack (stored uncompressed) | [i18n.ts](https://github.com/tbnobody/OpenDTU/blob/8da432d52f33a7598deb93550df2115b8e5bb700/webapp/src/i18n.ts), [I18n.cpp](https://github.com/tbnobody/OpenDTU/blob/8da432d52f33a7598deb93550df2115b8e5bb700/src/I18n.cpp), [WebApi_i18n.cpp](https://github.com/tbnobody/OpenDTU/blob/8da432d52f33a7598deb93550df2115b8e5bb700/src/WebApi_i18n.cpp), [lang/README.md](https://github.com/tbnobody/OpenDTU/blob/8da432d52f33a7598deb93550df2115b8e5bb700/lang/README.md) |
| **OpenDTU-OnBattery** | Same as OpenDTU, inherited | Same | Same | Same | [locales](https://github.com/hoylabs/OpenDTU-OnBattery/tree/cd00fa550d4713792886f35f7f263679d935376e/webapp/src/locales) |
| **AhoyDTU** | Build-time `{#TOKEN}` replacement in HTML | Chosen by which `.bin` you flash (`-de` envs) | One image per language | +1,200 B for the DE image (measured from release zip) | [convertHtml.py](https://github.com/lumapu/ahoy/blob/c4fe30f118bd334afa548d71b56018b73dafd6cb/scripts/convertHtml.py), [v0.8.156](https://github.com/lumapu/ahoy/releases/tag/ahoy_v0.8.156) |
| **Tasmota** v16.0.0 | C `#define` header per language (28) | `MY_LANGUAGE` at compile time | 27 extra images per chip family (ESP8266 envs and ESP32 envs in separate ini files); 287 release assets | ESP8266: DE +432 B, NL +896 B. ESP32: DE +608 B, NL +1,088 B | [platformio_tasmota_env.ini](https://raw.githubusercontent.com/arendst/Tasmota/development/platformio_tasmota_env.ini), [env32.ini](https://raw.githubusercontent.com/arendst/Tasmota/development/platformio_tasmota_env32.ini), [releases API](https://api.github.com/repos/arendst/Tasmota/releases/latest), [#1367](https://github.com/arendst/Tasmota/issues/1367), [#1473](https://github.com/arendst/Tasmota/issues/1473) |
| **ESPHome** web_server | No i18n; `"lang"` is hard-coded to `"en"` | n/a | UI JS comes from a CDN by default (`oi.esphome.io`); `local: true` inlines it | 0 by default ("still uses a good amount of flash" when local) | [docs](https://esphome.io/components/web_server/), [web_server.cpp](https://raw.githubusercontent.com/esphome/esphome/dev/esphome/components/web_server/web_server.cpp) |
| **WLED** | None in official builds | n/a | PR #4274 (closed 2026-04-14 for inactivity and messy structure): fetched packs **only** from a hard-coded GitHub raw URL. Its localStorage cache never hit because the read and write keys differ. PR #5671 is still an open draft. The maintainers said they will not accept it in this form and want translations "off-tree" | n/a | [#459](https://github.com/wled/WLED/issues/459), [#4274](https://github.com/wled/WLED/pull/4274), [#5671 comments](https://api.github.com/repos/wled/WLED/issues/5671/comments?per_page=100) |
| **ESP3D-WEBUI** v3.1+ | `lang-<code>.json(.gz)` on the device filesystem, with an optional `_manifest` | Scans `/languages` | Per firmware target; the docs do not say how a pack reaches the device | Not stated | [translation docs](https://esp3d.io/esp3d-webui/version_3x/documentation/translation/) |
| **OpenBeken** | No UI i18n found; device serves a stub, UI comes from GitHub Pages (`webappRoot` default in `new_cfg.c`) | n/a | Remote UI | "virtually free" (unverified, page timed out) | [OpenBekenIOT/webapp](https://github.com/OpenBekenIOT/webapp) |
| **ESPEasy** | English `F()` labels rendered server-side; no i18n discussion found (search result only) | n/a | n/a | n/a | [RootPage.cpp](https://raw.githubusercontent.com/letscontrolit/ESPEasy/mega/src/src/WebServer/RootPage.cpp) |

Non-firmware pack systems, for design patterns:

| Project | Pattern worth copying | Sources |
|---|---|---|
| **OpenWrt LuCI** | English strings inline in JS act as key and fallback: `_(s)` returns `TR[hash(s)] \|\| s`. Packs are separate packages without a version lock (luci-i18n-base-nl 46.0 KB ipk) | [cbi.js](https://raw.githubusercontent.com/openwrt/luci/master/modules/luci-base/htdocs/luci-static/resources/cbi.js), [luci.mk](https://raw.githubusercontent.com/openwrt/luci/master/luci.mk) |
| **Home Assistant frontend** | Pack file name carries a content hash; the app build compiles in the expected hash, so each build pins its exact pack; English is merged into every pack; falls back to `en` when the fetch fails | [translations.js](https://raw.githubusercontent.com/home-assistant/frontend/dev/build-scripts/gulp/translations.js), [common-translation.ts](https://raw.githubusercontent.com/home-assistant/frontend/dev/src/util/common-translation.ts) |
| **OctoPrint** | User uploads a pack (zip only; the docs also list tar, but the code rejects it) to device storage; no version check | [languages.py](https://raw.githubusercontent.com/OctoPrint/OctoPrint/master/src/octoprint/server/api/languages.py) |
| **Grafana, Fluidd, Mainsail** | English bundled, other locales lazy-loaded; Fluidd stays on the current locale when a load fails | [Fluidd i18n.ts](https://raw.githubusercontent.com/fluidd-core/fluidd/develop/src/plugins/i18n.ts), [Grafana i18n.tsx](https://raw.githubusercontent.com/grafana/grafana/main/packages/grafana-i18n/src/i18n.tsx) |

**Findings (facts):**
- None of the firmwares studied fetches language packs from GitHub at runtime in a released build. The only attempt, WLED #4274, was never merged.
- Compile-time, per-image languages are almost free on flash, but they multiply the build and release matrix, and users must reflash to switch language.
- Packs kept outside the feature workflow decay. OpenDTU-OnBattery has 1,261 en keys, while fr has 879 and the inherited es pack 632. English fallback hides the gap, and neither project has a CI check for it.
- Tasmota decided in 2018 never to translate JSON or MQTT. Collaborator davidelang: "Language choice for the GUI should never change the over-the-wire API."
- WLED's maintainers objected on maintenance grounds, not flash: "the wled core team neither has the knowledge nor the people needed to keep translations up-to-date".

**Source conflict:** the EMS-ESP doc page [Adding-Languages](https://emsesp.org/Adding-Languages/) names `local_translations.h` / `MAKE_PSTR_LIST`, while the code uses `locale_translations.h` / `MAKE_TRANSLATION`. The doc is stale.

---

## 3. Library options for a vanilla-JS UI

Sizes are Bundlephobia gzip figures, re-queried by the verifier on 2026-11-07. "Drop-in" means it works as a plain `<script>` with no bundler, which is how build.py inlines v2.js.

| Option | gz size | Licence | Last release | Drop-in? | Fit for OTGW |
|---|---|---|---|---|---|
| **No library** (`t()` + `Intl.PluralRules` + about 25-line locale matcher) | about 0.3 to 0.8 KB (estimate, not measured) | ours | n/a | yes | **Best.** NL and DE need only one/other plural forms. `Intl.PluralRules` is Baseline widely available since 2022-03 ([MDN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/PluralRules)) |
| i18next 27.0.0 (+ http-backend 4.0.10 + language detector 8.3.0) | 15,249 B (about 20 KB with both plugins) | MIT | 27.0.0 published **2026-11-07** | yes (UMD) | Overkill, about 12% of v2.html.gz. v27 changed defaults (`fallbackLng` now `false`) and was hours old when checked |
| typesafe-i18n 5.27.1 | 1,270 B | MIT | 2026-02-11 (previous release 2023-08) | yes (IIFE) | Its value is the TS type generator, which a vanilla-JS UI cannot use |
| @fluent/bundle 0.19.1 | 4,081 B (minified) | Apache-2.0 | 2025-04-02 | UMD, but not minified | Rich grammar for translators, but a new `.ftl` format; unneeded |
| @lingui/core 6.12.0 | 2,466 B | MIT | 2026-11-03 | no (ESM-only, needs compiled catalogs) | Poor |
| node-polyglot 2.6.0 | 7,423 B | BSD-2-Clause | 2024-07-11 | no (CommonJS with `require`) | Unsuitable |
| rosetta 1.1.0 | 420 B (with dependencies) | MIT | 2020-06-29 | **no**: the dist file calls `require("dlv")` (verifier correction) | Unsuitable |
| messageformat 4.1.0 / intl-messageformat 12.1.4 / @messageformat/core 3.4.1 | 8,233 / 9,990 / 19,685 B | Apache-2.0 / BSD-3 / MIT | n/a | partly | ICU or MF2 syntax is not needed; native `Intl.MessageFormat` is only TC39 Stage 1 |

Sources: [bundlephobia i18next](https://bundlephobia.com/api/size?package=i18next) (one such query per package), [i18next fallback docs](https://www.i18next.com/principles/fallback), [typesafe-i18n](https://github.com/codingcommons/typesafe-i18n), [rosetta dist](https://unpkg.com/rosetta@1.1.0/dist/index.min.js), [polyglot index.js](https://raw.githubusercontent.com/airbnb/polyglot.js/master/index.js), [lingui core](https://lingui.dev/ref/core), [tc39 MessageFormat](https://github.com/tc39/proposal-intl-messageformat).

**Opinion:** the no-library helper is the clear choice. Following the build-on-open-source rule, I checked whether a library fits, and none does. The ones that work without a bundler bring 1 to 20 KB gz of features this UI does not need. The helper is trivial glue, and the pattern is proven: typesafe-i18n's [detect.mts](https://raw.githubusercontent.com/codingcommons/typesafe-i18n/main/packages/detectors/src/detect.mts), LuCI's two-line lookup.

---

## 4. Delivering packs

| | (a) Bundled in LittleFS | (b) Browser fetch from GitHub | (c) User uploads to device | (d) Device downloads itself |
|---|---|---|---|---|
| **App-slot flash** | 0 (only the `t()` helper in v2.js) | 0, plus about 100 B per language for URL and SRI hash | 0, plus one small upload endpoint | TLS client: large flash and RAM cost, and **against the no-HTTPS rule** |
| **LittleFS** | about 13 to 16 KB gz per language (estimate, see 6.6) | 0 | about the same as (a), per installed pack | about the same as (a) |
| **Offline / AP mode** | Works | Fails; English is shown | Works after the upload | n/a |
| **Privacy** | No third party | The CDN sees the public IP and time. With the default Referrer-Policy the LAN origin `http://192.168.x.y/` is sent as Referer, unless `referrerPolicy:'no-referrer'` is set ([W3C Referrer Policy](https://w3c.github.io/webappsec-referrer-policy/)) | No third party | A third party sees the device |
| **Version match** | Exact: built from the same commit | Exact **if** the URL is pinned to the firmware's commit or tag and the SRI hash comes from the build | Needs a key-set hash in the pack | n/a |
| **Failure mode** | None at runtime; costs LittleFS budget | Silent English fallback. Builds that were never pushed (local) or a blocked CDN give English | User installs the wrong or an old pack; per-key fallback covers it | n/a |

**Facts for (b)**, measured with curl and an Origin header on 2026-11-07 by several reports and re-measured by the verifiers. These come from one vantage point (Frankfurt) on one date.

- **jsDelivr** `cdn.jsdelivr.net/gh/<o>/<r>@<ref>/<file>` sends `Access-Control-Allow-Origin: *`, and the preflight passes.
  - Tag- or commit-pinned URL: `max-age=31536000, immutable`.
  - Branch URL: browser `max-age=604800` (7 days), CDN `s-maxage=43200` (12 h). Branch URLs therefore go stale in the browser for up to a week.
  - Files up to 20 MB "by default" ([README](https://raw.githubusercontent.com/jsdelivr/jsdelivr/master/README.md)).
  - **Best host.**
- **raw.githubusercontent.com** sends ACAO `*`, but:
  - `Content-Type: text/plain` and `max-age=300`.
  - The OPTIONS preflight returns 403, so only simple requests work.
  - Rate limits for unauthenticated users exist but have no published number ([changelog 2025-05-08](https://github.blog/changelog/2025-05-08-updated-rate-limits-for-unauthenticated-requests/)).
  - Source conflict, resolved: one report said raw serves uncompressed. The verifier showed it gzips when the client sends `Accept-Encoding`, which every browser does.
- **GitHub release assets cannot be used from a browser.** Neither the 302 from github.com nor the 200 from `release-assets.githubusercontent.com` carries ACAO. The API asset route ends at the same host. This was checked with curl HEAD and a ranged GET, not with a real browser fetch.
- **api.github.com** has CORS, but allows only 60 unauthenticated requests per hour per IP ([docs](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api)). Unsuitable as the pack path.
- **Mixed content does not block this.** MDN: "loading https://secure.com into http://insecure.com is not a mixed content request" ([MDN](https://developer.mozilla.org/en-US/docs/Web/Security/Mixed_content), [W3C](https://www.w3.org/TR/mixed-content/)). Chrome's Local Network Access only restricts public-to-local requests ([blog](https://developer.chrome.com/blog/local-network-access)).
- **`http://<LAN IP>` is not a secure context.** The Cache API, service workers and `crypto.subtle` are therefore unavailable on the device page; use the HTTP cache plus localStorage.
  - Source conflict: MDN's list of [restricted features](https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts/features_restricted_to_secure_contexts) does not name the Cache API, while the [CacheStorage page](https://developer.mozilla.org/en-US/docs/Web/API/CacheStorage) says it is restricted. I follow the specific page.
- **SRI via `fetch(url, {integrity})`** works cross-origin with CORS and needs no secure context ([MDN RequestInit](https://developer.mozilla.org/en-US/docs/Web/API/RequestInit), [W3C SRI](https://www.w3.org/TR/SRI/)). On plain HTTP it protects against a compromised CDN, not against a man-in-the-middle on the LAN.
- **Not verified:** whether Home Assistant ingress sets a Content-Security-Policy (`connect-src`) that blocks a fetch to jsDelivr. This must be tested in the HA ingress rig before (b) is relied on.

**Facts for (d):** `http://raw.githubusercontent.com` and `http://cdn.jsdelivr.net` both answer 301 to HTTPS. Without TLS the device cannot download from GitHub. The no-TLS route is a **browser relay**: the browser fetches from jsDelivr, the SRI check passes, and the browser POSTs the pack to a device endpoint that writes it to LittleFS. That endpoint falls under the ADR-056 auth and CSRF rules.

**Opinion: combine the options.** Use (a) for NL and DE. Use (b) for other languages. Use (c) as the "install on device" relay built on (b), which also covers offline users.

---

## 5. Translation workflow

| Platform | Cost / licence | Repo integration | Fit |
|---|---|---|---|
| **Hosted Weblate** (libre plan) | Free for public libre projects, up to 160,000 hosted strings. Software GPLv3+, latest weblate-2026.11.1 (2026-11-05) | GitHub App opens PRs. Formats `json` (flat, no plurals), `json-nested`, `i18nextv4` (CLDR plurals). Add-on "Remove blank strings" keeps untranslated keys out, so English fallback works. CDN add-on publishes a `manifest.json` with sha256 and size per file. Warns: **do not squash-merge** its PRs | **Recommended.** OpenWrt LuCI uses it. OTGW-firmware is public GPL-3.0, so it qualifies |
| Crowdin | Free for OSS **only if** "you do not have any commercial products related to the open-source project"; translations go into the Crowdin Global TM; proprietary | Service-branch PRs (`l10n_` prefix), or crowdin/github-action v3.6.0 (MIT) | Eligibility is doubtful because of NodoShop hardware sales |
| Tolgee | Open core: Apache-2.0 plus a non-commercial EE licence. Cloud free plan: 30,000 words, 3 seats | CLI push/pull; PR automation is up to you. `.` is the default key nesting delimiter | Workable, more setup |
| Pontoon (Mozilla) | BSD-3, self-host | Git/Hg/SVN sync; supports key-value JSON (verifier correction) | Self-hosting effort; poor fit |
| Plain JSON via PRs | Free | Native | Fine for the maintainer (NL) and Git-savvy helpers; a barrier for typical NL/BE/DE users |

Size check: about 630 strings x (3 languages + 1) = 2,520 hosted strings, about 1.6% of the libre cap. That figure was corrected from an arithmetic error in one report.

**Missing-key check in CI:** [lingualdev/i18n-check](https://github.com/lingualdev/i18n-check) v0.9.5 (MIT) reports missing keys and also placeholder mismatches for i18next and ICU syntax (verifier correction). With the `{name}` syntax proposed below, a small Python check in `tests/` is simpler and fits `scripts/run_tests.py`. **Opinion:** report coverage per language (WARN), and fail on a placeholder mismatch.

**Recommendation for NL and DE first:**
1. Robert writes NL himself, or reviews a machine-translated draft, since he is a native speaker.
2. For DE, ask the community through Weblate, with one named DE reviewer.
3. Keep the catalogs at the repo root (`i18n/`), not under `src/OTGW-firmware/data/`. Weblate bot commits under `src/**` would trip the `_VERSION_PRERELEASE` rule, which a bot cannot meet, and client-side hooks do not run on bot commits. build.py copies and gzips the packs into the LittleFS image at build time.
4. Merge Weblate PRs with a merge commit.

Sources: [weblate.org/hosting](https://weblate.org/en/hosting/), [Weblate JSON](https://docs.weblate.org/en/latest/formats/json.html), [add-ons](https://docs.weblate.org/en/latest/admin/addons.html), [continuous](https://docs.weblate.org/en/latest/admin/continuous.html), [Crowdin OSS](https://crowdin.com/page/open-source-project-setup-request), [Tolgee EE licence](https://raw.githubusercontent.com/tolgee/tolgee-platform/main/ee/LICENSE), [Tolgee pricing](https://tolgee.io/pricing), [Pontoon formats](https://github.com/mozilla/pontoon/tree/main/pontoon/sync/tests/formats).

---

## 6. Design sketch for OTGW (opinion, built on the patterns above)

### 6.1 Keys and source of truth
- **Flat dotted keys** grouped by screen, for example `nav.settings`, `status.boiler.flame`, `settings.mqtt.broker.label`, `help.MQTTbroker`. Flat files match the Weblate `json` format and keep diffs readable.
- **Placeholders** are `{name}`. **Plurals** use the suffixes `key_one` / `key_other`, chosen with `new Intl.PluralRules(lang).select(n)`.
- **`i18n/en.json` in the repo is the English source of truth.** build.py checks that every `t('...')` key used in v2.js and v2.html exists in en.json, and that en.json has no unused keys. It then inlines en.json into v2.js as the built-in catalog, so English cost stays about what it is today.
- HTML: `data-i18n="key"` (and `data-i18n-title`, `data-i18n-placeholder`) attributes, plus one `applyI18n(root)` pass after each render. Text built in JS calls `t()` directly. This avoids WLED #4274's fragile DOM text replacement.
- **Not translated:** REST field names, MQTT topics and payloads, HA discovery names, log lines and OpenTherm MsgID names (decision for Robert, see 7.6).

### 6.2 How v2.js picks the language
```
1. user override   -> setting (see question 7.3), else localStorage 'otgw.lang'
2. navigator.languages, in order: exact tag, then base tag (nl-BE -> nl, de-AT -> de)
3. 'en'
```
- Namespace the localStorage key (`otgw.lang`). Under HA ingress, localStorage belongs to the HA origin.
- Safari exposes only one entry in `navigator.languages` (MDN note, **unchecked**), so a visible language picker is needed.
- Lookup: `pack[key] ?? EN[key] ?? key`. The fallback is **per key**, never per pack.

### 6.3 Loading order for a non-English language
1. `GET /i18n/<lang>.json` from LittleFS (bundled or installed).
2. If that fails and remote packs are allowed: `fetch(jsdelivr/<repo>@<githash>/i18n/<lang>.json, {integrity, referrerPolicy:'no-referrer', credentials:'omit'})`.
3. If that fails too: stay on English and show a small notice.

This is a one-off fetch at startup. It counts toward the device's cap of 2 API calls in flight only when it goes to the device. Tie the jsDelivr ref to the firmware githash, which is already in v2.js as the cache key. jsDelivr serves commit URLs immutably. Builds that were never pushed will 404 and fall back to English, which is correct.

### 6.4 Settings help texts
- Today: about 18 KB of English JSON in PROGMEM (`settingsHelp.h`: 17,683 B raw, 6,766 B gz, measured).
- Proposal: move the help into the packs as `help.<settingName>` keys. Keep English either in PROGMEM (no change) or move it to a LittleFS `en` help file. Moving it frees about 18 KB of app flash.
- Never add translated help to PROGMEM. EMS-ESP's forced `EN_ONLY` on 4 MB boards is the warning.

### 6.5 Versioning a pack against the firmware
- build.py computes `keyset = sha256(sorted English keys + their placeholder lists)[:12]` and compiles it into v2.js.
- Each pack carries `{"_meta": {"lang": "nl", "keyset": "...", "source": "<githash>", "coverage": 0.97}}`.
- On load: if the keyset matches, use the pack. If it does not match, still use it per key, log it in the console and show "translation partly out of date". Never reject a pack; ESP3D does the same.
- For remote packs, the SRI hash in the firmware gives exact binding on top of the keyset.
- The CI check (section 5) reports coverage per language, so drift is visible before a release, unlike OpenDTU-OnBattery.

### 6.6 Size estimate per language (an estimate, not a measurement of a real pack)

Measured locally on 2026-11-07 with a rough regex pass over the current files:
- v2.js: 251 literals that look like UI text, 6,089 chars.
- v2.html: 253 text nodes (5,364 chars) and 51 title/placeholder/aria attributes (1,493 chars).
- Combined: about 13 KB of text, **5.9 KB gz**.

The heuristic undercounts: template literals, short labels and messages built by concatenation are missed. Settings help is 6.8 KB gz.

**Estimate:** UI strings plus keys come to about 7 to 10 KB gz per language (similar to EMS-ESP's ~6 KB and OpenDTU's ~10 KB). With settings help, that gives **about 13 to 16 KB gz per language**, so **about 26 to 32 KB of LittleFS for NL + DE**. That fits the 340 KiB budget, but it is not free. A real string inventory of v2.js gives the true number.

---

## 7. Risks and open questions for the maintainer

1. **NL and DE: bundled or fetched?**
   - Bundle them on LittleFS: they always work, offline too, and cost about 26 to 32 KB of LittleFS.
   - Fetch from GitHub only: 0 bytes, but they fail in AP mode, on offline LANs and possibly under HA ingress, where those users see English.
   - Recommendation: bundle NL and DE, and fetch the rest.
2. **Remote fetch: on by default or opt-in?**
   - On by default: works without effort, but the CDN sees the user's IP, and the LAN origin unless `no-referrer` is set. Today the UI makes no third-party calls at all.
   - Opt-in (a setting, or an explicit "download language" button): privacy-clean, one extra click.
   - Recommendation: opt-in, through the "install on device" relay.
3. **Where does the language choice live?**
   - Per browser (localStorage): no firmware setting needed, but every browser and the HA ingress view guess separately.
   - Device setting: follows the user everywhere, but it is one more setting and every viewer of the device gets the same language.
   - Recommendation: device setting `ui.sLanguage` (default `auto`), with an optional per-browser override.
4. **Settings help: move to LittleFS?**
   - Move it: frees about 18 KB of app flash and makes translated help possible.
   - Keep it in PROGMEM: no risk, but help stays English only.
   - Recommendation: move it with the first NL pack.
5. **Translation platform.**
   - Hosted Weblate: free, non-coders can contribute, needs merge commits and catalogs outside `src/`.
   - PR-only: zero tooling, but every translator needs Git.
   - Recommendation: Weblate, starting at the same time as NL and DE.
6. **Scope of translation.**
   - UI only: matches Tasmota; MQTT, HA and REST stay stable.
   - Also HA entity names: users would see Dutch names in HA, but changing language would rename entities, which breaks automations.
   - Recommendation: UI only.
7. **HA ingress CSP (blocking, unverified).** If ingress blocks `connect-src` to jsDelivr, remote packs do not work there and only the bundled or installed packs work. This needs a test in the HA ingress rig before (b) is promised.
8. **Process.** The `t()` and pack mechanism is a new pattern with a build-tooling change (build.py). Under the project rules it needs an ADR via adr-kit. The coverage check should be its CI gate (ADR-080).

**Not verified anywhere:**
- A real browser `fetch()` against GitHub release assets (curl only).
- HA ingress CSP.
- The real size of an OTGW pack.
- OpenBeken's "virtually free" quote.
- Fluidd's use of Weblate.

---

## 8. Sources

**Section 2: firmwares**
- EMS-ESP32: https://github.com/emsesp/EMS-ESP32/blob/9a21d71df369e02321a79d0e8bc3578bb28d6816/interface/src/App.tsx · .../interface/progmem-generator.js · .../interface/package.json · .../interface/.typesafe-i18n.json · .../interface/src/components/inputs/LanguageSelector.tsx · .../scripts/build_interface.py · .../.github/workflows/dev_release.yml · .../.github/workflows/stable_release.yml · .../src/core/locale_translations.h · .../src/core/emsesp_common.h · .../platformio.ini · https://emsesp.org/Adding-Languages/
- OpenDTU: https://github.com/tbnobody/OpenDTU/blob/8da432d52f33a7598deb93550df2115b8e5bb700/webapp/src/i18n.ts · .../src/I18n.cpp · .../src/WebApi_i18n.cpp · .../src/WebApi_file.cpp · .../webapp/src/views/ConfigAdminView.vue · .../include/defaults.h · .../lang/README.md · .../webapp/vite.config.ts · https://github.com/tbnobody/OpenDTU/releases/tag/v26.9.28
- OpenDTU-OnBattery: https://github.com/hoylabs/OpenDTU-OnBattery/tree/cd00fa550d4713792886f35f7f263679d935376e/webapp/src/locales · .../lang · https://github.com/hoylabs/OpenDTU-OnBattery/commits/development/lang
- AhoyDTU: https://github.com/lumapu/ahoy/blob/c4fe30f118bd334afa548d71b56018b73dafd6cb/scripts/convertHtml.py · .../src/web/lang.json · https://github.com/lumapu/ahoy/releases/tag/ahoy_v0.8.156
- Tasmota: https://raw.githubusercontent.com/arendst/Tasmota/development/tasmota/my_user_config.h · https://raw.githubusercontent.com/arendst/Tasmota/development/tasmota/language/nl_NL.h · https://raw.githubusercontent.com/arendst/Tasmota/development/platformio_tasmota_env.ini · https://raw.githubusercontent.com/arendst/Tasmota/development/platformio_tasmota_env32.ini · https://api.github.com/repos/arendst/Tasmota/releases/latest · https://ota.tasmota.com/tasmota/release/ · https://github.com/arendst/Tasmota/issues/1367 · https://github.com/arendst/Tasmota/issues/1473
- ESPHome: https://esphome.io/components/web_server/ · https://raw.githubusercontent.com/esphome/esphome/dev/esphome/components/web_server/web_server.cpp · https://github.com/esphome/esphome-webserver · https://developers.esphome.io/contributing/translations/
- WLED: https://github.com/wled/WLED/issues/459 · https://github.com/wled/WLED/pull/4274 · https://github.com/wled/WLED/pull/5671 · https://api.github.com/repos/wled/WLED/issues/5671/comments?per_page=100
- ESP3D: https://esp3d.io/esp3d-webui/version_3x/documentation/translation/
- OpenBeken (partly unverified): https://github.com/OpenBekenIOT/webapp · https://github.com/spinpwr/OpenBekenIOT_webapp · https://raw.githubusercontent.com/openshwprojects/OpenBK7231T_App/main/docs/commands.md
- ESPEasy: https://raw.githubusercontent.com/letscontrolit/ESPEasy/mega/src/src/WebServer/RootPage.cpp
- LuCI: https://raw.githubusercontent.com/openwrt/luci/master/luci.mk · https://raw.githubusercontent.com/openwrt/luci/master/modules/luci-base/htdocs/luci-static/resources/cbi.js · https://raw.githubusercontent.com/openwrt/luci/master/modules/luci-base/ucode/dispatcher.uc · https://downloads.openwrt.org/releases/24.10.4/packages/x86_64/luci/
- OctoPrint: https://raw.githubusercontent.com/OctoPrint/OctoPrint/master/src/octoprint/server/api/languages.py · https://raw.githubusercontent.com/OctoPrint/OctoPrint/master/src/octoprint/server/__init__.py · https://docs.octoprint.org/en/main/_sources/api/languages.rst.txt
- Home Assistant: https://raw.githubusercontent.com/home-assistant/frontend/dev/build-scripts/gulp/translations.js · https://raw.githubusercontent.com/home-assistant/frontend/dev/src/util/common-translation.ts · https://raw.githubusercontent.com/home-assistant/core/dev/homeassistant/helpers/translation.py
- Grafana: https://raw.githubusercontent.com/grafana/grafana/main/packages/grafana-i18n/src/i18n.tsx · https://raw.githubusercontent.com/grafana/grafana/main/public/app/core/internationalization/loadTranslations.ts
- Mainsail / Fluidd: https://raw.githubusercontent.com/mainsail-crew/mainsail/develop/src/plugins/i18n.ts · https://docs.mainsail.xyz/development/localization/ · https://raw.githubusercontent.com/fluidd-core/fluidd/develop/src/plugins/i18n.ts · https://raw.githubusercontent.com/fluidd-core/fluidd/develop/src/dynamicImports.ts

**Section 3: libraries**
- https://bundlephobia.com/api/size?package=i18next (and the same query for typesafe-i18n, rosetta, node-polyglot, @lingui/core, @fluent/bundle, messageformat, @messageformat/core, intl-messageformat)
- https://registry.npmjs.org/i18next · https://www.i18next.com/principles/fallback · https://www.i18next.com/translation-function/plurals · https://raw.githubusercontent.com/i18next/i18next/master/CHANGELOG.md
- https://github.com/codingcommons/typesafe-i18n · https://raw.githubusercontent.com/codingcommons/typesafe-i18n/main/packages/detectors/src/detect.mts
- https://unpkg.com/rosetta@1.1.0/dist/index.min.js · https://raw.githubusercontent.com/airbnb/polyglot.js/master/index.js · https://lingui.dev/ref/core · https://github.com/projectfluent/fluent.js/tree/main/fluent-bundle
- https://github.com/tc39/proposal-intl-messageformat · https://github.com/tc39/proposal-intl-localematcher
- https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/PluralRules · https://developer.mozilla.org/en-US/docs/Web/API/Navigator/languages

**Section 4: delivery**
- https://raw.githubusercontent.com/jsdelivr/jsdelivr/master/README.md · https://cdn.jsdelivr.net/gh/rvdbreemen/OTGW-firmware@v1.7.9/README.md · https://cdn.jsdelivr.net/gh/rvdbreemen/OTGW-firmware@dev/README.md
- https://raw.githubusercontent.com/rvdbreemen/OTGW-firmware/dev/README.md · https://github.blog/changelog/2025-05-08-updated-rate-limits-for-unauthenticated-requests/
- https://github.com/rvdbreemen/OTGW-firmware/releases/download/v1.7.9/capture-otgw.sh · https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases/assets/708523616
- https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api · https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api
- https://developer.mozilla.org/en-US/docs/Web/Security/Mixed_content · https://www.w3.org/TR/mixed-content/ · https://developer.chrome.com/blog/local-network-access
- https://www.w3.org/TR/secure-contexts/ · https://developer.mozilla.org/en-US/docs/Web/API/CacheStorage · https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts/features_restricted_to_secure_contexts
- https://developer.mozilla.org/en-US/docs/Web/API/RequestInit · https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity · https://www.w3.org/TR/SRI/
- https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Referrer-Policy · https://w3c.github.io/webappsec-referrer-policy/

**Section 5: translation platforms**
- https://weblate.org/en/hosting/ · https://docs.weblate.org/en/latest/formats/json.html · https://docs.weblate.org/en/latest/formats/i18next.html · https://docs.weblate.org/en/latest/admin/addons.html · https://docs.weblate.org/en/latest/admin/continuous.html · https://docs.weblate.org/en/latest/admin/code-hosting.html · https://github.com/WeblateOrg/weblate/issues/5564 · https://api.github.com/repos/rvdbreemen/OTGW-firmware
- https://crowdin.com/page/open-source-project-setup-request · https://support.crowdin.com/github-integration/ · https://github.com/crowdin/github-action
- https://raw.githubusercontent.com/tolgee/tolgee-platform/main/ee/LICENSE · https://tolgee.io/pricing · https://docs.tolgee.io/tolgee-cli/push-pull-strings
- https://github.com/mozilla/pontoon · https://github.com/mozilla/pontoon/tree/main/pontoon/sync/tests/formats
- https://github.com/lingualdev/i18n-check · https://github.com/openwrt/luci · https://contributors.home-assistant.io/translate/

**Section 6: local measurement**
- `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\src\OTGW-firmware\data\v2.js` (407,854 B)
- `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\src\OTGW-firmware\data\v2.html` (50,937 B)
- `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware\src\OTGW-firmware\settingsHelp.h` (17,885 B)

These were read-only regex counts, run 2026-11-07.