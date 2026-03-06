<!-- markdownlint-disable-file -->

# Task Research Notes: OT monitor panel fill and command spacing

## Research Executed

### File Analysis

- src/OTGW-firmware/data/index.html
  - The OT monitor UI places the command bar (`.ot-cmd-bar`) directly above the black log container and defines the gateway mode indicator in the same panel.
- src/OTGW-firmware/data/index.js
  - `refreshOTmonitor()` creates each OT monitor row with `.otmonrow`, adds `.no-data-row` when `entry.epoch == 0`, stores `entry.epoch` in a hidden input, and removes `.no-data-row` once values arrive.
  - `refreshDevTime()` updates `isPSmode` from `/v2/device/time`, and `applyPSmodeState()` keeps OT monitor polling active in PS=1 mode.
  - `sendOTGWcommand()` wires the short command input/button/status, but there is no JS-side spacing logic for the command bar.
- src/OTGW-firmware/data/index.css
  - `.otmonrow` is always rendered with a filled background in the light theme, but there is no `.no-data-row`, `.ot-cmd-bar`, `.ot-cmd-input`, or `.ot-cmd-status` rule in the stylesheet.
- src/OTGW-firmware/data/index_dark.css
  - The dark theme mirrors the same issue: `.otmonrow` always has a filled background and there are no command-bar-specific spacing/layout rules.
- src/OTGW-firmware/src/OTGW-firmware/restAPI.ino
  - `/v2/otgw/otmonitor` emits per-field `lastupdated` values as `epoch`, which the frontend already consumes as `entry.epoch`.
- src/OTGW-firmware/src/OTGW-firmware/OTGW-Core.ino
  - `msglastupdated[]` is the authoritative timestamp store for OT values and is cleared when `PS=1` is detected, confirming that row freshness is intended to be timestamp-driven.
- docs/adr/ADR-037-gateway-mode-detection.md
  - Gateway/monitor mode and PS=1 interaction are documented constraints; PS=1 suppresses time sync but the UI still needs to expose mode correctly.
- docs/adr/ADR-045-ps1-print-summary-parsing.md
  - PS=1 summary parsing intentionally continues feeding the normal OT data pipeline, while WebSocket log display stays in summary mode.
- docs/adr/ADR-005-websocket-real-time-streaming.md
  - WebSocket use is limited to OT streaming over `ws://`; frontend changes must preserve this model.
- docs/adr/ADR-025-safari-websocket-connection-management.md
  - Frontend behavior must remain Safari-safe and avoid fragile browser-specific handling.

### Code Search Results

- `PS=1|summary|No UI updates`
  - Found PS=1 handling in `src/OTGW-firmware/OTGW-Core.ino`, `src/OTGW-firmware/data/index.js`, and ADRs `ADR-037` / `ADR-045`.
- `no-data-row|epoch`
  - Found `.no-data-row` only in `src/OTGW-firmware/data/index.js`; no matching CSS exists in either theme stylesheet.
- `ot-cmd-bar|ot-cmd-input|ot-cmd-status`
  - Found only in `src/OTGW-firmware/data/index.html` and JS event handlers; no dedicated CSS exists in either theme stylesheet.
- `mode-status|gatewayMode`
  - Found in `src/OTGW-firmware/data/index.js` and both theme stylesheets, confirming gateway mode indicator styling is separate from OT row freshness styling.
- `sendOTmonitorV2|msglastupdated`
  - Found in `src/OTGW-firmware/restAPI.ino` and `src/OTGW-firmware/OTGW-Core.ino`, confirming the backend already exposes the timestamps needed for a freshness-based UI.

### External Research

- #githubRepo:"mdn/browser-compat-data css.properties.gap flex_context api.WebSocket readyState"
  - MDN browser-compat-data models `gap` with flex-specific compatibility history, which matches the project instruction to validate browser support when using layout spacing changes across Safari/Firefox/Chrome.
- #fetch:https://developer.mozilla.org/en-US/docs/Web/CSS/gap
  - MDN documents `gap` as valid for flex/grid/multi-column layouts; flex-layout support is listed as Safari 14.1+, Chrome 84+, Firefox 63+, which fits this project's browser support floor.
- #fetch:https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState
  - MDN confirms `WebSocket.readyState` values and broad browser support, reinforcing the existing instruction that any WebSocket-related UI changes must continue using ready-state-safe logic.

### Project Conventions

- Standards referenced: frontend browser compatibility rules in `.github/copilot-instructions.md`; local-network `ws://` / no-HTTPS constraints; OT monitor data freshness via `epoch`; dual theme parity across `index.css` and `index_dark.css`.
- Instructions followed: `.github/instructions/adr.coding-agent.instructions.md`, `.github/instructions/adr.code-review.instructions.md`, `.github/copilot-instructions.md`, `.github/skills/adr/SKILL.md`.

## Key Discoveries

### Project Structure

The relevant UI is concentrated in the LittleFS frontend bundle under `src/OTGW-firmware/data/`:

- `index.html` defines the OT monitor panel, command bar, log container, statistics tab, and graph tab.
- `index.js` polls `/v2/otgw/otmonitor` and `/v2/device/time`, manages PS=1 state, and dynamically builds OT monitor rows from API data.
- `index.css` and `index_dark.css` provide separate light/dark theme styles and must both be updated for consistent behavior.

The data source is server-side:

- `restAPI.ino` exposes `/v2/otgw/otmonitor` with per-field `epoch` timestamps.
- `OTGW-Core.ino` updates `msglastupdated[]` on live OT data and clears it when PS=1 mode begins.

### Implementation Patterns

The frontend already contains the right semantic hook for issue 1 but never completes it visually:

- `refreshOTmonitor()` adds `.no-data-row` when `entry.epoch == 0`.
- It later removes `.no-data-row` when the field starts receiving data.
- There is no corresponding CSS rule, so the base `.otmonrow` background remains fully filled from first render in both themes.

For issue 2, the command bar markup exists, but spacing is accidental/default:

- `.ot-cmd-bar`, `.ot-cmd-input`, and `.ot-cmd-status` exist in HTML only.
- Neither theme stylesheet defines margin, gap, flex wrapping, or separation from `.ot-log-container`.
- The visual closeness to the black log panel is therefore a missing-style issue, not a JS layout bug.

### Complete Examples

```javascript
// Existing row lifecycle in refreshOTmonitor()
if ((document.getElementById("otmon_" + entry.name)) == null) {
  var rowDiv = document.createElement("div");
  rowDiv.setAttribute("class", "otmonrow");
  if (entry.epoch == 0) rowDiv.classList.add('no-data-row');

  var epoch = document.createElement("INPUT");
  epoch.setAttribute("type", "hidden");
  epoch.setAttribute("id", "otmon_epoch_" + entry.name);
  epoch.value = entry.epoch;
  rowDiv.appendChild(epoch);
  // ... append name/value/unit columns
} else {
  var update = document.getElementById("otmon_" + entry.name);
  if (update.parentNode) {
    if (entry.epoch == 0) {
      update.parentNode.classList.add('no-data-row');
    } else {
      update.parentNode.classList.remove('no-data-row');
    }
  }
}

// Existing command bar markup in index.html
// <div class="ot-cmd-bar">
//   <input id="otCmdInput" class="ot-cmd-input" ... />
//   <button id="btnSendCmd" class="btn-log-control">Send</button>
//   <span id="otCmdStatus" class="ot-cmd-status"></span>
// </div>
```

### API and Schema Documentation

- `/v2/otgw/otmonitor` is the OT monitor data source.
- Each field includes a freshness timestamp (`epoch`) derived from backend `lastupdated` values.
- `refreshDevTime()` reads `/v2/device/time` and updates `isPSmode` from `devtime.psmode`.
- `OTGW-Core.ino` clears `msglastupdated[]` when `PS=1` is detected, so zero timestamps are an intentional empty-state signal.

### Configuration Examples

```css
/* Current state in both themes */
.otmonrow {
  background: lightblue;   /* dark theme uses #2c2c2e */
  display: flex;
  align-items: center;
  padding: 2px 0;
}

.ot-log-controls {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-bottom: 10px;
}

/* Missing today in both themes:
   .no-data-row
   .ot-cmd-bar
   .ot-cmd-input
   .ot-cmd-status
*/
```

### Technical Requirements

- Issue 1 must respect PS=1 behavior documented in ADR-037 and ADR-045: PS mode changes how data arrives, but OT monitor values still flow through the standard pipeline and carry timestamps.
- Any UI fix should rely on the existing timestamp/epoch mechanism rather than infer freshness from current value text.
- Any spacing/layout fix must be added to both `index.css` and `index_dark.css` to avoid theme regressions.
- Frontend changes must remain compatible with Chrome, Firefox, and Safari, per `.github/copilot-instructions.md`.
- WebSocket-related behavior must remain `ws://`-based and ready-state-safe; no HTTPS/WSS or browser-specific hacks.

## Recommended Approach

Use the existing `entry.epoch` / `msglastupdated[]` pipeline as the single source of truth for OT monitor row fill state, and complete the missing frontend styling in both themes.

Why this is the strongest fit:

- The backend already exposes the exact freshness signal needed.
- The frontend already toggles a semantic class (`.no-data-row`) from that signal.
- The current bug appears to be an incomplete UI implementation rather than a missing data source.
- The command-spacing issue is likewise a missing stylesheet concern, not an API or command-queue problem.

This keeps the fix localized to UI assets unless user-visible “timestamp-based fill” means a more advanced progressive-fill animation than the current binary seen/unseen behavior. If the desired effect is a gradual fill proportional to recency, that expectation is not yet encoded anywhere in the current code and needs clarification.

## Implementation Guidance

- **Objectives**: Restore a true empty-state appearance for OT monitor rows until data has been observed; add explicit separation between the command bar and the log container.
- **Key Tasks**: Add missing row-state CSS for both themes; add explicit command-bar layout/spacing styles for both themes; verify PS=1 transitions still clear/repopulate row freshness correctly.
- **Dependencies**: Existing `epoch` values from `/v2/otgw/otmonitor`; `msglastupdated[]` updates in firmware; theme-specific stylesheets; browser compatibility rules from project instructions.
- **Success Criteria**: Rows do not appear fully filled on initial load when their `epoch` is zero; rows visually transition once data is seen; the short command area has clear breathing room above the black log panel in both light and dark themes; no conflict with PS=1 mode handling or Safari/Firefox/Chrome compatibility expectations.