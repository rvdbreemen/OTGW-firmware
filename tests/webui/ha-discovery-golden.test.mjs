// HA-discovery golden-file test harness (TASK-648).
//
// PURPOSE
//   Given captured HA-discovery MQTT dumps (text files of lines in the form
//   `topic<TAB>payload`, one per line), assert:
//     (a) LEGACY MODE: the dump is byte-for-byte equal to the committed
//         golden fixture.
//     (b) MODERN MODE: the dump contains exactly the five expected device
//         identifiers, all unique_ids follow the naming convention, no
//         via_device leakage, and any bilateral labels appear under exactly
//         two sides.
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
//                   the five-device and unique_id assertions run.
//
//   BILATERAL_CSV   Optional comma-separated list of firmware sensor labels
//                   (e.g. "TSet,TBoiler"). When set, each label must appear
//                   in exactly one boiler uniq_id AND exactly one thermostat
//                   uniq_id. Omit or leave empty to skip this check.
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
// Block B: Modern mode five-device split + naming + no via_device
// ---------------------------------------------------------------------------

console.log('\n--- Block B: Modern mode five-device assertions ---');

const modernDumpPath = process.env.MODERN_DUMP;
const nodeId         = process.env.NODE_ID;

const EXPECTED_SUFFIXES = ['boiler', 'thermostat', 'gateway', 'esp', 'sat'];

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

    // B-1: Five distinct device identifiers
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

    for (const suffix of EXPECTED_SUFFIXES) {
      const expected = `${nodeId}-${suffix}`;
      log(
        foundDevices.has(expected),
        `Device identifier "${expected}" present in payloads (found: ${[...foundDevices].join(', ') || 'none'})`
      );
    }

    // B-2: Every uniq_id / unique_id matches ^{nodeId}-(boiler|thermostat|gateway|esp|sat)-
    const uniqIdPattern = new RegExp(
      `^${nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')}-(boiler|thermostat|gateway|esp|sat)-`
    );
    const uniqIdRe = /"(?:uniq_id|unique_id)"\s*:\s*"([^"]+)"/g;

    let totalUniqIds    = 0;
    let badUniqIds      = 0;
    const badExamples   = [];

    for (const { payload } of entries) {
      uniqIdRe.lastIndex = 0;
      let m;
      while ((m = uniqIdRe.exec(payload)) !== null) {
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
        `All ${totalUniqIds} uniq_id/unique_id values match ^${nodeId}-(boiler|thermostat|gateway|esp|sat)-`
      );
    }

    // B-3: No payload contains "via_device"
    const viaDevicePayloads = entries.filter(({ payload }) => /"via_device"/.test(payload));
    if (viaDevicePayloads.length > 0) {
      console.log(`  hint: via_device found in topics: ${viaDevicePayloads.slice(0, 3).map(e => e.topic).join(', ')}`);
    }
    log(viaDevicePayloads.length === 0, `No payload contains "via_device" key (found ${viaDevicePayloads.length})`);

    // ---------------------------------------------------------------------------
    // Block C: Bilateral sensor check (optional)
    // ---------------------------------------------------------------------------

    console.log('\n--- Block C: Bilateral sensor label check ---');

    const bilateralCsv = process.env.BILATERAL_CSV;
    if (!bilateralCsv || bilateralCsv.trim() === '') {
      log(null, 'Bilateral sensor check (BILATERAL_CSV not set or empty)');
    } else {
      const labels = bilateralCsv.split(',').map(s => s.trim()).filter(Boolean);

      // Build a map: label -> set of device-type segments that own it
      // We look for uniq_ids that contain the label and extract the device segment.
      const deviceSegRe = new RegExp(
        `^${nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')}-(boiler|thermostat|gateway|esp|sat)-(.+)$`
      );

      for (const label of labels) {
        const owningSegments = new Set();

        for (const { payload } of entries) {
          uniqIdRe.lastIndex = 0;
          let m;
          while ((m = uniqIdRe.exec(payload)) !== null) {
            const uniqId = m[1];
            const segMatch = deviceSegRe.exec(uniqId);
            if (segMatch) {
              const segment  = segMatch[1]; // e.g. "boiler" or "thermostat"
              const labelPart = segMatch[2]; // e.g. "TSet" or "tset-..."
              // Match if the label part starts with the label (case-insensitive)
              if (labelPart.toLowerCase().startsWith(label.toLowerCase())) {
                owningSegments.add(segment);
              }
            }
          }
        }

        const hasBoiler      = owningSegments.has('boiler');
        const hasThermostat  = owningSegments.has('thermostat');
        const exactlyTwo     = owningSegments.size === 2 && hasBoiler && hasThermostat;

        if (!exactlyTwo) {
          console.log(`  hint: label "${label}" found under segments: ${[...owningSegments].join(', ') || 'none'}`);
        }
        log(
          exactlyTwo,
          `Bilateral label "${label}" appears under both -boiler- and -thermostat- (found: ${[...owningSegments].join(', ') || 'none'})`
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
