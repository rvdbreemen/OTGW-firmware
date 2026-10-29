# Dallas Temperature Sensor Labels API Documentation

## Overview

The Dallas Temperature Sensor Labels feature provides custom labeling for Dallas DS18B20/DS18S20/DS1822 temperature sensors with a file-based, zero-RAM backend architecture.

### Key Features

- **File-Based Storage**: Labels stored in `/dallas_labels.ini` on LittleFS filesystem
- **Zero Backend RAM**: Labels never loaded into backend memory
- **Bulk Operations**: Simple GET/POST API for fetching and updating all labels
- **Web UI Integration**: Non-blocking modal dialog for inline label editing
- **Graph Visualization**: 16 unique colors per theme for up to 16 sensors
- **Persistent**: Labels survive reboots and firmware upgrades

### Architecture

```
┌─────────────┐     GET /api/v2/sensors/labels      ┌──────────────┐
│   Web UI    │ ──────────────────────────────────> │   Backend    │
│             │                                      │              │
│ (Browser)   │ <────────────────────────────────── │  (ESP8266)   │
│             │     JSON: {"addr": "label", ...}    │              │
└─────────────┘                                      └──────────────┘
      │                                                      │
      │                                                      │
      │ POST /api/v2/sensors/labels                        │
      │ {"addr": "New Label", ...}                         │
      └────────────────────────────────────────────────────┘
                                                             │
                                                             v
                                                    ┌─────────────────┐
                                                    │  LittleFS File  │
                                                    │                 │
                                                    │ dallas_labels.  │
                                                    │     ini         │
                                                    └─────────────────┘
```

**Data Flow:**
1. Web UI loads → fetches all labels via GET
2. User clicks sensor name → modal dialog opens
3. User enters label → Web UI reads all labels, modifies one, writes all back
4. Backend writes to file → returns success
5. Web UI updates display with new label

## REST API Specification

### GET /api/v2/sensors/labels

Retrieves all Dallas temperature sensor labels from `/dallas_labels.ini` file.

**URL:** `http://{device-ip}/api/v2/sensors/labels`

**Method:** `GET`

**Request Parameters:** None

**Response (Success - With Labels):**

```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF64D1841703F2": "Kitchen",
  "28FF64D1841703F3": "Bedroom"
}
```

**Response (Success - No Labels):**

```json
{}
```

**HTTP Status Codes:**
- `200 OK`: Labels retrieved successfully (even if empty)
- `503 Service Unavailable`: Not enough free memory to stream the file right now; retry later

**Notes:**
- Returns empty object `{}` if the file doesn't exist
- The file is sent as stored; the backend does not parse it
- Web UI should use sensor address as default label if not found in response
- Minimal backend memory usage (dynamic allocation during request only)

### POST /api/v2/sensors/labels

Writes all Dallas temperature sensor labels to `/dallas_labels.ini` file.

**URL:** `http://{device-ip}/api/v2/sensors/labels`

**Method:** `POST` (`PUT` is accepted too)

**Content-Type:** `application/json`

**Request Body:**

```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF64D1841703F2": "Kitchen"
}
```

**Request Body Schema:**
- Type: JSON object
- Keys: 16-character hexadecimal sensor address (e.g., "28FF64D1841703F1")
- Values: Custom label string (max 16 characters recommended); keys and values must both be quoted strings
- Maximum body size: 2048 bytes
- Empty object `{}` to clear all labels

**Response (Success):**

```json
{"success":true}
```

**Response (Error - Invalid body):**

```json
{"success":false,"error":"Empty or invalid JSON body"}
```

Other `400` error texts: `"Body too large (max 2048 bytes)"`, `"Invalid JSON: expected quoted key"`, `"Invalid JSON: unterminated key"`, `"Invalid JSON: expected colon"`, `"Invalid JSON: expected quoted value"`, `"Invalid JSON: unterminated value"`.

**Response (Error - Write Failed):**

```json
{"success":false,"error":"Failed to open file for writing"}
```

**HTTP Status Codes:**
- `200 OK`: Labels updated successfully
- `400 Bad Request`: Empty, too large or malformed request body
- `500 Internal Server Error`: Could not open the labels file for writing

**Notes:**
- Replaces entire file contents (not incremental update)
- Does not validate sensor existence (allows pre-configuration)
- Creates file automatically if it doesn't exist
- The write is not atomic: the file is truncated and then rewritten

## Usage Patterns

### Fetch Labels on Page Load

```javascript
async function loadSensorLabels() {
  try {
    const response = await fetch('/api/v2/sensors/labels');
    const labels = await response.json();
    
    // Store in memory for fast lookup
    window.sensorLabels = labels;
    
    // Use labels in display
    updateSensorDisplay();
  } catch (error) {
    console.error('Failed to load sensor labels:', error);
    window.sensorLabels = {}; // Empty fallback
  }
}

function getSensorLabel(address) {
  return window.sensorLabels[address] || address; // Default to hex address
}
```

### Update Single Label (Read-Modify-Write Pattern)

```javascript
async function updateSensorLabel(address, newLabel) {
  try {
    // 1. Fetch all labels
    const response = await fetch('/api/v2/sensors/labels');
    const labels = await response.json();
    
    // 2. Modify one label
    labels[address] = newLabel;
    
    // 3. Write all labels back
    const updateResponse = await fetch('/api/v2/sensors/labels', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(labels)
    });
    
    const result = await updateResponse.json();
    
    if (result.success) {
      // Update local cache
      window.sensorLabels[address] = newLabel;
      // Update display
      updateSensorDisplay();
    } else {
      console.error('Failed to update label:', result.error);
    }
  } catch (error) {
    console.error('Error updating sensor label:', error);
  }
}
```

### Bulk Label Import/Export

```javascript
// Export labels for backup
async function exportLabels() {
  const response = await fetch('/api/v2/sensors/labels');
  const labels = await response.json();
  
  const dataStr = JSON.stringify(labels, null, 2);
  const dataBlob = new Blob([dataStr], {type: 'application/json'});
  
  const link = document.createElement('a');
  link.href = URL.createObjectURL(dataBlob);
  link.download = 'dallas_labels_backup.json';
  link.click();
}

// Import labels from backup
async function importLabels(file) {
  const text = await file.text();
  const labels = JSON.parse(text);
  
  const response = await fetch('/api/v2/sensors/labels', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(labels)
  });
  
  const result = await response.json();
  console.log('Import result:', result);
}
```

### Clear All Labels

```javascript
async function clearAllLabels() {
  const response = await fetch('/api/v2/sensors/labels', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({}) // Empty object clears all
  });
  
  const result = await response.json();
  console.log('Clear result:', result);
}
```

## File Format

### Location
`/dallas_labels.ini` on LittleFS filesystem

### Structure

```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF64D1841703F2": "Kitchen",
  "28FF64D1841703F3": "Bedroom",
  "28FF64D1841703F4": "Bathroom",
  "28FF64D1841703F5": "Garage"
}
```

**Key Format:**
- Exactly 16 hexadecimal characters
- Uppercase or lowercase accepted
- Represents Dallas sensor unique address

**Value Format:**
- String (max 16 characters recommended)
- Any UTF-8 characters allowed
- Frontend should handle display width

**File Size:**
- Empty file: `2 bytes` (`{}`)
- Typical (5 sensors): `~200 bytes`
- Maximum (16 sensors, 16-char labels): `~960 bytes`

## Web UI Integration

### Non-Blocking Modal Dialog

The Web UI uses a custom modal dialog for label editing (replaces blocking `prompt()`).

**Features:**
- WebSocket traffic continues during editing
- Keyboard shortcuts (Enter to save, Escape to cancel)
- Auto-focus input field with text pre-selection
- Inline validation with error messages
- Theme-aware styling (light/dark)

**Implementation:**

```javascript
function openLabelModal(sensorAddress) {
  const currentLabel = getSensorLabel(sensorAddress);
  
  // Show modal with current label
  const modal = document.getElementById('labelModal');
  const input = document.getElementById('labelInput');
  
  input.value = currentLabel;
  input.select();
  modal.style.display = 'block';
  
  // Handle save
  document.getElementById('saveLabel').onclick = async () => {
    const newLabel = input.value.trim();
    if (newLabel && newLabel !== currentLabel) {
      await updateSensorLabel(sensorAddress, newLabel);
    }
    modal.style.display = 'none';
  };
  
  // Handle cancel
  document.getElementById('cancelLabel').onclick = () => {
    modal.style.display = 'none';
  };
}
```

### Graph Integration

Dallas sensors appear automatically in the temperature graph with unique colors.

**Color Palette:**
- 16 unique colors per theme (light/dark)
- Colors chosen for visual distinction
- Consistent color assignment per sensor address

**Dynamic Registration:**
```javascript
function detectAndRegisterSensors(data) {
  // Extract sensor addresses from API response
  const sensorPattern = /^28[0-9A-F]{14}$/i;
  
  for (const key in data.otmonitor) {
    if (sensorPattern.test(key)) {
      const address = key;
      const label = getSensorLabel(address);
      
      // Register sensor series if not already registered
      if (!chart.get(address)) {
        registerSensorSeries(address, label);
      }
    }
  }
}
```

## Memory Impact

### Backend (ESP8266)

**Before (Stored in RAM):**
- `settingDallasLabels[1024]` buffer: 1024 bytes persistent RAM
- `label[17]` field in `DallasrealDevice[16]`: 272 bytes
- Settings JSON capacity overhead: +1024 bytes temporary
- **Total: ~2.3KB persistent RAM**

**After (File-Based):**
- Label file operations: ~1KB heap (temporary, during API calls only)
- **Total: 0 bytes persistent RAM**

**Savings: 1.3KB persistent RAM freed (~3% of ESP8266 available RAM)**

### Frontend (Browser)

**Label Storage:**
- Stored in JavaScript object in browser memory
- Typical size: ~200-500 bytes for 5-10 sensors
- Negligible impact (browsers have abundant RAM)

**Network Traffic:**
- Initial page load: 1 GET request (~200-500 bytes response)
- Label update: 1 POST request (~200-500 bytes)
- Minimal bandwidth usage

## Security Considerations

### JSON Injection Prevention

**Backend:**
- No JSON library: a small hand-written check verifies that the body is an object of quoted string keys and values
- The validated body is written to the file as received

**Frontend:**
- Validates input before sending
- Sanitizes display output
- Uses `textContent` instead of `innerHTML` where possible

### Input Validation

**Backend:**
- Checks the body structure before accepting it
- Rejects bodies larger than 2048 bytes
- Returns `400` with an error text on a malformed body

**Frontend:**
- Limits label length (UI enforced)
- Trims whitespace
- Checks for empty labels

### File System Safety

**Writes:**
- The file is truncated and rewritten; the write is not atomic
- A reset or power loss during the write can leave a partial file

**Error Handling:**
- Gracefully handles missing file
- Returns empty object instead of error

## Troubleshooting

### Labels Not Persisting

**Check:**
1. LittleFS filesystem is mounted correctly
2. File write permissions are correct
3. Sufficient free space on filesystem
4. API POST request completes successfully

**Solution:**
```javascript
// Check API response
const response = await fetch('/api/v2/sensors/labels', {
  method: 'POST',
  body: JSON.stringify(labels)
});
const result = await response.json();
console.log('Update result:', result);
```

### Labels Not Displaying

**Check:**
1. GET request returns expected data
2. JavaScript object is populated
3. Display update function is called
4. No JavaScript errors in console

**Solution:**
```javascript
// Debug label loading
fetch('/api/v2/sensors/labels')
  .then(r => r.json())
  .then(labels => console.log('Loaded labels:', labels))
  .catch(err => console.error('Load error:', err));
```

### File Corruption

**Symptoms:**
- GET returns content that is not valid JSON
- The Web UI shows sensor addresses instead of labels despite previous configuration

**Solution:**
```bash
# Delete /dallas_labels.ini on the File System page (/FSexplorer)

# Or overwrite it via the REST API
# POST empty object to recreate file
curl -X POST http://{device-ip}/api/v2/sensors/labels \
  -H "Content-Type: application/json" \
  -d '{}'
```

## Migration from Previous Versions

### From v1.0.0-rc7 (Settings-Based Storage)

**Previous Implementation:**
- Labels stored in `settingDallasLabels[1024]` RAM buffer
- Saved in `settings.json` file
- Exposed via `/api/v1/otgw/otmonitor` responses

**Migration Steps:**

1. **Backup existing labels** (if any):
   ```bash
   # Extract labels from settings.json before upgrade
   cat /settings.json | grep DallasLabels
   ```

2. **Upgrade firmware** with new file-based implementation

3. **Manually re-enter labels** via Web UI:
   - Click each sensor name
   - Enter label in modal dialog
   - Click Save

**Note:** Automatic migration is not implemented. Labels stored in `settings.json` will be lost after upgrade.

### Web UI Changes

**Before:**
- Labels included in OTmonitor API responses
- Single sensor update via `/api/v1/sensors/label` (POST)

**After:**
- Labels fetched separately via `/api/v2/sensors/labels` (GET)
- Bulk update only via `/api/v2/sensors/labels` (POST)
- Read-modify-write pattern for single sensor updates

**Code Changes Required:**

```javascript
// OLD (single sensor API)
await fetch('/api/v1/sensors/label', {
  method: 'POST',
  body: JSON.stringify({
    address: '28FF64D1841703F1',
    label: 'Living Room'
  })
});

// NEW (bulk API with read-modify-write)
const labels = await fetch('/api/v2/sensors/labels').then(r => r.json());
labels['28FF64D1841703F1'] = 'Living Room';
await fetch('/api/v2/sensors/labels', {
  method: 'POST',
  body: JSON.stringify(labels)
});
```

## OpenAPI Specification

A complete OpenAPI 3.0 specification is available at:
- **File:** `docs/api/openapi-dallas-sensors.yaml`
- **Format:** YAML (OpenAPI 3.0.3)
- **Tools:** Compatible with Swagger UI, Postman, etc.

**View in Swagger UI:**
```bash
# Serve the spec file
python -m http.server 8000

# Open in browser
http://localhost:8000/docs/api/openapi-dallas-sensors.yaml
```

## Related Documentation

- **API Documentation:** `example-api/api-call-responses.txt`
- **API Changes:** `example-api/API_CHANGES_v1.0.0.md`
- **Implementation Guide:** `docs/features/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md`
- **Architecture Decision:** `docs/adr/ADR-033-dallas-sensor-custom-labels-graph-visualization.md`
- **OpenAPI Spec:** `docs/api/openapi-dallas-sensors.yaml`

## References

- Dallas DS18B20 Datasheet: [Maxim Integrated](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)
- LittleFS: [GitHub](https://github.com/littlefs-project/littlefs)
- OpenAPI Specification: [swagger.io](https://swagger.io/specification/)
