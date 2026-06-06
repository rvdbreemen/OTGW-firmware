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
// Block B: Modern mode seven-device split + naming + via_device hub (ADR-124)
// ---------------------------------------------------------------------------

console.log('\n--- Block B: Modern mode seven-device assertions (ADR-124) ---');

const modernDumpPath = process.env.MODERN_DUMP;
const nodeId         = process.env.NODE_ID;

// ADR-124 seven-device topology. The OT-Core device (enum HaDevice::OtCore) is
// rendered per hardware so the model is obvious in HA: "pic" on a PIC build,
// "ot-direct" on an OTGW32 direct-OT build — exactly one is present in any given
// dump. boiler/thermostat/gateway/esp/sat are always present; sensors is present
// only when the unit has Dallas/S0 hardware (reported, not required).
const CORE_SUFFIXES     = ['boiler', 'thermostat', 'gateway', 'esp', 'sat'];
const OTCORE_SUFFIXES   = ['pic', 'ot-direct'];
// Full device-segment alternation used by the uniq_id / bilateral regexes.
// ot-direct is listed before the short tokens so the alternation prefers it.
const DEVICE_SEG_ALT    = '(boiler|thermostat|gateway|ot-direct|esp|pic|sat|sensors)';

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

    // B-1: distinct device identifiers — core five always present, exactly one
    // OT-core (pic xor otdirect), sensors reported when present.
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

    for (const suffix of CORE_SUFFIXES) {
      const expected = `${nodeId}-${suffix}`;
      log(
        foundDevices.has(expected),
        `Device identifier "${expected}" present in payloads (found: ${[...foundDevices].join(', ') || 'none'})`
      );
    }
    // Exactly one OT-Core device (pic on a PIC build, ot-direct on OTGW32).
    const otCorePresent = OTCORE_SUFFIXES.filter((s) => foundDevices.has(`${nodeId}-${s}`));
    log(
      otCorePresent.length === 1,
      `Exactly one OT-Core device present (pic xor ot-direct) — found: ${otCorePresent.join(', ') || 'none'}`
    );
    // sensors is hardware-conditional: report only.
    log(
      null,
      `Sensors device ${foundDevices.has(`${nodeId}-sensors`) ? 'present' : 'absent (no Dallas/S0 hardware in this dump)'}`
    );

    // B-2: Every uniq_id / unique_id matches ^{nodeId}-<device-segment>-
    const uniqIdPattern = new RegExp(
      `^${nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')}-${DEVICE_SEG_ALT}-`
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
        `All ${totalUniqIds} uniq_id/unique_id values match ^${nodeId}-${DEVICE_SEG_ALT}-`
      );
    }

    // B-3: ADR-124 §3 — Gateway is the via_device hub.
    //   (a) via_device is present (the hierarchy is emitted at all),
    //   (b) every via_device value equals "<nodeId>-gateway",
    //   (c) the Gateway's own device block never carries via_device.
    const viaDeviceRe = /"via_device"\s*:\s*"([^"]+)"/g;
    const expectedVia = `${nodeId}-gateway`;
    let viaTotal = 0;
    let viaBad   = 0;
    const viaBadExamples = [];
    for (const { payload } of entries) {
      viaDeviceRe.lastIndex = 0;
      let m;
      while ((m = viaDeviceRe.exec(payload)) !== null) {
        viaTotal++;
        if (m[1] !== expectedVia) {
          viaBad++;
          if (viaBadExamples.length < 3) viaBadExamples.push(m[1]);
        }
      }
    }
    log(viaTotal > 0, `via_device hierarchy emitted (found ${viaTotal} via_device keys)`);
    if (viaBad > 0) console.log(`  hint: non-conforming via_device values: ${viaBadExamples.join(', ')}`);
    log(viaBad === 0, `All via_device values equal "${expectedVia}" (bad: ${viaBad})`);
    // (c) The gateway's own full device block must not point at itself.
    const gatewayIdRe = new RegExp(
      `"identifiers"\\s*:\\s*"${nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')}-gateway"`
    );
    const gatewaySelfVia = entries.filter(
      ({ payload }) => gatewayIdRe.test(payload) && /"via_device"/.test(payload)
    );
    log(
      gatewaySelfVia.length === 0,
      `Gateway device block carries no via_device (found ${gatewaySelfVia.length})`
    );

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
        `^${nodeId.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')}-${DEVICE_SEG_ALT}-(.+)$`
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
