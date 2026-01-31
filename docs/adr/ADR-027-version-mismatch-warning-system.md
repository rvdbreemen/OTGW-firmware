# ADR-027: Version Mismatch Warning System in Web UI

**Status:** Accepted  
**Date:** 2026-01-31  
**Supersedes:** N/A

## Context

After implementing Safari WebSocket fix (ADR-025) and conditional cache-busting (ADR-026), there was still a user experience gap: users were unaware when firmware and filesystem versions didn't match, leading to confusion about broken functionality during the update transition period.

**Problem scenario:**
1. User flashes new firmware → ESP reboots
2. Firmware version: `cd83ad6` (new)
3. Filesystem version: `00514ba` (old - not yet flashed)
4. **Version mismatch exists** but user has no visual indication
5. User tries to use Web UI → Some features don't work correctly
6. User confused: "I just updated the firmware, why is it broken?"
7. User doesn't realize they need to flash filesystem too

**Visibility gap:**
- Debug log shows warning (telnet port 23): "WARNING: Firmware version (cd83ad6) does not match filesystem version (00514ba)"
- Most users don't check telnet debug log
- No indication in the Web UI where users actually interact
- Error manifests as broken functionality, not clear root cause

**Requirements:**
- Warning must be visible in Web UI where users are looking
- Must show only when versions mismatch (not during normal operation)
- Must disappear automatically when versions match again (after filesystem flash)
- Must work in both light and dark themes
- Must be prominent enough to notice but not block functionality
- Must work in all browsers (Safari, Chrome, Firefox, Edge)

## Decision

**Add prominent red warning banner in Web UI that automatically shows when firmware and filesystem versions mismatch, hides when they match.**

**Implementation approach:**
1. **Backend detection:** `checklittlefshash()` compares versions, sets `sMessage` on mismatch
2. **API exposure:** `/api/v0/devtime` endpoint returns `sMessage` in JSON
3. **Frontend polling:** Web UI polls devtime API periodically (every few seconds)
4. **Visual warning:** JavaScript detects version-related message, applies warning styling
5. **Automatic cleanup:** Warning disappears when `sMessage` clears (versions match)

**Warning banner design:**
- **Location:** Bottom-left fixed position
- **Color:** Red background (#ff6b6b) with white text
- **Style:** Bold text, rounded corners, drop shadow
- **Content:** "Flash your littleFS with matching version!"
- **Behavior:** Only shown when message contains version keywords

**Detection keywords:**
- "littlefs" (case-insensitive)
- "version" (case-insensitive)
- "flash your" (case-insensitive)

**CSS implementation:**
```css
/* Both light and dark themes */
.version-warning {
  position: fixed;
  bottom: 20px;
  left: 20px;
  padding: 15px 25px;
  background-color: #ff6b6b;
  color: white;
  font-weight: bold;
  border-radius: 8px;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
  z-index: 1000;
}
```

**JavaScript detection:**
```javascript
function updateMessage(data) {
  var msg = data.sMessage || "";
  var msgElement = document.getElementById('message');
  
  if (msg && msg.length > 0) {
    msgElement.innerText = msg;
    msgElement.style.display = 'block';
    
    // Check for version-related keywords (case-insensitive)
    var msgLower = msg.toLowerCase();
    if (msgLower.includes('littlefs') || 
        msgLower.includes('version') || 
        msgLower.includes('flash your')) {
      msgElement.classList.add('version-warning');
    } else {
      msgElement.classList.remove('version-warning');
    }
  } else {
    // No message: hide warning
    msgElement.style.display = 'none';
    msgElement.classList.remove('version-warning');
  }
}
```

## Alternatives Considered

### Alternative 1: Modal Dialog (Blocking)
**Approach:** Show modal dialog that blocks UI until user clicks "OK" or "Dismiss".

**Pros:**
- Impossible to miss
- Forces user acknowledgment
- Clear call to action

**Cons:**
- **Blocks UI access:** User can't proceed without dismissing
- Annoying if shown repeatedly
- Requires button interaction
- May interrupt workflow
- Could appear on every page load during mismatch

**Why not chosen:** Too intrusive. Users might need to access UI even during version mismatch (to flash filesystem). Non-blocking warning is better UX.

### Alternative 2: Top Banner (Full Width)
**Approach:** Warning banner across top of page, full width.

**Pros:**
- Highly visible
- Standard warning pattern
- Doesn't hide content (pushes down)

**Cons:**
- Takes valuable screen space
- Disrupts page layout
- May hide navigation
- Harder to dismiss visually
- Fixed header complicates layout

**Why not chosen:** Bottom-left corner is visible without disrupting page layout or hiding navigation.

### Alternative 3: Toast Notification (Auto-Dismiss)
**Approach:** Temporary toast notification that appears and fades after few seconds.

**Pros:**
- Non-intrusive
- Familiar pattern
- Doesn't block content
- Clean UI

**Cons:**
- **Temporary:** May disappear before user notices
- User might miss if not looking at screen
- No persistent reminder
- Requires re-showing logic
- Doesn't solve "forgot to flash filesystem" scenario

**Why not chosen:** Version mismatch is a persistent state that requires persistent warning. Temporary toast could be missed or forgotten.

### Alternative 4: Inline Message in Settings/About Page
**Approach:** Show warning only in Settings or About page where version info displayed.

**Pros:**
- Contextually relevant location
- Doesn't clutter main UI
- Easy to implement

**Cons:**
- **Low visibility:** Users may never visit Settings/About page
- Not shown where problem manifests (main UI)
- Easy to miss
- Doesn't help users who encounter broken functionality first

**Why not chosen:** Too easy to miss. Warning must be visible where users actually work (main dashboard).

### Alternative 5: Telnet Log Only (Status Quo)
**Approach:** Keep current behavior - warning only in telnet debug log.

**Pros:**
- No UI changes needed
- No additional code
- Debug log already has detailed info

**Cons:**
- **Invisible to most users:** Very few users check telnet log
- Requires technical knowledge (telnet connection)
- Not where users are looking when problem occurs
- Doesn't help average user

**Why not chosen:** Clearly insufficient based on user confusion. Web UI is where users need the warning.

## Consequences

### Positive
- **Visible warning:** Users immediately aware of version mismatch
- **Contextual location:** Shows where users are working (Web UI)
- **Automatic behavior:** Appears/disappears based on actual state
- **Cross-theme support:** Works in both light and dark themes
- **Cross-browser:** Works in Safari, Chrome, Firefox, Edge
- **Non-blocking:** Doesn't prevent UI access during mismatch
- **Persistent:** Stays visible until problem resolved
- **Clear action:** Message tells user exactly what to do ("Flash your littleFS")

### Negative
- **Additional UI element:** Adds visual clutter during mismatch period
  - **Accepted:** Only shown during abnormal state (rare)
- **Polling overhead:** Devtime API polled every few seconds
  - **Accepted:** Already polling for time display, minimal additional cost
- **CSS duplication:** Same styles in both theme files
  - **Accepted:** Small amount of code (~10 lines per theme)

### Risks & Mitigation
- **False positives:** Warning shown when it shouldn't be
  - **Mitigation:** Keyword detection carefully chosen (specific to version messages)
  - **Mitigation:** Backend only sets sMessage when actual mismatch detected
- **Warning stuck:** Doesn't disappear after filesystem flash
  - **Mitigation:** checklittlefshash() clears sMessage when versions match
  - **Mitigation:** Frontend re-checks message on every poll
- **Theme compatibility:** Different appearance in light/dark theme
  - **Mitigation:** Identical styling in both theme CSS files
  - **Testing:** Verified in both themes

## Implementation Notes

**Files modified:**
- `data/index.js`: Enhanced message display logic with keyword detection
- `data/index.css`: Added `.version-warning` class styling (light theme)
- `data/index_dark.css`: Added `.version-warning` class styling (dark theme)

**Backend message setting:**
```cpp
// helperStuff.ino - checklittlefshash()
bool match = (strcasecmp(CSTR(_githash), _VERSION_GITHASH) == 0);
if (!match) {
  DebugTf(PSTR("WARNING: Firmware version (%s) does not match filesystem version (%s)\r\n"), 
          _VERSION_GITHASH, CSTR(_githash));
  DebugTln(F("This may cause compatibility issues. Flash matching filesystem version."));
  // Set message for Web UI
  sMessage = "Flash your littleFS with matching version!";
} else {
  sMessage = "";  // Clear message when versions match
}
```

**Frontend message updates:**
```javascript
// Polls /api/v0/devtime periodically
function updateDevtime() {
  fetch('/api/v0/devtime')
    .then(response => response.json())
    .then(data => {
      updateMessage(data);  // Shows/hides warning based on sMessage
      // ... other updates
    });
}
```

**State transitions:**
```
Normal operation (versions match):
  sMessage = ""
  Warning element: display: none
  
After firmware flash (mismatch):
  sMessage = "Flash your littleFS with matching version!"
  Warning element: display: block, class: version-warning
  Visual: Red box at bottom-left
  
After filesystem flash (match again):
  sMessage = ""
  Warning element: display: none
```

## Visual Design

**Warning appearance (both themes):**
```
┌────────────────────────────────────────────────────┐
│ Flash your littleFS with matching version!         │
└────────────────────────────────────────────────────┘
```

**Styling details:**
- Background: #ff6b6b (red)
- Text: white, bold
- Padding: 15px vertical, 25px horizontal
- Border-radius: 8px (rounded corners)
- Box-shadow: 0 4px 6px rgba(0,0,0,0.3)
- Position: fixed, bottom: 20px, left: 20px
- Z-index: 1000 (above other content)

**Accessibility:**
- High contrast (white on red)
- Bold text for readability
- Fixed position for consistency
- No flashing or animation (avoid seizure triggers)

## Browser Compatibility

**Tested browsers:**
- Safari 26 (macOS): ✅ Warning shown/hidden correctly
- Safari 26 (iOS): ✅ Works
- Chrome 120: ✅ Works
- Firefox 121: ✅ Works
- Edge (latest): ✅ Works

**JavaScript features used:**
- `String.toLowerCase()`: All browsers ✅
- `String.includes()`: ES6, all modern browsers ✅
- `Element.classList`: All browsers ✅
- `Element.style.display`: All browsers ✅

## User Experience Flow

**Typical update sequence:**
1. **Normal operation:**
   - User sees dashboard
   - No warning shown
   - Everything works normally

2. **Flash firmware:**
   - User uploads new firmware
   - ESP reboots
   - Dashboard loads
   - **Red warning appears at bottom-left:** "Flash your littleFS with matching version!"
   - User immediately aware something needs attention

3. **Flash filesystem:**
   - User uploads new filesystem
   - ESP reboots
   - Dashboard loads
   - **Warning disappears automatically**
   - User sees clean UI, knows update complete

**Error prevention:**
- Clear message prevents user confusion
- Tells user exactly what to do
- Prevents premature "update complete" assumption
- Reduces support requests

## Related Decisions
- **ADR-025:** Safari WebSocket Connection Management (companion fix)
- **ADR-026:** Conditional JavaScript Cache-Busting (handles version transition)
- **ADR-008:** LittleFS for Configuration Persistence (version.hash storage)
- **ADR-018:** ArduinoJson for Data Interchange (devtime API JSON response)

## References
- **Pull Request:** #394 (Safari WebSocket + caching + warning system)
- **Implementation:** `data/index.js` lines 450-470 (message display logic)
- **Implementation:** `data/index.css` lines 800-810 (warning styling)
- **Implementation:** `data/index_dark.css` lines 800-810 (dark theme warning styling)
- **Backend:** `helperStuff.ino` lines 750-770 (checklittlefshash)
- **Backend:** `OTGW-Core.ino` lines 200-210 (version check enabled)
- **API:** `/api/v0/devtime` endpoint (returns sMessage in JSON)
- **Testing:** Verified on hardware with firmware/filesystem update sequence
