// TASK-951 regression guard: every device-info (debug) and settings key that the
// Web UI renders must resolve to a human label via translateToHuman() and a
// tooltip via translateTooltip() — no raw snake_case key may leak to the screen.
// Both screens share these functions (translateToHuman at index.js formatDeviceInfoLabel
// and at the settings row builder), so one assertion covers both.
//
// Run: npx playwright test tests/webui_label_coverage.spec.js
// (needs @playwright/test + a chromium; same toolchain as webui_settings_layout.spec.js)

const path = require('path');
const { test, expect } = require('@playwright/test');

const REPO_ROOT = path.resolve(__dirname, '..');
const DATA_DIR = path.join(REPO_ROOT, 'src', 'OTGW-firmware', 'data');

// Keys that previously rendered raw (TASK-951). Extend this list when new
// device-info/settings keys are added so the guard keeps them honest.
const KEYS = [
  // device-info (debug)
  'platform', 'hardware_type', 'eth_current_subnet', 'eth_current_gateway', 'eth_current_dns', 'otgwsimulation',
  'perf_sat_status_total_ms', 'perf_sat_status_send_ms', 'perf_sat_status_render_ms', 'perf_sat_status_chunks',
  'perf_device_info_total_ms', 'perf_device_info_send_ms', 'perf_device_info_render_ms', 'perf_device_info_chunks',
  'perf_device_info_max_ms', 'perf_device_info_samples',
  'perf_settings_total_ms', 'perf_settings_send_ms', 'perf_settings_render_ms', 'perf_settings_chunks',
  // UI settings
  'ui_autoscroll', 'ui_timestamps', 'ui_capture', 'ui_autoscreenshot', 'ui_autodownloadlog', 'ui_autoexport', 'ui_usev2', 'ui_graphtimewindow',
  // SAT settings
  'sathpcycle', 'satforcepwm', 'satpushsetpoint', 'satwindowdetect', 'satwindowminsec', 'satthermalcomfort', 'sathumiditytimeout',
  'satflushtreshold', 'satdhwenabled', 'satdhwenable', 'satzonecount', 'satzonetimeout',
  'satsensorarea0', 'satsensorarea1', 'satsensorarea2', 'satsensorarea3', 'satdhwsetpoint', 'satflameoffset', 'satflowoffset',
  'satmodsupdelay', 'satmodsupoffset', 'satminpressure', 'satmaxpressure', 'satmaxpressdrop', 'satzoneheadroom', 'satsolarminelev',
  'satboilerratedkw', 'satboilerefficiency',
  // OT-Direct settings
  'otdchmode', 'otdflowtemp', 'otdflowmax', 'otdroomsetpoint', 'otdgradient', 'otdexponent', 'otdoffset', 'otdkp', 'otdki', 'otdkboost',
  'otdhysteresis', 'otdroomcomp', 'otdhysteresisenable', 'otdventenable', 'otdopenbypass', 'otdautobypass', 'otdfreeventenable', 'otdventsetpoint',
  // WiFi static-IP settings
  'wifistaticip', 'wifisubnet', 'wifigateway', 'wifidns1', 'wifidns2'
];

test('every device-info/settings key resolves to a human label + tooltip', async ({ page }) => {
  await page.setContent('<!doctype html><html><body><main id="settingsPage"></main></body></html>');
  await page.addScriptTag({ path: path.join(DATA_DIR, 'index.js') });

  const results = await page.evaluate((keys) => keys.map((k) => ({
    key: k,
    label: (typeof translateToHuman === 'function') ? translateToHuman(k) : '<no translateToHuman>',
    tip: (typeof translateTooltip === 'function') ? translateTooltip(k) : '<no translateTooltip>'
  })), KEYS);

  const rawLabels = results.filter((r) => r.label === r.key);          // label fell through to the raw key
  const noTips = results.filter((r) => !r.tip || r.tip.length === 0);  // missing tooltip

  expect(rawLabels.map((r) => r.key), 'keys rendering as raw snake_case (no label)').toEqual([]);
  expect(noTips.map((r) => r.key), 'keys with no tooltip').toEqual([]);
});
