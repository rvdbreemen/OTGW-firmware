#!/bin/bash

# OpenTherm Gateway REST API v3 - cURL Examples
# 
# This script demonstrates common API operations using cURL.
# Assumes the device is available at http://otgw.local
# 
# Usage: ./curl_examples.sh [hostname] [port]
# Example: ./curl_examples.sh otgw.local 80

OTGW_HOST="${1:-otgw.local}"
OTGW_PORT="${2:-80}"
OTGW_URL="http://$OTGW_HOST:$OTGW_PORT/api/v3"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper function to print section headers
print_header() {
  echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

# Helper function to print example with description
print_example() {
  echo -e "${GREEN}$1${NC}"
  echo "$2"
  echo
}

# 1. API DISCOVERY EXAMPLES
print_header "API Discovery"

print_example "Get API Root" \
  "curl -X GET $OTGW_URL/"

print_example "Get System Resources Index" \
  "curl -X GET $OTGW_URL/system"

print_example "Get Configuration Resources Index" \
  "curl -X GET $OTGW_URL/config"

print_example "Get OpenTherm Resources Index" \
  "curl -X GET $OTGW_URL/otgw"

# 2. SYSTEM INFORMATION EXAMPLES
print_header "System Information"

print_example "Get Device Information" \
  "curl -X GET $OTGW_URL/system/info"

print_example "Get System Health Status" \
  "curl -X GET $OTGW_URL/system/health"

print_example "Get System Time and Timezone" \
  "curl -X GET $OTGW_URL/system/time"

print_example "Get Network Configuration" \
  "curl -X GET $OTGW_URL/system/network"

print_example "Get System Statistics" \
  "curl -X GET $OTGW_URL/system/stats"

# 3. CACHING WITH ETAGS
print_header "Caching with ETags"

print_example "First Request (includes ETag)" \
  "curl -i -X GET $OTGW_URL/system/info"

print_example "Conditional Request (304 if unchanged)" \
  "curl -X GET $OTGW_URL/system/info -H 'If-None-Match: \"a1b2c3d4-5678901\"'"

print_example "Save response headers to inspect ETag" \
  "curl -D headers.txt $OTGW_URL/system/info > response.json"

# 4. CONFIGURATION EXAMPLES
print_header "Configuration Management"

print_example "Get Device Settings" \
  "curl -X GET $OTGW_URL/config/device"

print_example "Get Network Settings" \
  "curl -X GET $OTGW_URL/config/network"

print_example "Get MQTT Configuration" \
  "curl -X GET $OTGW_URL/config/mqtt"

print_example "Update Device Hostname" \
  "curl -X PATCH $OTGW_URL/config/device \
  -H 'Content-Type: application/json' \
  -d '{\"hostname\": \"my-otgw\"}'"

print_example "Update MQTT Configuration" \
  "curl -X PATCH $OTGW_URL/config/mqtt \
  -H 'Content-Type: application/json' \
  -d '{
    \"broker\": \"192.168.1.100\",
    \"port\": 1883,
    \"enabled\": true
  }'"

print_example "Get Available Features" \
  "curl -X GET $OTGW_URL/config/features"

# 5. OPENTHERM DATA EXAMPLES
print_header "OpenTherm Data and Control"

print_example "Get Gateway Status" \
  "curl -X GET $OTGW_URL/otgw/status"

print_example "Get Current Data (temperatures, pressures)" \
  "curl -X GET $OTGW_URL/otgw/data"

print_example "Get All OpenTherm Messages (page 1)" \
  "curl -X GET '$OTGW_URL/otgw/messages?page=0&per_page=20'"

print_example "Get Specific Message by ID" \
  "curl -X GET '$OTGW_URL/otgw/messages?id=25'"

print_example "Get Temperature Messages Only" \
  "curl -X GET '$OTGW_URL/otgw/messages?category=temperature'"

print_example "Send Command to Gateway (Temporary Setpoint)" \
  "curl -X POST $OTGW_URL/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{\"command\": \"TT=21.5\"}'"

print_example "Send Command (Override Return Temperature)" \
  "curl -X POST $OTGW_URL/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{\"command\": \"TR=45.0\"}'"

print_example "Get OTmonitor Format Data" \
  "curl -X GET $OTGW_URL/otgw/monitor"

# 6. PIC FIRMWARE EXAMPLES
print_header "PIC Firmware Information"

print_example "Get PIC Information" \
  "curl -X GET $OTGW_URL/pic/info"

print_example "Get PIC Flash Status" \
  "curl -X GET $OTGW_URL/pic/flash"

print_example "Get PIC Diagnostics" \
  "curl -X GET $OTGW_URL/pic/diag"

# 7. SENSOR EXAMPLES
print_header "External Sensors"

print_example "Get Dallas Temperature Sensors" \
  "curl -X GET $OTGW_URL/sensors/dallas"

print_example "Get S0 Pulse Counter" \
  "curl -X GET $OTGW_URL/sensors/s0"

print_example "Reset S0 Counter to Zero" \
  "curl -X PUT $OTGW_URL/sensors/s0"

# 8. DATA EXPORT EXAMPLES
print_header "Data Export"

print_example "Get Export Formats Index" \
  "curl -X GET $OTGW_URL/export"

print_example "Export as Telegraf Metrics" \
  "curl -X GET $OTGW_URL/export/telegraf"

print_example "Export Settings as JSON" \
  "curl -X GET $OTGW_URL/export/settings"

print_example "Export OTmonitor Format" \
  "curl -X GET $OTGW_URL/export/otmonitor"

print_example "Get Recent Debug Logs" \
  "curl -X GET $OTGW_URL/export/logs"

# 9. CORS PREFLIGHT EXAMPLES
print_header "CORS Support"

print_example "Browser Preflight Check (OPTIONS)" \
  "curl -X OPTIONS $OTGW_URL/otgw/status \
  -H 'Access-Control-Request-Method: PATCH' \
  -H 'Access-Control-Request-Headers: Content-Type' \
  -v"

# 10. ERROR HANDLING EXAMPLES
print_header "Error Handling"

print_example "Send Invalid Command Format" \
  "curl -X POST $OTGW_URL/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{\"command\": \"INVALID\"}'"

print_example "Attempt to Access Non-existent Resource" \
  "curl -X GET $OTGW_URL/resource/notfound"

print_example "Send Malformed JSON" \
  "curl -X PATCH $OTGW_URL/config/device \
  -H 'Content-Type: application/json' \
  -d '{invalid json}'"

# 11. ADVANCED PAGINATION EXAMPLES
print_header "Advanced Pagination"

print_example "Get Last Page of Messages" \
  "curl -X GET '$OTGW_URL/otgw/messages?page=6&per_page=20'"

print_example "Get 50 Items per Page" \
  "curl -X GET '$OTGW_URL/otgw/messages?page=0&per_page=50'"

print_example "Parse Links from Response" \
  "curl -X GET '$OTGW_URL/otgw/messages?page=0&per_page=20' | grep '\"next\"'"

# 12. BATCH OPERATIONS EXAMPLES
print_header "Batch Operations"

print_example "Read and Store Settings in File" \
  "curl -X GET $OTGW_URL/export/settings > settings_backup.json"

print_example "View Stored Settings" \
  "cat settings_backup.json"

print_example "Monitor Device in Real-time (repeat every 5 seconds)" \
  "while true; do curl -s $OTGW_URL/system/health | jq '.status'; sleep 5; done"

# 13. PRETTY-PRINTING JSON
print_header "Pretty-Printing JSON Output"

print_example "Using jq to Pretty Print and Query" \
  "curl -s $OTGW_URL/system/info | jq '.'"

print_example "Extract Specific Fields with jq" \
  "curl -s $OTGW_URL/system/info | jq '{hostname, version, uptime}'"

print_example "Pretty Print with Python" \
  "curl -s $OTGW_URL/system/info | python3 -m json.tool"

# 14. PERFORMANCE TESTING
print_header "Performance Testing"

print_example "Time a Simple Request" \
  "time curl -X GET $OTGW_URL/system/health"

print_example "Sequential Requests (10 times)" \
  "for i in {1..10}; do curl -s $OTGW_URL/system/health > /dev/null; done"

# 15. DEBUG MODE
print_header "Debug Mode"

print_example "Show Request and Response Headers" \
  "curl -v -X GET $OTGW_URL/system/info"

print_example "Show Full Request Details" \
  "curl -v -X POST $OTGW_URL/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{\"command\": \"TT=21.5\"}'"

echo -e "\n${BLUE}=== Summary ===${NC}\n"
echo "For more examples and complete documentation, see:"
echo "  - docs/api/v3/API_REFERENCE.md"
echo "  - example-api/v3/javascript_examples.js"
echo "  - example-api/v3/python_examples.py"
echo
