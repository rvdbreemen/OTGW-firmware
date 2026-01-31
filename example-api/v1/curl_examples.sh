#!/bin/bash
#
# OTGW REST API v1 - cURL Examples
# Version: 1.0.0
#
# This file contains practical cURL examples for the OTGW REST API v1.
# The v1 API provides simple HTTP endpoints for basic device operations.
#
# Usage: 
#   bash curl_examples.sh
#   Or source individual examples
#
# Note: Replace DEVICE_IP with your OTGW device IP address (default: 192.168.1.100)
#

DEVICE_IP="${DEVICE_IP:-192.168.1.100}"
DEVICE_URL="http://${DEVICE_IP}"
API_V1="${DEVICE_URL}/api/v1"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== OTGW REST API v1 Examples ===${NC}\n"

# ============================================================================
# 1. HEALTH CHECK - Monitor device health and system status
# ============================================================================
echo -e "${YELLOW}1. HEALTH CHECK${NC}"
echo "Get device health status, OpenTherm status, and PIC availability"
echo ""
echo "Request:"
echo "  GET /api/v1/health"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/health | jq ."
echo ""
echo "Response example:"
cat << 'EOF'
{
  "uptime": 3600,
  "status": "UP",
  "otgw_connected": true,
  "picavailable": true,
  "mqtt_connected": false,
  "wifi_rssi": -45,
  "heap_free": 25600,
  "heap_max": 40960
}
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/health" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/health"
echo ""
echo ""

# ============================================================================
# 2. FLASH STATUS - Check firmware and PIC flash status
# ============================================================================
echo -e "${YELLOW}2. UNIFIED FLASH STATUS${NC}"
echo "Get flash status for both ESP and PIC firmware"
echo ""
echo "Request:"
echo "  GET /api/v1/flashstatus"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/flashstatus | jq ."
echo ""
echo "Response example:"
cat << 'EOF'
{
  "esp_flash_ok": true,
  "pic_flash_in_progress": false,
  "pic_available": true
}
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/flashstatus" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/flashstatus"
echo ""
echo ""

# ============================================================================
# 3. PIC FLASH STATUS - Poll during PIC firmware upgrade
# ============================================================================
echo -e "${YELLOW}3. PIC FLASH STATUS (Polling)${NC}"
echo "Minimal endpoint for polling PIC flash state during upgrade"
echo ""
echo "Request:"
echo "  GET /api/v1/pic/flashstatus"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/pic/flashstatus | jq ."
echo ""
echo "Response example:"
cat << 'EOF'
{
  "pic_flash_in_progress": false,
  "progress_percent": 0
}
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/pic/flashstatus" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/pic/flashstatus"
echo ""
echo ""

# ============================================================================
# 4. GET DEVICE SETTINGS - Retrieve all device configuration
# ============================================================================
echo -e "${YELLOW}4. GET DEVICE SETTINGS${NC}"
echo "Retrieve all device configuration settings"
echo ""
echo "Request:"
echo "  GET /api/v1/settings"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/settings | jq ."
echo ""
echo "Key settings include:"
echo "  - hostname: Device name"
echo "  - mqtt_broker: MQTT broker address"
echo "  - mqtt_port: MQTT broker port"
echo "  - mqtt_prefix: MQTT topic prefix"
echo "  - ntp_server: NTP time server"
echo "  - tz_info: Timezone"
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/settings" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/settings"
echo ""
echo ""

# ============================================================================
# 5. UPDATE DEVICE SETTINGS - Modify device configuration
# ============================================================================
echo -e "${YELLOW}5. UPDATE DEVICE SETTINGS${NC}"
echo "Update device configuration (hostname, MQTT settings, timezone, etc.)"
echo ""
echo "Request:"
echo "  POST /api/v1/settings"
echo "  Content-Type: application/json"
echo ""
echo "cURL command:"
cat << 'EOF'
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{
    "hostname": "OpenTherm-Gateway",
    "mqtt_broker": "192.168.1.50",
    "mqtt_port": 1883,
    "mqtt_prefix": "otgw",
    "ntp_server": "pool.ntp.org",
    "tz_info": "CET-1CEST,M3.5.0,M10.5.0"
  }' \
  http://DEVICE_IP/api/v1/settings
EOF
echo ""
echo -e "${RED}Note: POST /api/v1/settings is a write operation${NC}"
echo ""
echo ""

# ============================================================================
# 6. TELEGRAF FORMAT - Export data in Telegraf/InfluxDB format
# ============================================================================
echo -e "${YELLOW}6. TELEGRAF FORMAT${NC}"
echo "Export OpenTherm data in Telegraf/InfluxDB line protocol format"
echo ""
echo "Request:"
echo "  GET /api/v1/otgw/telegraf"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/telegraf"
echo ""
echo "Response example (InfluxDB line protocol):"
cat << 'EOF'
otgw_system,host=gateway,version=1.0.0 uptime=3600i,heap_free=25600i,heap_max=40960i 1609459200000000000
otgw_heating,host=gateway room_temp=21.5,setpoint=22.0,boiler_temp=55.3,modulation=80.5 1609459200000000000
otgw_dhw,host=gateway dhw_temp=45.2,dhw_setpoint=50.0,dhw_active=1i 1609459200000000000
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/otgw/telegraf"
echo ""
echo ""

# ============================================================================
# 7. OTMONITOR FORMAT - Export data in OTmonitor format
# ============================================================================
echo -e "${YELLOW}7. OTMONITOR FORMAT${NC}"
echo "Export OpenTherm data in OTmonitor format (multi-line key=value)"
echo ""
echo "Request:"
echo "  GET /api/v1/otgw/otmonitor"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/otmonitor"
echo ""
echo "Response example:"
cat << 'EOF'
Boiler External setpoint from remote:	0
Boiler Status:			84 (CH enabled, DHW enabled, Flame on, ModMode)
Control Setpoint Temperature:		21.5
Boiler Setpoint Temperature:		60.0
Boiler Actual Temperature:		54.3
DHW Setpoint Temperature:		50.0
DHW Actual Temperature:			45.2
Modulation:				80.5
Pump modulation:			30.0
Room Setpoint Temperature:		22.0
Room Temperature:			21.5
Air Pressure:				1015 mbar
Along with 40+ other OpenTherm parameters...
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/otgw/otmonitor" | head -20
echo "... (truncated for brevity)"
echo ""
echo ""

# ============================================================================
# 8. GET OTGW VALUE BY MESSAGE ID - Query specific OpenTherm message
# ============================================================================
echo -e "${YELLOW}8. GET OTGW VALUE BY MESSAGE ID${NC}"
echo "Query a specific OpenTherm message by numeric ID"
echo ""
echo "Request:"
echo "  GET /api/v1/otgw/id/{msgid}"
echo ""
echo "Common message IDs:"
echo "  0   = Status"
echo "  1   = Control Setpoint"
echo "  16  = Room Temperature"
echo "  17  = Boiler Temperature"
echo "  18  = DHW Setpoint"
echo "  24  = Modulation Level"
echo ""
echo "cURL examples:"
echo "  # Get Status (ID 0)"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/id/0 | jq ."
echo ""
echo "  # Get Room Temperature (ID 16)"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/id/16 | jq ."
echo ""
echo "Response example:"
cat << 'EOF'
{
  "id": 16,
  "label": "Room Setpoint",
  "value": 21.5,
  "unit": "°C",
  "type": "float"
}
EOF
echo ""
echo -e "${GREEN}Execute (Status):${NC}"
curl -s "${API_V1}/otgw/id/0" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/otgw/id/0"
echo ""
echo ""

# ============================================================================
# 9. GET OTGW VALUE BY LABEL - Query by human-readable label
# ============================================================================
echo -e "${YELLOW}9. GET OTGW VALUE BY LABEL${NC}"
echo "Query OpenTherm message by human-readable label"
echo ""
echo "Request:"
echo "  GET /api/v1/otgw/label/{label}"
echo ""
echo "cURL examples:"
echo "  # Get Room Temperature"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/label/roomtemp | jq ."
echo ""
echo "  # Get Boiler Temperature"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/label/boilertemp | jq ."
echo ""
echo "Response example:"
cat << 'EOF'
{
  "label": "Room Setpoint",
  "value": 21.5,
  "unit": "°C"
}
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V1}/otgw/label/roomtemp" | python3 -m json.tool 2>/dev/null || curl -s "${API_V1}/otgw/label/roomtemp"
echo ""
echo ""

# ============================================================================
# 10. SEND OTGW COMMAND - Control boiler via OpenTherm commands
# ============================================================================
echo -e "${YELLOW}10. SEND OTGW COMMAND${NC}"
echo "Send OpenTherm commands to control boiler settings"
echo ""
echo "Request:"
echo "  POST /api/v1/otgw/command/{command}"
echo ""
echo "Common commands:"
echo "  CS=1        - Enable Central Heating"
echo "  CS=0        - Disable Central Heating"
echo "  SW=1        - Enable DHW"
echo "  SW=0        - Disable DHW"
echo "  TT=22.0     - Set target room temperature (Temporary override)"
echo "  TR=50.0     - Set DHW setpoint"
echo ""
echo "cURL examples:"
echo "  # Set room temperature to 22°C"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/TT=22.0"
echo ""
echo "  # Enable Central Heating"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/CS=1"
echo ""
echo "  # Disable DHW"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/SW=0"
echo ""
echo -e "${RED}Note: These are control commands. Use with caution!${NC}"
echo ""
echo ""

# ============================================================================
# 11. AUTO CONFIGURE - Trigger MQTT Auto Discovery for Home Assistant
# ============================================================================
echo -e "${YELLOW}11. AUTO CONFIGURE${NC}"
echo "Trigger Home Assistant MQTT Auto Discovery"
echo ""
echo "Request:"
echo "  POST /api/v1/otgw/autoconfigure"
echo ""
echo "cURL command:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/autoconfigure"
echo ""
echo "Response:"
echo "  200 OK"
echo ""
echo "This publishes discovery topics for Home Assistant integration:"
echo "  - Climate entity for boiler/heating"
echo "  - Temperature sensors"
echo "  - Binary sensors for boiler status"
echo "  - Service calls for OTGW commands"
echo ""
echo -e "${YELLOW}Requires: MQTT broker connected${NC}"
echo ""
echo ""

# ============================================================================
# 12. POLLING PATTERN - Monitor device health periodically
# ============================================================================
echo -e "${YELLOW}12. POLLING PATTERN${NC}"
echo "Example: Poll health status every 30 seconds"
echo ""
echo "Bash script:"
cat << 'BASH_EOF'
#!/bin/bash
DEVICE_IP="192.168.1.100"
INTERVAL=30

while true; do
  echo "Checking device health at $(date)"
  response=$(curl -s http://${DEVICE_IP}/api/v1/health)
  
  status=$(echo $response | jq -r '.status')
  otgw=$(echo $response | jq -r '.otgw_connected')
  pic=$(echo $response | jq -r '.picavailable')
  heap=$(echo $response | jq -r '.heap_free')
  
  echo "Status: $status | OTGW: $otgw | PIC: $pic | Heap: $heap bytes"
  
  # Alert if problems detected
  if [ "$status" != "UP" ] || [ "$otgw" != "true" ]; then
    echo "WARNING: Device issues detected!"
  fi
  
  sleep $INTERVAL
done
BASH_EOF
echo ""
echo ""

# ============================================================================
# 13. ERROR HANDLING - Common error responses
# ============================================================================
echo -e "${YELLOW}13. ERROR HANDLING${NC}"
echo "Common error responses and their meanings:"
echo ""
echo "400 Bad Request:"
echo "  curl -s 'http://${DEVICE_IP}/api/v1/otgw/id/invalid'"
echo "  Response: Invalid message ID format"
echo ""
echo "404 Not Found:"
echo "  curl -s 'http://${DEVICE_IP}/api/v1/nonexistent'"
echo "  Response: API endpoint does not exist"
echo ""
echo "405 Method Not Allowed:"
echo "  curl -X DELETE http://${DEVICE_IP}/api/v1/health"
echo "  Response: DELETE method not supported on this endpoint"
echo ""
echo ""

# ============================================================================
# QUICK REFERENCE
# ============================================================================
echo -e "${BLUE}=== QUICK REFERENCE ===${NC}"
echo ""
echo "All v1 Endpoints:"
echo "  GET    /api/v1/health                    - Check device status"
echo "  GET    /api/v1/flashstatus               - Check ESP flash status"
echo "  GET    /api/v1/settings                  - Get device settings"
echo "  POST   /api/v1/settings                  - Update device settings"
echo "  GET    /api/v1/pic/flashstatus           - Check PIC flash status"
echo "  GET    /api/v1/otgw/telegraf             - Export Telegraf format"
echo "  GET    /api/v1/otgw/otmonitor            - Export OTmonitor format"
echo "  GET    /api/v1/otgw/id/{msgid}           - Get OT message by ID"
echo "  GET    /api/v1/otgw/label/{label}        - Get OT message by label"
echo "  POST   /api/v1/otgw/command/{command}    - Send OTGW command"
echo "  POST   /api/v1/otgw/autoconfigure        - Trigger HA auto-discovery"
echo ""
echo "Configuration:"
echo "  DEVICE_IP=${DEVICE_IP}"
echo "  Use: export DEVICE_IP=your.device.ip before running this script"
echo ""
echo -e "${GREEN}Examples completed!${NC}"
