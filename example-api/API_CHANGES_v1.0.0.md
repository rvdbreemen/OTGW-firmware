# API Changes in v1.0.0

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
```      "unit": "°C",
      "epoch": 1736899200
    }
  ]
}
```

### New Format (Map/Object) - >= v1.0.0

The `name` property has been removed from the individual objects and is now used as the key for the object.

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
