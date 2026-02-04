# API Changes in v1.0.0

## New Endpoint: `/api/v1/sensors/label` (v1.0.0-rc7+)

A new endpoint for managing custom labels for Dallas DS18B20 temperature sensors has been added to the V1 API.

### Update Sensor Label
**Endpoint:** `POST /api/v1/sensors/label`

Allows you to assign custom names to Dallas temperature sensors instead of using hex addresses. Labels are limited to 16 characters and are stored persistently in settings.

**Request:**
```json
{
  "address": "28FF64D1841703F1",
  "label": "Living Room"
}
```

**Response:**
```json
{
  "success": true,
  "address": "28FF64D1841703F1",
  "label": "Living Room"
}
```

### Sensor Labels in OTmonitor Data

When custom labels are set, they appear as additional fields in the OTmonitor endpoints:

**V1 Format:**
```json
{
  "otmonitor": [
    {
      "name": "28FF64D1841703F1",
      "value": 21.5,
      "unit": "°C",
      "epoch": 1736899200
    },
    {
      "name": "28FF64D1841703F1_label",
      "value": "Living Room",
      "unit": "",
      "epoch": 1736899200
    }
  ]
}
```

**V2 Format:**
```json
{
  "otmonitor": {
    "28FF64D1841703F1": {
      "value": 21.5,
      "unit": "°C",
      "epoch": 1736899200
    },
    "28FF64D1841703F1_label": {
      "value": "Living Room",
      "unit": "",
      "epoch": 1736899200
    }
  }
}
```

**Notes:**
- Labels default to the sensor's hex address until customized
- Labels are displayed in the Web UI graph and main page
- Click a sensor name in the Web UI to edit its label via a non-blocking modal dialog

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
