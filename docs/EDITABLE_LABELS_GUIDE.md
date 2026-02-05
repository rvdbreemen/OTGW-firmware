# Editable Field Labels Guide

## Overview

The OTGW firmware now supports custom labels for dashboard fields, allowing you to personalize the display names shown in the Web UI. This feature complements the existing Dallas temperature sensor label editing functionality.

## Features

- **Edit OpenTherm Field Labels**: Customize the display names for any OpenTherm field that appears in the dashboard
- **Persistent Storage**: Custom labels are saved to the device and persist across reboots
- **Reset to Default**: Easily restore the original label for any field
- **Visual Feedback**: Editable fields show a dashed underline when you hover over them
- **Dallas Sensor Support**: Continue to use the existing sensor label editing for temperature sensors

## How to Edit a Label

### Web UI Method (Recommended)

1. **Navigate to the Dashboard**: Open the OTGW firmware web interface and go to the Home page where you see the OpenTherm monitor data

2. **Identify Editable Fields**: Fields that can be edited will show:
   - A pointer cursor when you hover over them
   - A dashed underline in blue (light theme) or light blue (dark theme)
   - A tooltip saying "Click to edit label"

3. **Click to Edit**: Click on any editable field label to open the edit dialog

4. **Edit the Label**:
   - The dialog shows:
     - **Field**: The internal field name (e.g., `boilertemperature`)
     - **Default Label**: The original translated label (e.g., `Boiler Temperature`)
     - **Custom Label**: Text input where you can enter your custom label (max 50 characters)
   - Enter your desired label text
   - Press **Enter** or click **Save** to save the change
   - Click **Cancel** or press **Escape** to close without saving

5. **Reset to Default** (Optional):
   - If you want to restore the original label, click **Reset to Default**
   - This will delete your custom label and restore the default translation

### API Method (Advanced)

You can also manage custom labels via the REST API:

#### Get All Custom Labels
```bash
GET /api/v1/labels/custom
```

Response:
```json
{
  "boilertemperature": "Ketel Temperatuur",
  "roomtemperature": "Kamer Temp"
}
```

#### Set a Custom Label
```bash
POST /api/v1/labels/custom
Content-Type: application/json

{
  "field": "boilertemperature",
  "label": "Ketel Temperatuur"
}
```

Response:
```json
{
  "success": true,
  "field": "boilertemperature",
  "label": "Ketel Temperatuur"
}
```

#### Reset Label to Default
```bash
DELETE /api/v1/labels/custom
Content-Type: application/json

{
  "field": "boilertemperature"
}
```

Response:
```json
{
  "success": true,
  "field": "boilertemperature",
  "message": "Label reset to default"
}
```

## Which Fields Can Be Edited?

All OpenTherm fields that have a default translation can be edited. This includes:

- **Temperature Fields**: `boilertemperature`, `roomtemperature`, `outsidetemperature`, `returnwatertemperature`, `dhwtemperature`, etc.
- **Status Fields**: `flamestatus`, `chenable`, `chmodus`, `dhwenable`, `dhwmode`, etc.
- **Setpoints**: `roomsetpoint`, `controlsetpoint`, `maxchwatersetpoint`, `dhwsetpoint`, etc.
- **Diagnostic Fields**: `relmodlvl`, `chwaterpressure`, `oemfaultcode`, etc.
- **Device Information**: `hostname`, `ipaddress`, `fwversion`, etc.
- **Settings**: All MQTT, NTP, and sensor configuration fields

Dallas temperature sensors (16-character hex addresses) continue to use their own dedicated label editing system.

## Use Cases

### Localization
Translate field names to your preferred language:
- English → Dutch: "Room Temperature" → "Kamer Temperatuur"
- English → German: "Boiler Temperature" → "Kesseltemperatur"
- English → French: "Water Pressure" → "Pression d'eau"

### Simplification
Use shorter or simpler names:
- "Domestic Hotwater Temperature" → "DHW Temp"
- "Relative Modulation Level" → "Modulation"
- "Central Heating Water Pressure" → "Water Pressure"

### Personalization
Use names that make sense in your specific setup:
- "Outside Temperature" → "Garden Temp"
- "Room Temperature" → "Living Room"
- "Dallas Sensor 1" → "Attic Temperature"

## Technical Details

### Storage
- Custom labels are stored in the device's LittleFS filesystem in the `settings.ini` file
- Maximum storage: 1024 bytes JSON (approx. 20-30 custom labels depending on length)
- Each label can be up to 50 characters long

### Performance
- Labels are loaded once when the page loads
- Editing a label triggers an immediate save to the device
- The dashboard refreshes automatically after saving to show the new label

### Compatibility
- Works with all modern browsers (Chrome, Firefox, Safari, Edge)
- Respects dark/light theme settings
- Touch-friendly for mobile devices

## Troubleshooting

### Label doesn't save
- Check browser console for error messages
- Ensure label is not empty
- Ensure label is not longer than 50 characters
- Verify device has sufficient free space in LittleFS

### Field is not clickable/editable
- Only fields with default translations can be edited
- Dallas sensors use a separate editing system (different modal)
- Some internal/system fields may not be editable

### Label doesn't appear after editing
- Try refreshing the page (F5)
- Check that the custom label was saved via GET /api/v1/labels/custom
- Ensure you're viewing the correct field

## Examples

### Example 1: Translate to Dutch
Original: "Boiler Temperature"
Custom: "Ketel Temperatuur"

Steps:
1. Click on "Boiler Temperature" field
2. Enter "Ketel Temperatuur" in the custom label field
3. Click Save

### Example 2: Simplify a Long Name
Original: "Domestic Hotwater Temperature"
Custom: "DHW Temp"

Steps:
1. Click on "Domestic Hotwater Temperature" field
2. Enter "DHW Temp"
3. Click Save

### Example 3: Reset to Default
Custom label: "DHW Temp"
Want to restore: "Domestic Hotwater Temperature"

Steps:
1. Click on "DHW Temp" field
2. Click "Reset to Default" button
3. Confirm the reset

## Security Note

Custom labels are stored locally on the device. They are not encrypted, so avoid using sensitive information in label names. Labels are meant for display purposes only and do not affect the functionality of the device.
