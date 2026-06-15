// HA-discovery golden-file test harness (TASK-648).
//
// PURPOSE
//   Given captured HA-discovery MQTT dumps (text files of lines in the form
//   `topic<TAB>payload`, one per line), assert:
//     (a) LEGACY MODE: the dump is byte-for-byte equal to the committed
//         golden fixture.
//     (b) MODERN MODE: the dump uses a single HA device (ADR-140), all
//         non-BLE unique_ids carry a source-engine prefix (esp_/pic_/otd_/
//         sat_/sensors_), any via_device present nests under the bare nodeId,
//         and bilateral labels appear under both _boiler and _thermostat
//         suffixes. BLE sensors are kept as their own child HA devices
//         (identifier "<uniqueId>_ble_<mac>", via_device == nodeId) and are
//         carved out of the single-device and prefix assertions.
//
// CAPTURE PROCEDURE
//   On a live device running discovery-republish mode, subscribe to
//   `homeassistant/#` with a retaining MQTT client and redirect output to a
//   text file, one line per message: `topic\tpayload` (tab-separated).
//   Example with mosquitto_sub:
//     mosquitto_sub -h <broker> -t 'homeassistant/#' -v \
//       | awk '{ topic=$1; $1=""; print topic "\t" substr($0,2) }' \
//       > modern-dump.txt
//   For legacy mode capture, use the same command against a device running
//   the legacy single-device discovery code, saving to legacy-dump.txt.
//
// ENV VARS
//   LEGACY_DUMP     Path to the captured legacy-mode dump file.
//   LEGACY_GOLDEN   Path to the committed golden fixture for legacy mode.
//                   When both are set, the dump must match the golden byte-
//                   for-byte (after normalising line endings to LF).
//
//   MODERN_DUMP     Path to the captured modern-mode dump file.
//   NODE_ID         The device node_id prefix used in HA discovery (e.g.
//                   "otgw-abc123"). When both MODERN_DUMP and NODE_ID are set,
//                   the single-device and unique_id-prefix assertions run.
//
//   BILATERAL_CSV   Optional comma-separated list of firmware sensor labels
//                   (e.g. "TSet,TBoiler"). When set, each label must appear in a
//                   uniq_id ending in "_boiler" AND a uniq_id ending in
//                   "_thermostat". Omit or leave empty to skip this check.
//
// USAGE
//   node tests/webui/ha-discovery-golden.test.mjs
//   # or with env vars:
//   LEGACY_DUMP=captures/legacy.txt LEGACY_GOLDEN=tests/fixtures/ha-discovery-legacy-golden.txt \
//   MODERN_DUMP=captures/modern.txt NODE_ID=otgw-abc123 BILATERAL_CSV=TSet,TBoiler \
//   node tests/webui/ha-discovery-golden.test.mjs
//
// EXIT CODES
//   0  All executed assertions passed (SKIPs do not fail).
//   1  One or more assertions failed.

import fs from 'node:fs';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

let ok = true;

/**
 * Log a single assertion result.
 * @param {boolean|null} pass  true=PASS, false=FAIL, null=SKIP
 * @param {string} msg         Human-readable assertion label
 */
function log(pass, msg) {
  if (pass === null) {
    console.log('SKIP: ' + msg);
  } else if (pass) {
    console.log('PASS: ' + msg);
  } else {
    console.log('FAIL: ' + msg);
    ok = false;
  }
}

/**
 * Read a file and return its contents normalised to LF line endings.
 * Returns null (with a console error) if the file cannot be read.
 * @param {string} envName  Name of the env var that holds the path (for error messages)
 * @param {string} filePath
 * @returns {string|null}
 */
function readFile(envName, filePath) {
  try {
    return fs.readFileSync(filePath, 'utf8').replace(/\r\n/g, '\n');
  } catch (e) {
    console.error(`ERROR: cannot read ${envName}=${filePath}: ${e.message}`);
    return null;
  }
}

/**
 * Parse a dump file into an array of { topic, payload } objects.
 * Lines that do not contain a TAB separator are silently skipped.
 * @param {string} text  Normalised (LF) file content
 * @returns {{ topic: string, payload: string }[]}
 */
function parseDump(text) {
  return text.split('\n')
    .filter(line => line.includes('\t'))
    .map(line => {
      const idx = line.indexOf('\t');
      return { topic: line.slice(0, idx), payload: line.slice(idx + 1) };
    });
}

// ---------------------------------------------------------------------------
// Block A: Legacy golden-file identity
// ---------------------------------------------------------------------------

console.log('\n--- Block A: Legacy mode byte-identity ---');

const legacyDumpPath   = process.env.LEGACY_DUMP;
const legacyGoldenPath = process.env.LEGACY_GOLDEN;

if (!legacyDumpPath && !legacyGoldenPath) {
  log(null, 'Legacy identity check (LEGACY_DUMP and LEGACY_GOLDEN not set)');
} else if (!legacyDumpPath) {
  log(null, 'Legacy identity check (LEGACY_DUMP not set)');
} else if (!legacyGoldenPath) {
  log(null, 'Legacy identity check (LEGACY_GOLDEN not set)');
} else {
  const dump   = readFile('LEGACY_DUMP',   legacyDumpPath);
  const golden = readFile('LEGACY_GOLDEN', legacyGoldenPath);

  if (dump === null || golden === null) {
    log(false, 'Legacy identity check (file read error — see ERROR above)');
  } else {
    const match = dump === golden;
    if (!match) {
      // Show a brief diff hint: first differing line
      const dLines = dump.split('\n');
      const gLines = golden.split('\n');
      let firstDiff = -1;
      for (let i = 0; i < Math.max(dLines.length, gLines.length); i++) {
        if (dLines[i] !== gLines[i]) { firstDiff = i + 1; break; }
      }
      console.log(`  hint: first difference at line ${firstDiff}`);
      console.log(`  dump lines: ${dLines.length}, golden lines: ${gLines.length}`);
    }
    log(match, `Legacy dump matches golden fixture (${legacyDumpPath} vs ${legacyGoldenPath})`);
  }
}

// ---------------------------------------------------------------------------
// Block B: Modern mode SINGLE-device topology + uniq_id prefixes + via_device
// nests under nodeId (ADR-140). BLE child devices are carved out (ADR-140).
// ---------------------------------------------------------------------------

console.log('\n--- Block B: Modern mode single-device assertions (ADR-140) ---');

const modernDumpPath = process.env.MODERN_DUMP;
const nodeId         = process.env.NODE_ID;

// ADR-140 single-device topology (supersedes ADR-124 seven-device). Everything
// the gateway exposes is ONE HA device whose identifier is the bare nodeId. The
// seven former device groupings survive only as uniq_id source-prefixes:
//   esp_       -> ESP diagnostics
//   pic_/otd_  -> the active OT engine (Boiler/Thermostat/Gateway/OtCore all
//                 route through pic_ on a PIC build or otd_ on an OTDirect build)
//   sat_       -> SAT entities
//   sensors_   -> Dallas/S0 sensor entities
// BLE sensors are the one deliberate exception: each stays its own child HA
// device ("<uniqueId>_ble_<mac>", via_device == nodeId, uniq_id
// "<uniqueId>_ble_<mac>_<kind>") and is carved out of B-1/B-2 below.
// CORE_SUFFIXES: the bilateral suffixes a single OT value reported by both sides
// yields (one uniq_id ending "_boiler", one ending "_thermostat"); no second
// device is created. (Block C consumes this.)
const CORE_SUFFIXES     = ['_boiler', '_thermostat'];
// DEVICE_SEG_ALT: the source-prefix alternation used by the uniq_id / bilateral
// regexes. The trailing underscore in every prefix is load-bearing.
const DEVICE_SEG_ALT    = '(esp_|pic_|otd_|sat_|sensors_)';
// FORBIDDEN_ID_SEG: identifier suffixes that must NOT appear in single-device
// mode. Anchored with `$` at the use site so "ot-direct" is rejected without the
// alternation-ordering trap the old (ot-direct|...|ot) ordering needed.
const FORBIDDEN_ID_SEG  = '(boiler|thermostat|gateway|esp|pic|ot-direct|sat|sensors)';

// Reusable: escape nodeId for safe interpolation into a RegExp.
const escNodeId = nodeId ? nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&') : '';

if (!modernDumpPath && !nodeId) {
  log(null, 'Modern mode checks (MODERN_DUMP and NODE_ID not set)');
} else if (!modernDumpPath) {
  log(null, 'Modern mode checks (MODERN_DUMP not set)');
} else if (!nodeId) {
  log(null, 'Modern mode checks (NODE_ID not set)');
} else {
  const modernText = readFile('MODERN_DUMP', modernDumpPath);

  if (modernText === null) {
    log(false, 'Modern mode checks (file read error — see ERROR above)');
  } else {
    const entries = parseDump(modernText);
    log(entries.length > 0, `Parsed ${entries.length} entries from modern dump`);

    // B-1: ADR-140 single device. Collect every distinct "identifiers" value.
    // (kIds expands to the full word "identifiers" in firmware, so this matches.)
    const identifierRe = /"identifiers"\s*:\s*"([^"]+)"/g;
    const foundDevices = new Set();
    for (const { payload } of entries) {
      let m;
      // Reset lastIndex per payload since we reuse the regex
      identifierRe.lastIndex = 0;
      while ((m = identifierRe.exec(payload)) !== null) {
        foundDevices.add(m[1]);
      }
    }

    // B-1 positive: ADR-140 single device, with a BLE carve-out. BLE sensors are
    // legitimately their own child HA devices, each with an identifier of the form
    // "<uniqueId>_ble_<mac>" nested under the single device via via_device. So the
    // gateway itself must contribute exactly ONE non-`_ble_` identifier and it must
    // equal the bare nodeId; any number of `_ble_` child identifiers may coexist.
    const nonBleDevices = [...foundDevices].filter((id) => !/_ble_/.test(id));
    log(
      nonBleDevices.length === 1 && nonBleDevices[0] === nodeId,
      `Exactly one non-BLE device identifier, equal to bare nodeId "${nodeId}" (non-BLE found: ${nonBleDevices.join(', ') || 'none'}; BLE children allowed)`
    );

    // B-1 negative: NO multi-device suffix identifiers leaked. The `$` anchor
    // means a bare "<nodeId>-ot-direct" etc. is rejected outright.
    const forbiddenIdRe = new RegExp(`^${escNodeId}-${FORBIDDEN_ID_SEG}$`);
    const leakedDevices = [...foundDevices].filter((id) => forbiddenIdRe.test(id));
    if (leakedDevices.length > 0) {
      console.log(`  hint: leaked per-device identifiers: ${leakedDevices.join(', ')}`);
    }
    log(
      leakedDevices.length === 0,
      `No "${nodeId}-(boiler|thermostat|gateway|esp|pic|ot-direct|sat|sensors)" identifiers present (leaked: ${leakedDevices.length})`
    );

    // B-2: ADR-140 — every uniq_id / unique_id matches ^{nodeId}-<source-prefix>
    // where the prefix already carries its trailing underscore (esp_, pic_, ...).
    // BLE carve-out: BLE child entities use uniq_ids of the form
    // "<uniqueId>_ble_<mac>_<kind>" which do not carry a source-engine prefix;
    // they are exempt from this naming convention (skipped, not failed).
    const uniqIdPattern = new RegExp(`^${escNodeId}-${DEVICE_SEG_ALT}`);
    const uniqIdRe = /"(?:uniq_id|unique_id)"\s*:\s*"([^"]+)"/g;

    let totalUniqIds    = 0;
    let badUniqIds      = 0;
    const badExamples   = [];

    for (const { payload } of entries) {
      uniqIdRe.lastIndex = 0;
      let m;
      while ((m = uniqIdRe.exec(payload)) !== null) {
        if (/_ble_/.test(m[1])) continue;  // BLE child entity — exempt
        totalUniqIds++;
        if (!uniqIdPattern.test(m[1])) {
          badUniqIds++;
          if (badExamples.length < 3) badExamples.push(m[1]);
        }
      }
    }

    if (totalUniqIds === 0) {
      log(null, 'uniq_id naming convention (no uniq_id/unique_id keys found in dump — check dump contents)');
    } else {
      if (badUniqIds > 0) {
        console.log(`  hint: ${badUniqIds} non-conforming uniq_ids — examples: ${badExamples.join(', ')}`);
      }
      log(
        badUniqIds === 0,
        `All ${totalUniqIds} uniq_id/unique_id values match ^${nodeId}-${DEVICE_SEG_ALT}`
      );
    }

    // B-3: ADR-140 — the gateway is a single device, so the only legitimate
    // via_device is the one BLE child entities use to nest under it: every
    // via_device value MUST equal the bare nodeId. Zero via_device keys also
    // passes (a build with no BLE sensors). The check FAILS only if a via_device
    // points somewhere other than nodeId (e.g. a stale ADR-124 per-device parent).
    const viaDeviceRe = /"via_device"\s*:\s*"([^"]+)"/g;
    let viaTotal = 0;
    let viaForeign = 0;
    const viaForeignExamples = [];
    for (const { payload } of entries) {
      viaDeviceRe.lastIndex = 0;
      let m;
      while ((m = viaDeviceRe.exec(payload)) !== null) {
        viaTotal++;
        if (m[1] !== nodeId) {
          viaForeign++;
          if (viaForeignExamples.length < 3) viaForeignExamples.push(m[1]);
        }
      }
    }
    if (viaForeign > 0) console.log(`  hint: foreign via_device values: ${viaForeignExamples.join(', ')}`);
    log(viaForeign === 0, `Every via_device equals bare nodeId "${nodeId}" (total ${viaTotal}, foreign ${viaForeign})`);

    // ---------------------------------------------------------------------------
    // Block C: Bilateral sensor check (optional)
    // ---------------------------------------------------------------------------

    console.log('\n--- Block C: Bilateral sensor label check ---');

    const bilateralCsv = process.env.BILATERAL_CSV;
    if (!bilateralCsv || bilateralCsv.trim() === '') {
      log(null, 'Bilateral sensor check (BILATERAL_CSV not set or empty)');
    } else {
      const labels = bilateralCsv.split(',').map(s => s.trim()).filter(Boolean);

      // ADR-140: bilateral entities share one device; they are distinguished by a
      // trailing _boiler / _thermostat SUFFIX on the uniq_id. The shape is
      //   <nodeId>-<source-prefix><label><suffix>
      // so strip the "<nodeId>-<prefix>" head, then test for the suffix and the
      // label in the remaining body. deviceSegRe captures (prefix, body).
      const deviceSegRe = new RegExp(
        `^${escNodeId}-${DEVICE_SEG_ALT}(.+)$`
      );

      for (const label of labels) {
        const owningSuffixes = new Set();

        for (const { payload } of entries) {
          uniqIdRe.lastIndex = 0;
          let m;
          while ((m = uniqIdRe.exec(payload)) !== null) {
            const uniqId = m[1];
            const segMatch = deviceSegRe.exec(uniqId);
            if (segMatch) {
              const body = segMatch[2]; // e.g. "tset_boiler" or "TSet_thermostat"
              for (const suffix of CORE_SUFFIXES) {           // '_boiler', '_thermostat'
                if (!body.toLowerCase().endsWith(suffix)) continue;
                // labelPart = body with the suffix stripped (e.g. "tset")
                const labelPart = body.slice(0, body.length - suffix.length);
                if (labelPart.toLowerCase().startsWith(label.toLowerCase())) {
                  owningSuffixes.add(suffix);
                }
              }
            }
          }
        }

        const hasBoiler      = owningSuffixes.has('_boiler');
        const hasThermostat  = owningSuffixes.has('_thermostat');
        const exactlyBoth    = owningSuffixes.size === 2 && hasBoiler && hasThermostat;

        if (!exactlyBoth) {
          console.log(`  hint: label "${label}" found with suffixes: ${[...owningSuffixes].join(', ') || 'none'}`);
        }
        log(
          exactlyBoth,
          `Bilateral label "${label}" appears under both _boiler and _thermostat suffixes (found: ${[...owningSuffixes].join(', ') || 'none'})`
        );
      }
    }
  }
}

// If we never entered the modern block, still emit the Block C header notice
if (!modernDumpPath || !nodeId) {
  console.log('\n--- Block C: Bilateral sensor label check ---');
  log(null, 'Bilateral sensor check (skipped: MODERN_DUMP or NODE_ID not set)');
}

// ---------------------------------------------------------------------------
// Summary
// ---------------------------------------------------------------------------

console.log('');
console.log(ok ? 'RESULT: ALL PASS' : 'RESULT: FAILURES');
process.exitCode = ok ? 0 : 1;
