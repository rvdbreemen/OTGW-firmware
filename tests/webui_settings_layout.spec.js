const path = require('path');
const { test, expect } = require('@playwright/test');

const REPO_ROOT = path.resolve(__dirname, '..');
const DATA_DIR = path.join(REPO_ROOT, 'src', 'OTGW-firmware', 'data');

function fileUrl(filePath) {
  return 'file:///' + filePath.replace(/\\/g, '/');
}

async function renderSettingsFixture(page, theme, viewportWidth) {
  await page.setViewportSize({ width: viewportWidth, height: 1100 });
  await page.setContent(`<!doctype html>
    <html>
      <head>
        <meta charset="utf-8">
        <link rel="stylesheet" href="${fileUrl(path.join(DATA_DIR, 'ds-tokens.css'))}">
        <link rel="stylesheet" href="${fileUrl(path.join(DATA_DIR, 'components.css'))}">
      </head>
      <body data-theme="${theme}">
        <main id="settingsPage"></main>
      </body>
    </html>`);

  await page.addScriptTag({ path: path.join(DATA_DIR, 'index.js') });
  await page.evaluate(() => {
    window.onload = null;

    function addSettingRow(groupBody, id, label, type, value, buttonText) {
      const row = document.createElement('div');
      row.className = 'settingDiv';
      row.id = 'D_' + id;

      const labelEl = document.createElement('label');
      labelEl.className = 'settings-field-container';
      labelEl.setAttribute('for', id);
      labelEl.textContent = label;
      row.appendChild(labelEl);

      const inputWrap = document.createElement('div');
      inputWrap.className = 'settings-input-container';
      const input = document.createElement('input');
      input.id = id;
      input.type = type;
      if (type !== 'checkbox') input.value = value;
      if (type === 'checkbox') input.checked = value === true;
      inputWrap.appendChild(input);

      if (buttonText) {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'btn-wifi-reset';
        button.textContent = buttonText;
        inputWrap.appendChild(button);
      }

      row.appendChild(inputWrap);
      groupBody.appendChild(row);
    }

    const settingsPage = document.getElementById('settingsPage');
    const systemBody = getOrCreateSettingsGroup(settingsPage, 'system');
    addSettingRow(systemBody, 'hostname', 'Hostname', 'text', 'OTGW');
    addSettingRow(systemBody, 'httppasswd', 'Protected Endpoints Password', 'password', '');

    const networkBody = getOrCreateSettingsGroup(settingsPage, 'network');
    addSettingRow(networkBody, 'ssid', 'Wi-Fi Network (SSID)', 'text', 'Keep current', 'Reset WiFi');
    const fixedSettings = {
      wifistaticip: { value: '' },
      wifisubnet: { value: '255.255.255.0' },
      wifigateway: { value: '192.168.88.1' },
      wifidns1: { value: '192.168.88.1' },
      wifidns2: { value: '1.1.1.1' },
      ethstaticip: { value: 'false' },
      ethipaddress: { value: '192.168.88.64' },
      ethsubnet: { value: '255.255.255.0' },
      ethgateway: { value: '192.168.88.1' },
      ethdns: { value: '192.168.88.1' }
    };
    renderFixedIPSection('eth', fixedSettings.ethstaticip.value, fixedSettings);
    renderFixedIPSection('wifi', fixedSettings.wifistaticip.value, fixedSettings);

    buildWifiScanPanel(networkBody);
    const scanResult = document.getElementById('wifi-scan-result');
    scanResult.style.display = '';
    const option = document.createElement('option');
    option.textContent = 'SERGEANTD [ch6, -45dBm] [secured] (current)';
    scanResult.appendChild(option);
    document.getElementById('wifi-scan-status').textContent = '1 network found';

    const mqttBody = getOrCreateSettingsGroup(settingsPage, 'mqtt');
    addSettingRow(mqttBody, 'MQTTenable', 'MQTT Enabled', 'checkbox', false);
    addSettingRow(mqttBody, 'MQTTbroker', 'MQTT Broker Host/IP', 'text', 'homeassistant.local');
    addSettingRow(mqttBody, 'MQTTport', 'MQTT Broker Port', 'number', '1883');

    const ntpBody = getOrCreateSettingsGroup(settingsPage, 'ntp');
    addSettingRow(ntpBody, 'NTPenable', 'NTP Enabled', 'checkbox', true);

    const satBody = getOrCreateSettingsGroup(settingsPage, 'sat');
    addSettingRow(
      satBody,
      'SATmanufacturer',
      'SAT Manufacturer (0=Auto, 1=Atag, 2=Baxi, 3=Brotge, 4=DeDietrich, 5=Ferroli, 6=Geminox, 7=Ideal, 8=Immergas, 9=Intergas, 10=Itho, 11=Nefit, 12=Radiant, 13=Remeha, 14=Sime, 15=Vaillant, 16=Viessmann, 17=Worcester, 18=Other)',
      'number',
      '0'
    );

    if (typeof normalizeSettingsLabelWidth === 'function') {
      normalizeSettingsLabelWidth();
    }
  });
  await page.waitForTimeout(100);
}

async function assertSettingsLayoutContained(page) {
  const report = await page.evaluate(() => {
    const failures = [];
    const tolerance = 1;

    document.querySelectorAll('.settings-group').forEach((card) => {
      const cardRect = card.getBoundingClientRect();
      if (card.scrollWidth > card.clientWidth + tolerance) {
        failures.push({
          card: card.getAttribute('data-group-id'),
          selector: '.settings-group scrollWidth',
          clientWidth: Math.round(card.clientWidth),
          scrollWidth: Math.round(card.scrollWidth)
        });
      }
      const body = card.querySelector('.settings-group-body');
      if (body && body.scrollWidth > body.clientWidth + tolerance) {
        failures.push({
          card: card.getAttribute('data-group-id'),
          selector: '.settings-group-body scrollWidth',
          clientWidth: Math.round(body.clientWidth),
          scrollWidth: Math.round(body.scrollWidth)
        });
      }
      const selectors = [
        '.settings-group-body',
        '.settings-group-body > .settingDiv',
        '.settings-group-body > .settingDiv > .settings-field-container',
        '.settings-group-body > .settingDiv > .settings-input-container',
        '.settings-group-body > .settingDiv input',
        '.settings-group-body > .settingDiv button',
        '#wifi-scan-panel',
        '#wifi-scan-panel .settingDiv',
        '#wifi-scan-panel button',
        '#wifi-scan-panel select'
      ];

      selectors.forEach((selector) => {
        card.querySelectorAll(selector).forEach((el) => {
          const rect = el.getBoundingClientRect();
          if (rect.width === 0 && rect.height === 0) return;
          if (rect.left < cardRect.left - tolerance || rect.right > cardRect.right + tolerance) {
            failures.push({
              card: card.getAttribute('data-group-id'),
              selector,
              cardLeft: Math.round(cardRect.left),
              cardRight: Math.round(cardRect.right),
              left: Math.round(rect.left),
              right: Math.round(rect.right),
              width: Math.round(rect.width)
            });
          }
        });
      });
    });

    const settingsPage = document.getElementById('settingsPage');
    return {
      failures,
      pageLabelWidthVar: settingsPage.style.getPropertyValue('--settings-label-w') || null,
      cards: Array.from(document.querySelectorAll('.settings-group')).map((card) => {
        const rect = card.getBoundingClientRect();
        const body = card.querySelector('.settings-group-body');
        return {
          id: card.getAttribute('data-group-id'),
          width: Math.round(rect.width),
          labelWidthVar: body ? body.style.getPropertyValue('--settings-label-w') || null : null,
          bodyScrollWidth: body ? Math.round(body.scrollWidth) : null,
          bodyClientWidth: body ? Math.round(body.clientWidth) : null
        };
      })
    };
  });

  expect(report.pageLabelWidthVar, JSON.stringify(report, null, 2)).toBe(null);
  expect(report.failures, JSON.stringify(report, null, 2)).toEqual([]);
}

for (const theme of ['light', 'dark']) {
  test(`settings cards contain subgrid rows on wide desktop in ${theme} theme`, async ({ page }) => {
    await renderSettingsFixture(page, theme, 1600);
    await assertSettingsLayoutContained(page);
  });

  test(`settings cards contain subgrid rows on compact viewport in ${theme} theme`, async ({ page }) => {
    await renderSettingsFixture(page, theme, 390);
    await assertSettingsLayoutContained(page);
  });
}
