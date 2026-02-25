# API Changes in v1.0.0

## New Endpoints: Dallas Sensor Labels (v1.0.0+)

New endpoints for managing custom labels for Dallas DS18B20/DS18S20/DS1822 temperature sensors have been added to the V1 API.

### Architecture

**File-Based Storage:**
- Labels stored in `/dallas_labels.ini` file on LittleFS filesystem
- **Zero backend RAM usage** - labels never loaded into backend memory
- Labels fetched on-demand by Web UI via REST API
- Not exposed in `/api/v1/otgw/otmonitor` or `/api/v2/otgw/otmonitor` responses

**API Design:**
- **Bulk operations only** (no single sensor endpoints)
- Frontend manages label lookup and modification
- Uses read-modify-write pattern for single label updates

### Get All Labels
**Endpoint:** `GET /api/v1/sensors/labels`

Retrieves all Dallas temperature sensor labels from `/dallas_labels.ini` file.

**Response (with labels):**
```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF64D1841703F2": "Kitchen",
  "28FF64D1841703F3": "Bedroom"
}
```

**Response (no labels):**
```json
{}
```

### Update All Labels
**Endpoint:** `POST /api/v1/sensors/labels`

Writes all Dallas temperature sensor labels to `/dallas_labels.ini` file. Replaces entire file contents.

**Request:**
```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF64D1841703F2": "Kitchen"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Labels updated successfully"
}
```

### Usage Patterns

**Fetch labels on page load:**
```javascript
const labels = await fetch('/api/v1/sensors/labels').then(r => r.json());
const label = labels[sensorAddress] || sensorAddress; // Default to hex address
```

**Update single label (read-modify-write):**
```javascript
// 1. Fetch all labels
const labels = await fetch('/api/v1/sensors/labels').then(r => r.json());

// 2. Modify one label
labels['28FF64D1841703F1'] = "New Label";

// 3. Write all labels back
await fetch('/api/v1/sensors/labels', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify(labels)
});
```

**Notes:**
- Labels default to the sensor's hex address until customized
- Labels are displayed in the Web UI graph and main page
- Click a sensor name in the Web UI to edit its label via a non-blocking modal dialog
- Maximum 16 characters per label (recommended)
- Labels persist across reboots (stored in file)

---

## New Endpoint: `/api/v2/otgw/otmonitor`

A new optimization has been introduced for the `otmonitor` endpoint. The existing `/api/v1/` endpoint remains unchanged for backward compatibility.

### New V2 Format (Key-Value Map) - available at `/api/v2/otgw/otmonitor`

The V2 format uses a key-value map where the key is the property name. This reduces payload size by removing the redundant "name" field for each entry.

```json
{
  "otmonitor": {
    "flamestatus": {
      "value": "Off",
      "unit": "",
      "epoch": 1736899200
    },
    "roomtemperature": {
      "value": 20.500,
      "unit": "°C",
      "epoch": 1736899200
    }
  }
}
```

### Legacy V1 Format (Array of Objects) - remains at `/api/v1/otgw/otmonitor`

```json
{
  "otmonitor": [
    {
      "name": "flamestatus",
      "value": "Off",
      "unit": "",
      "epoch": 1736899200
    },
    {
      "name": "roomtemperature",
      "value": 20.500,
      "unit": "°C",
      "epoch": 1736899200
    }
  ]
}
```

### Migration Guide for Clients

**JavaScript Example:**

*Old:*
```javascript
let tempObj = data.otmonitor.find(item => item.name === 'roomtemperature');
let temp = tempObj ? tempObj.value : null;
```

*New:*
```javascript
let temp = data.otmonitor.roomtemperature ? data.otmonitor.roomtemperature.value : null;
```

**Note:** The WebUI (`index.js`) included in this release supports both formats for backward compatibility during the upgrade process, but the firmware now exclusively emits the new format.
