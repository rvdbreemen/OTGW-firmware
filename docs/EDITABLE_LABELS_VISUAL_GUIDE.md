# Editable Field Labels - Visual Guide

## UI Components

### 1. Editable Field Indicators

When you hover over an editable field in the OT Monitor display:

```
┌────────────────────────────────────────────┐
│  OpenTherm Monitor                         │
├────────────────────────────────────────────┤
│                                            │
│  🔥 Flame status            On             │
│  🌡️  Boiler Temperature    65.5  °C       │
│       ︿︿︿︿︿︿︿︿︿︿︿︿︿︿︿︿  ← Dashed underline on hover
│  💧 Water Pressure          1.5   bar      │
│  📊 Modulation Level        45    %        │
│                                            │
└────────────────────────────────────────────┘
```

**Visual feedback:**
- ✅ Pointer cursor when hovering
- ✅ Dashed blue underline appears on hover
- ✅ Tooltip: "Click to edit label"

### 2. Edit Label Modal Dialog

When you click on an editable field, a modal dialog appears:

```
┌───────────────────────────────────────────────────┐
│  Edit Field Label                              ✕  │
├───────────────────────────────────────────────────┤
│                                                   │
│  Field: boilertemperature                         │
│  Default Label: Boiler Temperature                │
│                                                   │
│  Custom Label (max 50 characters):                │
│  ┌─────────────────────────────────────────────┐  │
│  │ Ketel Temperatuur                          │  │
│  └─────────────────────────────────────────────┘  │
│                                                   │
├───────────────────────────────────────────────────┤
│                 [Reset to Default] [Cancel] [Save]│
└───────────────────────────────────────────────────┘
```

**Button colors:**
- 🟨 **Reset to Default** - Yellow/amber (warning color)
- ⬜ **Cancel** - Gray (neutral)
- 🔵 **Save** - Blue (primary action)

### 3. After Editing

Once saved, the custom label appears in the dashboard:

```
┌────────────────────────────────────────────┐
│  OpenTherm Monitor                         │
├────────────────────────────────────────────┤
│                                            │
│  🔥 Vlam status             Aan            │  ← Custom label (Dutch)
│  🌡️  Ketel Temperatuur      65.5  °C       │  ← Custom label (Dutch)
│  💧 Water Druk              1.5   bar      │  ← Custom label (Dutch)
│  📊 Modulatie Niveau        45    %        │  ← Custom label (Dutch)
│                                            │
└────────────────────────────────────────────┘
```

## Dallas Sensor Labels

Dallas temperature sensors use a separate modal (existing functionality):

```
┌───────────────────────────────────────────────────┐
│  Edit Sensor Label                             ✕  │
├───────────────────────────────────────────────────┤
│                                                   │
│  Sensor Address: 28A1B2C3D4E5F607                 │
│                                                   │
│  Custom Label (max 16 characters):                │
│  ┌─────────────────────────────────────────────┐  │
│  │ Attic Temperature                          │  │
│  └─────────────────────────────────────────────┘  │
│                                                   │
├───────────────────────────────────────────────────┤
│                                 [Cancel]   [Save] │
└───────────────────────────────────────────────────┘
```

**Key difference:**
- Dallas sensors: 16 character limit (hardware constraint)
- OpenTherm fields: 50 character limit (display flexibility)

## Theme Support

### Light Theme
- Modal background: White (#ffffff)
- Text: Dark gray (#333)
- Underline on hover: Blue (#007bff)
- Save button: Blue (#007bff)
- Reset button: Amber/yellow (#ffc107)

### Dark Theme  
- Modal background: Dark gray (#2a2a2a)
- Text: Light gray (#e0e0e0)
- Underline on hover: Light blue (#4a90e2)
- Save button: Light blue (#4a90e2)
- Reset button: Dark amber (#d39e00)

## Mobile Support

On touch devices:
- Tap to edit (no hover state needed)
- Modal is responsive and sized to fit screen
- Touch-friendly button sizes
- Keyboard automatically appears when focusing input

## Workflow Diagram

```
┌──────────────┐
│ User views   │
│ dashboard    │
└──────┬───────┘
       │
       ▼
┌──────────────┐     Yes    ┌──────────────┐
│ Hover over   ├──────────→ │ Show dashed  │
│ field label  │            │ underline    │
└──────┬───────┘            └──────────────┘
       │
       ▼
┌──────────────┐     Yes    ┌──────────────┐
│ Click on     ├──────────→ │ Open edit    │
│ label        │            │ modal        │
└──────────────┘            └──────┬───────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
                    ▼                             ▼
            ┌───────────────┐            ┌────────────────┐
            │ Edit label    │            │ Reset to       │
            │ and save      │            │ default        │
            └───────┬───────┘            └────────┬───────┘
                    │                             │
                    └──────────┬──────────────────┘
                               │
                               ▼
                    ┌──────────────────┐
                    │ Save to device   │
                    │ via API          │
                    └──────────┬───────┘
                               │
                               ▼
                    ┌──────────────────┐
                    │ Refresh display  │
                    │ with new label   │
                    └──────────────────┘
```

## API Flow

### Setting a Custom Label

```
Browser                      ESP8266
   │                            │
   │  POST /api/v1/labels/custom│
   │  {"field": "boilertemp",   │
   │   "label": "Ketel Temp"}   │
   ├───────────────────────────→│
   │                            │ Parse JSON
   │                            │ Validate
   │                            │ Save to settings
   │                            │ Write to LittleFS
   │                            │
   │  {"success": true, ...}    │
   │←───────────────────────────┤
   │                            │
   │  Refresh dashboard         │
   │                            │
```

### Loading Custom Labels

```
Browser                      ESP8266
   │                            │
   │  GET /api/v1/labels/custom │
   ├───────────────────────────→│
   │                            │ Read from settings
   │                            │ Return JSON
   │                            │
   │  {"boilertemp":            │
   │   "Ketel Temp", ...}       │
   │←───────────────────────────┤
   │                            │
   │  Cache in customLabels     │
   │  global variable           │
   │                            │
```

## Code Organization

```
Frontend (data/):
├── index.html
│   └── Field Label Modal HTML structure
├── index.css
│   └── Modal and editable label styling (light theme)
├── index_dark.css
│   └── Modal and editable label styling (dark theme)
└── index.js
    ├── customLabels object (global cache)
    ├── loadCustomLabels() - Load from API on init
    ├── translateToHuman() - Check custom labels first
    ├── editFieldLabel() - Open modal
    ├── saveFieldLabelFromModal() - Save to API
    ├── resetFieldLabelToDefault() - Reset via API
    └── closeFieldLabelModal() - Close modal

Backend (firmware/):
├── OTGW-firmware.h
│   └── settingCustomLabels[1024] - JSON storage
├── settingStuff.ino
│   └── Persistence (read/write to settings file)
├── helperStuff.ino
│   ├── loadCustomLabel()
│   ├── saveCustomLabel()
│   ├── deleteCustomLabel()
│   └── getCustomLabelsJson()
└── restAPI.ino
    ├── sendCustomLabels() - GET handler
    ├── updateCustomLabel() - POST handler
    └── deleteCustomLabelAPI() - DELETE handler
```

## Storage Format

Settings file (`/settings.ini` in LittleFS):

```json
{
  ...other settings...
  "CustomLabels": "{\"boilertemperature\":\"Ketel Temp\",\"roomtemperature\":\"Kamer Temp\"}"
  ...
}
```

The CustomLabels field contains a JSON string with field-to-label mappings. This nested JSON structure allows efficient storage and retrieval.
