#!/bin/bash
#
# OTGW REST API v2 - cURL Examples
# Version: 1.0.0
#
# This file contains cURL examples for the OTGW REST API v2.
# 
# API v2 Features:
#   - Based on v1 with improved endpoints
#   - OTmonitor endpoint returns enhanced data
#   - Better response formatting
#
# Usage: 
#   bash curl_examples.sh
#   Or source individual examples
#
# Note: Replace DEVICE_IP with your OTGW device IP address (default: 192.168.1.100)
#

DEVICE_IP="${DEVICE_IP:-192.168.1.100}"
DEVICE_URL="http://${DEVICE_IP}"
API_V2="${DEVICE_URL}/api/v2"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== OTGW REST API v2 Examples ===${NC}\n"
echo -e "${YELLOW}Note: v2 is a minimal upgrade from v1 with enhanced OTmonitor endpoint${NC}\n"

# ============================================================================
# 1. ENHANCED OTMONITOR ENDPOINT
# ============================================================================
echo -e "${YELLOW}1. ENHANCED OTMONITOR ENDPOINT (v2 Feature)${NC}"
echo "Get OpenTherm data in enhanced OTmonitor format with better structure"
echo ""
echo "Request:"
echo "  GET /api/v2/otgw/otmonitor"
echo ""
echo "cURL command:"
echo "  curl -s http://${DEVICE_IP}/api/v2/otgw/otmonitor | head -30"
echo ""
echo "Response format:"
cat << 'EOF'
# Enhanced OTmonitor format with structured output
# Better parsing support compared to v1
Status:                           84 (CH enabled, DHW enabled, Flame on)
Control Setpoint Temperature:     21.5
Boiler Setpoint Temperature:      60.0
Boiler Actual Temperature:        54.3
DHW Setpoint Temperature:         50.0
DHW Actual Temperature:           45.2
Modulation Level:                 80.5
Pump Modulation:                  30.0
Room Setpoint Temperature:        22.0
Room Temperature:                 21.5
...
EOF
echo ""
echo -e "${GREEN}Execute:${NC}"
curl -s "${API_V2}/otgw/otmonitor" 2>/dev/null | head -15 || echo "Endpoint not available"
echo ""
echo ""

# ============================================================================
# 2. BACKWARD COMPATIBILITY - v1 Endpoints Still Work in v2
# ============================================================================
echo -e "${YELLOW}2. BACKWARD COMPATIBILITY${NC}"
echo "All v1 endpoints continue to work in v2 for seamless upgrades"
echo ""
echo "V1 Endpoints available in V2:"
echo "  GET  /api/v1/health"
echo "  GET  /api/v1/settings"
echo "  POST /api/v1/settings"
echo "  GET  /api/v1/otgw/telegraf"
echo "  GET  /api/v1/otgw/otmonitor  (original format)"
echo "  GET  /api/v1/otgw/id/{msgid}"
echo "  GET  /api/v1/otgw/label/{label}"
echo "  POST /api/v1/otgw/command/{command}"
echo ""
echo "Your app can use v1 endpoints with v2 devices without changes."
echo ""
echo -e "${GREEN}Test v1 health endpoint (still works in v2):${NC}"
curl -s "${DEVICE_URL}/api/v1/health" | python3 -m json.tool 2>/dev/null || curl -s "${DEVICE_URL}/api/v1/health"
echo ""
echo ""

# ============================================================================
# 3. MIGRATION FROM V1 TO V2
# ============================================================================
echo -e "${YELLOW}3. UPGRADING FROM V1 TO V2${NC}"
echo "Migration is simple - most apps need no changes!"
echo ""
echo "Step 1: Update endpoint path (optional)"
echo "  Before: /api/v1/otgw/otmonitor"
echo "  After:  /api/v2/otgw/otmonitor"
echo ""
echo "Step 2: Handle improved response format"
echo "  v2 returns better structured OTmonitor data"
echo "  Parsing is more reliable"
echo ""
echo "Step 3: Verify your parser handles both formats"
echo "  Or keep using /api/v1/ for compatibility"
echo ""
echo "Recommended: Use /api/v2/ for new projects"
echo "             Keep /api/v1/ for existing apps"
echo ""
echo ""

# ============================================================================
# 4. COMMON WORKFLOWS
# ============================================================================
echo -e "${YELLOW}4. COMMON WORKFLOWS (Same for v1/v2)${NC}"
echo ""

# Check device health
echo "A) Check Device Health"
echo "  curl -s http://${DEVICE_IP}/api/v1/health | jq '.status'"
echo ""

# Get current temperature
echo "B) Get Current Room Temperature"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/id/16 | jq '.value'"
echo ""

# Set room temperature
echo "C) Set Room Temperature to 22°C"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/TT=22.0"
echo ""

# Export data
echo "D) Export OTmonitor Data"
echo "  curl -s http://${DEVICE_IP}/api/v2/otgw/otmonitor > otmonitor.txt"
echo ""

# Monitor in loop
echo "E) Monitor Device Every 30 Seconds"
cat << 'BASH_EOF'
while true; do
  curl -s http://DEVICE_IP/api/v1/health | jq \
    '{status: .status, otgw: .otgw_connected, heap: .heap_free}'
  sleep 30
done
BASH_EOF
echo ""
echo ""

# ============================================================================
# 5. COMMAND EXAMPLES
# ============================================================================
echo -e "${YELLOW}5. COMMON OTGW COMMANDS${NC}"
echo ""

echo "Enable Central Heating:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/CS=1"
echo ""

echo "Disable Central Heating:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/CS=0"
echo ""

echo "Set Room Temperature (Temporary):"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/TT=22.5"
echo ""

echo "Set DHW Setpoint:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/TR=48.0"
echo ""

echo "Enable DHW:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/SW=1"
echo ""

echo "Disable DHW:"
echo "  curl -X POST http://${DEVICE_IP}/api/v1/otgw/command/SW=0"
echo ""
echo ""

# ============================================================================
# 6. RESPONSE EXAMPLES
# ============================================================================
echo -e "${YELLOW}6. RESPONSE EXAMPLES${NC}"
echo ""

echo "Health Response:"
cat << 'EOF'
{
  "status": "UP",
  "uptime": 86400,
  "otgw_connected": true,
  "picavailable": true,
  "mqtt_connected": true,
  "wifi_rssi": -45,
  "heap_free": 25600,
  "heap_max": 40960
}
EOF
echo ""

echo "OT Value Response:"
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

echo "Flash Status Response:"
cat << 'EOF'
{
  "esp_flash_ok": true,
  "pic_flash_in_progress": false,
  "pic_available": true
}
EOF
echo ""
echo ""

# ============================================================================
# 7. ERROR HANDLING
# ============================================================================
echo -e "${YELLOW}7. ERROR HANDLING${NC}"
echo ""

echo "Check HTTP Status Code:"
echo "  curl -i http://${DEVICE_IP}/api/v1/health"
echo "  Look for '200 OK' or appropriate status code"
echo ""

echo "Handle Errors:"
cat << 'BASH_EOF'
response=$(curl -s -w "\n%{http_code}" http://DEVICE_IP/api/v1/health)
body=$(echo "$response" | head -n -1)
code=$(echo "$response" | tail -n 1)

if [ "$code" = "200" ]; then
  echo "Success: $body"
else
  echo "Error (HTTP $code): $body"
fi
BASH_EOF
echo ""
echo ""

# ============================================================================
# 8. PERFORMANCE TIPS
# ============================================================================
echo -e "${YELLOW}8. PERFORMANCE TIPS${NC}"
echo ""

echo "Reduce Payload - Use Specific Endpoints:"
echo "  ✗ Slow:  /api/v1/health (full health object)"
echo "  ✓ Fast:  /api/v1/otgw/id/16 (single value)"
echo ""

echo "Batch Operations:"
cat << 'BASH_EOF'
# Get multiple values with parallel requests
curl -s http://DEVICE_IP/api/v1/otgw/id/16 & # Room temp
curl -s http://DEVICE_IP/api/v1/otgw/id/17 & # Boiler temp
curl -s http://DEVICE_IP/api/v1/otgw/id/24 & # Modulation
wait
BASH_EOF
echo ""

echo "Caching:"
echo "  Poll /health every 30-60 seconds"
echo "  Cache results locally for monitoring UI"
echo "  Only poll /otgw endpoints when needed"
echo ""

echo "Timeout Handling:"
echo "  Use --max-time option:"
echo "  curl --max-time 5 http://DEVICE_IP/api/v1/health"
echo ""
echo ""

# ============================================================================
# 9. INTEGRATION EXAMPLES
# ============================================================================
echo -e "${YELLOW}9. INTEGRATION EXAMPLES${NC}"
echo ""

echo "Home Assistant (REST Sensor):"
cat << 'YAML_EOF'
sensor:
  - platform: rest
    resource: http://192.168.1.100/api/v1/otgw/id/16
    name: Room Temperature
    unit_of_measurement: °C
    value_template: '{{ value_json.value }}'
    scan_interval: 60
YAML_EOF
echo ""

echo "InfluxDB Export:"
echo "  curl -s http://${DEVICE_IP}/api/v1/otgw/telegraf | \\"
echo "    curl -X POST http://influxdb:8086/write?db=otgw --data-binary @-"
echo ""

echo "Daily Data Export:"
cat << 'BASH_EOF'
curl -s http://DEVICE_IP/api/v1/otgw/otmonitor > \
  otmonitor_$(date +%Y%m%d_%H%M%S).txt
BASH_EOF
echo ""
echo ""

# ============================================================================
# QUICK REFERENCE
# ============================================================================
echo -e "${BLUE}=== QUICK REFERENCE ===${NC}"
echo ""

echo "V2 Features vs V1:"
echo "  ✓ Enhanced OTmonitor endpoint"
echo "  ✓ Better response formatting"
echo "  ✓ Full backward compatibility"
echo "  ✓ Same performance"
echo ""

echo "Recommended Usage:"
echo "  New Projects:      Use /api/v2/"
echo "  Existing Apps:     Keep using /api/v1/"
echo "  Migration:         No breaking changes required"
echo ""

echo "Configuration:"
echo "  DEVICE_IP=${DEVICE_IP}"
echo "  Use: export DEVICE_IP=your.device.ip"
echo ""

echo -e "${GREEN}V2 Examples completed!${NC}"
