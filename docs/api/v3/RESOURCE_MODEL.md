# REST API v3 Resource Model

**Version:** 3.0.0  
**Status:** Design Specification  
**Last Updated:** 2026-01-31

---

## Table of Contents

- [Overview](#overview)
- [Resource Hierarchy](#resource-hierarchy)
- [System Resources](#system-resources)
- [Configuration Resources](#configuration-resources)
- [OpenTherm Gateway Resources](#opentherm-gateway-resources)
- [PIC Firmware Resources](#pic-firmware-resources)
- [Sensor Resources](#sensor-resources)
- [Export Resources](#export-resources)
- [URI Design Principles](#uri-design-principles)
- [Query Parameters](#query-parameters)

---

## Overview

The v3 API follows a **resource-oriented architecture** where every endpoint represents a resource (noun) manipulated through standard HTTP methods. This document defines the complete resource hierarchy and URI structure.

### Design Principles

1. **Resources are nouns, not verbs**
2. **Hierarchical structure** reflects logical relationships
3. **Consistent naming** (singular vs plural)
4. **Query parameters** for filtering, pagination
5. **HATEOAS links** for discoverability

---

## Resource Hierarchy

```
/api/v3/
├── system/                          # Device system information
│   ├── info                         # GET: Hardware/software details
│   ├── health                       # GET: Health check
│   ├── time                         # GET: Current time
│   ├── network                      # GET: Network status
│   └── stats                        # GET: Runtime statistics
│
├── config/                          # Configuration resources
│   ├── settings                     # GET, PUT, PATCH: All settings
│   ├── mqtt                         # GET, PATCH: MQTT configuration
│   └── ntp                          # GET, PATCH: NTP configuration
│
├── otgw/                            # OpenTherm Gateway
│   ├── status                       # GET: Current status
│   ├── messages                     # GET: All OT messages (paginated)
│   ├── messages/{id}                # GET: Specific message by ID (0-127)
│   ├── thermostat                   # GET: Thermostat aggregated data
│   ├── boiler                       # GET: Boiler aggregated data
│   ├── commands                     # POST: Submit command, GET: Queue status
│   └── autoconfigure                # POST: Trigger HA autoconfigure
│
├── pic/                             # PIC firmware
│   ├── firmware                     # GET: Firmware info
│   └── flash/
│       └── status                   # GET: Flash operation status
│
├── sensors/                         # Sensors
│   ├── dallas                       # GET: All Dallas sensors
│   ├── dallas/{address}             # GET: Specific Dallas sensor
│   └── s0                           # GET: S0 pulse counter
│
└── export/                          # Export formats (integration)
    ├── telegraf                     # GET: Telegraf-compatible JSON
    ├── otmonitor                    # GET: OTmonitor-compatible JSON
    └── prometheus                   # GET: Prometheus metrics (future)
```

---

## System Resources

### GET /api/v3/system

**Purpose:** System overview with links to sub-resources

**Response:**
```json
{
  "system": {
    "status": "online",
    "version": "1.0.0",
    "uptime": 86400,
    "free_heap": 12345,
    "_links": {
      "self": {"href": "/api/v3/system"},
      "info": {"href": "/api/v3/system/info"},
      "health": {"href": "/api/v3/system/health"},
      "time": {"href": "/api/v3/system/time"},
      "network": {"href": "/api/v3/system/network"},
      "stats": {"href": "/api/v3/system/stats"}
    }
  }
}
```

---

### GET /api/v3/system/info

**Purpose:** Detailed device information

**Response:**
```json
{
  "info": {
    "firmware": {
      "version": "1.0.0",
      "compiled": "2026-01-31 10:30:00",
      "author": "Robert van den Breemen"
    },
    "hardware": {
      "chip_id": "ABC123",
      "flash_size": 4194304,
      "flash_speed": 40,
      "cpu_freq": 80
    },
    "pic": {
      "available": true,
      "version": "5.0",
      "device_id": "16F88",
      "type": "gateway"
    },
    "_links": {
      "self": {"href": "/api/v3/system/info"}
    }
  }
}
```

---

### GET /api/v3/system/health

**Purpose:** Health check endpoint for monitoring

**Response:**
```json
{
  "health": {
    "status": "UP",
    "uptime": 86400,
    "heap": 12345,
    "wifi_rssi": -65,
    "mqtt_connected": true,
    "otgw_connected": true,
    "pic_available": true,
    "littlefs_mounted": true,
    "_links": {
      "self": {"href": "/api/v3/system/health"}
    }
  }
}
```

**Status Values:**
- `UP` - All systems operational
- `DEGRADED` - Some issues but functional
- `DOWN` - Critical failure

---

### GET /api/v3/system/time

**Purpose:** Current system time

**Response:**
```json
{
  "time": {
    "datetime": "2026-01-31 10:30:45",
    "epoch": 1738324245,
    "timezone": "Europe/Amsterdam",
    "ntp_enabled": true,
    "ntp_synced": true,
    "_links": {
      "self": {"href": "/api/v3/system/time"}
    }
  }
}
```

---

### GET /api/v3/system/network

**Purpose:** Network information

**Response:**
```json
{
  "network": {
    "hostname": "OTGW",
    "ip_address": "192.168.1.100",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "ssid": "MyNetwork",
    "rssi": -65,
    "wifi_quality": "Good",
    "_links": {
      "self": {"href": "/api/v3/system/network"}
    }
  }
}
```

---

### GET /api/v3/system/stats

**Purpose:** Runtime statistics

**Response:**
```json
{
  "stats": {
    "uptime": 86400,
    "boot_count": 42,
    "last_reset": "Power on",
    "free_heap": 12345,
    "max_free_block": 11000,
    "heap_fragmentation": 10,
    "sketch_size": 450560,
    "free_sketch_space": 2000000,
    "_links": {
      "self": {"href": "/api/v3/system/stats"}
    }
  }
}
```

---

## Configuration Resources

### GET /api/v3/config

**Purpose:** Configuration overview

**Response:**
```json
{
  "config": {
    "summary": {
      "hostname": "OTGW",
      "mqtt_enabled": true,
      "ntp_enabled": true
    },
    "_links": {
      "self": {"href": "/api/v3/config"},
      "settings": {"href": "/api/v3/config/settings"},
      "mqtt": {"href": "/api/v3/config/mqtt"},
      "ntp": {"href": "/api/v3/config/ntp"}
    }
  }
}
```

---

### GET /api/v3/config/settings

**Purpose:** Get all device settings

**Response:**
```json
{
  "settings": {
    "hostname": "OTGW",
    "mqttenable": true,
    "mqttbroker": "192.168.1.50",
    "mqttbrokerport": 1883,
    "ntpenable": true,
    "ntptimezone": "Europe/Amsterdam",
    "ledblink": true,
    "darktheme": false,
    ... (all settings)
  }
}
```

---

### PUT /api/v3/config/settings

**Purpose:** Replace all settings

**Request:**
```json
{
  "hostname": "OTGW-New",
  "mqttenable": true,
  ... (all required fields)
}
```

**Response:** 200 OK with updated settings

---

### PATCH /api/v3/config/settings

**Purpose:** Update specific settings (partial update)

**Request:**
```json
{
  "hostname": "OTGW-New",
  "mqttenable": true
}
```

**Response:**
```json
{
  "updated": ["hostname", "mqttenable"],
  "settings": {
    "hostname": "OTGW-New",
    "mqttenable": true
  }
}
```

---

### GET /api/v3/config/mqtt

**Purpose:** Get MQTT configuration

**Response:**
```json
{
  "mqtt": {
    "enabled": true,
    "broker": "192.168.1.50",
    "port": 1883,
    "user": "otgw",
    "top_topic": "otgw",
    "ha_prefix": "homeassistant",
    "ha_reboot_detection": true,
    "unique_id": "otgw-12345",
    "ot_message": true,
    "_links": {
      "self": {"href": "/api/v3/config/mqtt"}
    }
  }
}
```

---

### PATCH /api/v3/config/mqtt

**Purpose:** Update MQTT settings

**Request:**
```json
{
  "broker": "192.168.1.100",
  "port": 1883
}
```

**Response:** 200 OK with updated MQTT configuration

---

### GET /api/v3/config/ntp

**Purpose:** Get NTP configuration

**Response:**
```json
{
  "ntp": {
    "enabled": true,
    "timezone": "Europe/Amsterdam",
    "hostname": "pool.ntp.org",
    "send_time": true,
    "_links": {
      "self": {"href": "/api/v3/config/ntp"}
    }
  }
}
```

---

### PATCH /api/v3/config/ntp

**Purpose:** Update NTP settings

**Request:**
```json
{
  "timezone": "America/New_York"
}
```

**Response:** 200 OK with updated NTP configuration

---

## OpenTherm Gateway Resources

### GET /api/v3/otgw

**Purpose:** OTGW overview

**Response:**
```json
{
  "otgw": {
    "connected": true,
    "thermostat_connected": true,
    "boiler_connected": true,
    "gateway_mode": true,
    "_links": {
      "self": {"href": "/api/v3/otgw"},
      "status": {"href": "/api/v3/otgw/status"},
      "messages": {"href": "/api/v3/otgw/messages"},
      "thermostat": {"href": "/api/v3/otgw/thermostat"},
      "boiler": {"href": "/api/v3/otgw/boiler"},
      "commands": {"href": "/api/v3/otgw/commands"}
    }
  }
}
```

---

### GET /api/v3/otgw/status

**Purpose:** Current OTGW status

**Response:**
```json
{
  "status": {
    "online": true,
    "thermostat_connected": true,
    "boiler_connected": true,
    "gateway_mode": true,
    "flame_status": true,
    "ch_enabled": true,
    "ch_active": true,
    "dhw_enabled": true,
    "dhw_active": false,
    "_links": {
      "self": {"href": "/api/v3/otgw/status"}
    }
  }
}
```

---

### GET /api/v3/otgw/messages

**Purpose:** All OpenTherm messages (paginated)

**Query Parameters:**
- `page` - Page number (default: 1)
- `per_page` - Items per page (default: 20, max: 128)
- `label` - Filter by label (e.g., "roomtemperature")
- `category` - Filter by category (e.g., "temperature", "status")
- `updated_after` - Unix timestamp

**Response:**
```json
{
  "messages": [
    {
      "id": 0,
      "label": "statusflags",
      "value": 258,
      "unit": "",
      "last_updated": 1738324245
    },
    ...
  ],
  "pagination": {
    "page": 1,
    "per_page": 20,
    "total": 128,
    "total_pages": 7
  },
  "_links": {
    "self": {"href": "/api/v3/otgw/messages?page=1&per_page=20"},
    "first": {"href": "/api/v3/otgw/messages?page=1&per_page=20"},
    "next": {"href": "/api/v3/otgw/messages?page=2&per_page=20"},
    "last": {"href": "/api/v3/otgw/messages?page=7&per_page=20"}
  }
}
```

---

### GET /api/v3/otgw/messages/{id}

**Purpose:** Get specific OpenTherm message by ID (0-127)

**Response:**
```json
{
  "message": {
    "id": 24,
    "label": "roomtemperature",
    "value": 20.5,
    "unit": "°C",
    "last_updated": 1738324245,
    "_links": {
      "self": {"href": "/api/v3/otgw/messages/24"},
      "collection": {"href": "/api/v3/otgw/messages"}
    }
  }
}
```

---

### GET /api/v3/otgw/thermostat

**Purpose:** Aggregated thermostat data

**Response:**
```json
{
  "thermostat": {
    "connected": true,
    "room_temperature": 20.5,
    "room_setpoint": 21.0,
    "remote_override": null,
    "enabled": true,
    "_links": {
      "self": {"href": "/api/v3/otgw/thermostat"},
      "messages": {"href": "/api/v3/otgw/messages?category=thermostat"}
    }
  }
}
```

---

### GET /api/v3/otgw/boiler

**Purpose:** Aggregated boiler data

**Response:**
```json
{
  "boiler": {
    "connected": true,
    "flame_status": true,
    "temperature": 65.0,
    "return_temperature": 55.0,
    "pressure": 1.5,
    "modulation_level": 75,
    "max_modulation": 100,
    "dhw_temperature": 50.0,
    "dhw_setpoint": 60.0,
    "_links": {
      "self": {"href": "/api/v3/otgw/boiler"},
      "messages": {"href": "/api/v3/otgw/messages?category=boiler"}
    }
  }
}
```

---

### POST /api/v3/otgw/commands

**Purpose:** Submit command to OTGW queue

**Request:**
```json
{
  "command": "TT=20.5",
  "description": "Set temporary temperature override"
}
```

**Response:** 201 Created
```json
{
  "command": {
    "id": "12345",
    "command": "TT=20.5",
    "status": "queued",
    "queue_position": 2,
    "submitted_at": 1738324245,
    "_links": {
      "self": {"href": "/api/v3/otgw/commands/12345"},
      "queue": {"href": "/api/v3/otgw/commands"}
    }
  }
}
```

---

### GET /api/v3/otgw/commands

**Purpose:** Get command queue status

**Response:**
```json
{
  "queue": {
    "size": 2,
    "commands": [
      {
        "id": "12345",
        "command": "TT=20.5",
        "status": "pending",
        "queue_position": 1
      },
      {
        "id": "12346",
        "command": "SW=60",
        "status": "pending",
        "queue_position": 2
      }
    ],
    "_links": {
      "self": {"href": "/api/v3/otgw/commands"}
    }
  }
}
```

---

### POST /api/v3/otgw/autoconfigure

**Purpose:** Trigger Home Assistant MQTT auto-discovery

**Response:** 202 Accepted
```json
{
  "message": "Auto-configuration triggered",
  "status": "processing"
}
```

---

## PIC Firmware Resources

### GET /api/v3/pic

**Purpose:** PIC firmware overview

**Response:**
```json
{
  "pic": {
    "available": true,
    "version": "5.0",
    "device_id": "16F88",
    "type": "gateway",
    "_links": {
      "self": {"href": "/api/v3/pic"},
      "firmware": {"href": "/api/v3/pic/firmware"},
      "flash_status": {"href": "/api/v3/pic/flash/status"}
    }
  }
}
```

---

### GET /api/v3/pic/firmware

**Purpose:** PIC firmware information

**Response:**
```json
{
  "firmware": {
    "version": "5.0",
    "device_id": "16F88",
    "type": "gateway",
    "available": true,
    "_links": {
      "self": {"href": "/api/v3/pic/firmware"}
    }
  }
}
```

---

### GET /api/v3/pic/flash/status

**Purpose:** PIC flash operation status (for polling during upgrade)

**Response:**
```json
{
  "flash": {
    "flashing": false,
    "progress": 0,
    "filename": "",
    "error": "",
    "_links": {
      "self": {"href": "/api/v3/pic/flash/status"}
    }
  }
}
```

---

## Sensor Resources

### GET /api/v3/sensors

**Purpose:** All sensors overview

**Response:**
```json
{
  "sensors": {
    "dallas": {
      "enabled": true,
      "count": 2
    },
    "s0": {
      "enabled": true
    },
    "_links": {
      "self": {"href": "/api/v3/sensors"},
      "dallas": {"href": "/api/v3/sensors/dallas"},
      "s0": {"href": "/api/v3/sensors/s0"}
    }
  }
}
```

---

### GET /api/v3/sensors/dallas

**Purpose:** All Dallas temperature sensors

**Query Parameters:**
- `connected` - Filter by connection status (true/false)

**Response:**
```json
{
  "sensors": [
    {
      "address": "28AABBCCDDEE0011",
      "temperature": 20.5,
      "unit": "°C",
      "connected": true,
      "last_updated": 1738324245,
      "_links": {
        "self": {"href": "/api/v3/sensors/dallas/28AABBCCDDEE0011"}
      }
    },
    ...
  ],
  "_links": {
    "self": {"href": "/api/v3/sensors/dallas"}
  }
}
```

---

### GET /api/v3/sensors/dallas/{address}

**Purpose:** Specific Dallas sensor by address

**Response:**
```json
{
  "sensor": {
    "address": "28AABBCCDDEE0011",
    "temperature": 20.5,
    "unit": "°C",
    "connected": true,
    "last_updated": 1738324245,
    "_links": {
      "self": {"href": "/api/v3/sensors/dallas/28AABBCCDDEE0011"},
      "collection": {"href": "/api/v3/sensors/dallas"}
    }
  }
}
```

---

### GET /api/v3/sensors/s0

**Purpose:** S0 pulse counter data

**Response:**
```json
{
  "s0": {
    "enabled": true,
    "power_kw": 2.5,
    "interval_count": 10,
    "total_count": 123456,
    "last_updated": 1738324245,
    "_links": {
      "self": {"href": "/api/v3/sensors/s0"}
    }
  }
}
```

---

## Export Resources

### GET /api/v3/export/telegraf

**Purpose:** Telegraf-compatible format (array of metrics)

**Response:**
```json
[
  {
    "name": "flamestatus",
    "value": 1,
    "unit": "",
    "epoch": 1738324245
  },
  {
    "name": "roomtemperature",
    "value": 20.5,
    "unit": "°C",
    "epoch": 1738324245
  },
  ...
]
```

---

### GET /api/v3/export/otmonitor

**Purpose:** OTmonitor-compatible format (map)

**Response:**
```json
{
  "otmonitor": {
    "flamestatus": {
      "value": "On",
      "unit": "",
      "epoch": 1738324245
    },
    "roomtemperature": {
      "value": 20.5,
      "unit": "°C",
      "epoch": 1738324245
    },
    ...
  }
}
```

---

### GET /api/v3/export/prometheus

**Purpose:** Prometheus metrics format (future)

**Response:** (text/plain)
```
# HELP otgw_room_temperature Room temperature in Celsius
# TYPE otgw_room_temperature gauge
otgw_room_temperature 20.5
...
```

---

## URI Design Principles

### 1. Use Nouns, Not Verbs

✅ **GOOD:**
```
POST /api/v3/otgw/commands
GET /api/v3/system/health
```

❌ **BAD:**
```
POST /api/v3/otgw/sendCommand
GET /api/v3/system/checkHealth
```

### 2. Singular vs Plural

- **Collections:** Plural (`/messages`, `/sensors`)
- **Single Resources:** Singular (`/health`, `/status`)
- **Sub-resources:** Follow parent (`/sensors/dallas/{address}`)

### 3. Hierarchical Structure

Reflect logical relationships:
```
/api/v3/otgw/messages/24
        └─┬─┘ └───┬───┘ └┬┘
      resource  collection id
```

### 4. Consistent Depth

Limit nesting to 3-4 levels:
```
✅ /api/v3/sensors/dallas/28AABBCCDDEE0011
❌ /api/v3/system/network/wifi/status/connection/quality
```

### 5. Lowercase and Hyphens

Use lowercase, separate words with hyphens:
```
✅ /api/v3/system/free-heap
❌ /api/v3/system/FreeHeap
❌ /api/v3/system/free_heap
```

---

## Query Parameters

### Pagination

- `page` - Page number (1-based, default: 1)
- `per_page` - Items per page (default: 20, max: 128)

**Example:**
```
GET /api/v3/otgw/messages?page=2&per_page=10
```

### Filtering

- `label` - Filter by label name
- `category` - Filter by category
- `connected` - Boolean filter (true/false)
- `updated_after` - Unix timestamp

**Examples:**
```
GET /api/v3/otgw/messages?label=roomtemperature
GET /api/v3/otgw/messages?category=temperature
GET /api/v3/sensors/dallas?connected=true
GET /api/v3/otgw/messages?updated_after=1738320000
```

### Combining Parameters

Multiple parameters use AND logic:
```
GET /api/v3/otgw/messages?category=temperature&page=1&per_page=10
```

---

## See Also

- [ADR-025: REST API v3 Design](../adr/ADR-025-rest-api-v3-design.md)
- [Error Responses](ERROR_RESPONSES.md)
- [HTTP Status Codes](HTTP_STATUS_CODES.md)
- [Response Headers](RESPONSE_HEADERS.md)
- [Implementation Plan](../planning/REST_API_V3_MODERNIZATION_PLAN.md)
