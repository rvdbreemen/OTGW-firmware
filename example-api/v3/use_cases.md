---
# REST API v3 Common Use Cases
Title: REST API v3 Use Cases and Patterns
Version: 1.0.0
Last Updated: 2026-01-31
---

# REST API v3 Common Use Cases

This document shows practical examples for common OTGW scenarios using the REST API v3.

## Table of Contents

1. [Basic Queries](#basic-queries)
2. [Setting Temperatures](#setting-temperatures)
3. [Monitoring](#monitoring)
4. [Configuration](#configuration)
5. [Integration with Home Assistant](#integration-with-home-assistant)
6. [Data Logging and Analytics](#data-logging-and-analytics)
7. [Automation](#automation)
8. [Dashboard Building](#dashboard-building)

## Basic Queries

### Get Current System Status

**Scenario:** Check if the device is working properly

**cURL:**
```bash
curl -X GET http://otgw.local/api/v3/system/health | jq '.'
```

**JavaScript:**
```javascript
const client = new OTGWClient();
const health = await client.getHealthStatus();
console.log(`Status: ${health.status}, Heap: ${health.heap_free} bytes`);
```

**Python:**
```python
client = OTGWClient()
health = client.get_health_status()
print(f"Status: {health['status']}")
print(f"Free Heap: {health['heap_free']} bytes")
```

### Get Current Temperatures

**Scenario:** Display current room and boiler temperatures on a dashboard

**cURL:**
```bash
curl -s http://otgw.local/api/v3/otgw/data | jq '{room_temp, boiler_temp, dhw_temp}'
```

**JavaScript:**
```javascript
const data = await client.getOTGWData();
console.log(`Room: ${data.room_temp}°C, Boiler: ${data.boiler_temp}°C`);
```

**Python:**
```python
data = client.get_otgw_data()
print(f"Room: {data['room_temp']}°C, Boiler: {data['boiler_temp']}°C")
```

### Get Device Information

**Scenario:** Display device info in a settings page

**cURL:**
```bash
curl -s http://otgw.local/api/v3/system/info | jq '.'
```

**JavaScript:**
```javascript
const info = await client.getSystemInfo();
document.getElementById('hostname').textContent = info.hostname;
document.getElementById('version').textContent = info.version;
```

**Python:**
```python
info = client.get_system_info()
print(f"Device: {info['hostname']}")
print(f"Version: {info['version']}")
```

## Setting Temperatures

### Set Room Temperature Override (Temporary)

**Scenario:** User wants to temporarily override the room temperature for comfort

**cURL:**
```bash
# Set to 22°C
curl -X POST http://otgw.local/api/v3/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{"command": "TT=22.0"}'
```

**JavaScript:**
```javascript
// User adjusts slider in web UI
async function updateTemperature(value) {
  const result = await client.setTemperature(parseFloat(value));
  if (result.status === 'queued') {
    console.log('Temperature set to ' + value + '°C');
  }
}
```

**Python:**
```python
def set_comfort_mode(temperature):
    result = client.set_temperature(temperature)
    print(f"Set to {temperature}°C: {result['status']}")

# Usage
set_comfort_mode(22.5)
```

### Set Return Temperature Override

**Scenario:** Technician wants to override the return temperature for system optimization

**cURL:**
```bash
curl -X POST http://otgw.local/api/v3/otgw/command \
  -H 'Content-Type: application/json' \
  -d '{"command": "TR=45.0"}'
```

**JavaScript:**
```javascript
async function setReturnTemp(temp) {
  return client.setReturnTemperature(temp);
}
```

**Python:**
```python
client.set_return_temperature(45.0)
```

## Monitoring

### Real-time Health Monitoring

**Scenario:** Monitor device health and alert on problems

**Python (every 30 seconds):**
```python
import time
from datetime import datetime

def monitor_device_health(interval=30):
    while True:
        try:
            health = client.get_health_status()
            status = health['status']
            heap = health['heap_free']
            
            if status != 'UP':
                print(f"ALERT: Device status is {status}")
            
            if heap < 10000:  # Less than 10KB free
                print(f"WARNING: Low memory: {heap} bytes")
            
            timestamp = datetime.now().strftime('%H:%M:%S')
            print(f"[{timestamp}] Status: {status}, Heap: {heap}")
            
            time.sleep(interval)
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(interval)

# Run
monitor_device_health()
```

**JavaScript (live dashboard update):**
```javascript
// Update every 30 seconds
setInterval(async () => {
  try {
    const health = await client.getHealthStatus();
    document.getElementById('status').textContent = health.status;
    document.getElementById('heap').textContent = health.heap_free;
  } catch (error) {
    console.error('Update failed:', error);
  }
}, 30000);
```

### Monitor Temperature Trends

**Scenario:** Track boiler temperature over time to detect heating issues

**Python:**
```python
import time
from datetime import datetime
import json

def log_temperatures(duration_minutes=60, interval=5):
    """Log temperatures every interval seconds for duration_minutes"""
    log = []
    start = time.time()
    
    while (time.time() - start) < (duration_minutes * 60):
        data = client.get_otgw_data()
        entry = {
            'timestamp': datetime.now().isoformat(),
            'room_temp': float(data['room_temp']),
            'boiler_temp': float(data['boiler_temp']),
            'dhw_temp': float(data['dhw_temp'])
        }
        log.append(entry)
        
        print(f"[{entry['timestamp']}] Room: {entry['room_temp']}°C, "
              f"Boiler: {entry['boiler_temp']}°C")
        
        time.sleep(interval)
    
    # Save to file
    with open('temperature_log.json', 'w') as f:
        json.dump(log, f, indent=2)
    
    return log

# Usage: Log for 60 minutes
temperatures = log_temperatures(duration_minutes=60, interval=30)
```

## Configuration

### Update MQTT Broker

**Scenario:** Change MQTT broker without rebooting device

**cURL:**
```bash
curl -X PATCH http://otgw.local/api/v3/config/mqtt \
  -H 'Content-Type: application/json' \
  -d '{
    "broker": "192.168.1.100",
    "port": 1883,
    "enabled": true,
    "topic_prefix": "otgw/living-room"
  }'
```

**JavaScript:**
```javascript
async function updateMQTTBroker(broker, port) {
  const result = await client.updateMQTTConfig({
    broker: broker,
    port: port,
    enabled: true
  });
  console.log('MQTT updated:', result);
}
```

**Python:**
```python
def update_mqtt_server(broker, port=1883):
    result = client.update_mqtt_config(
        broker=broker,
        port=port,
        enabled=True
    )
    print(f"MQTT configured: {broker}:{port}")

# Usage
update_mqtt_server('192.168.1.50', 1883)
```

### Update Timezone

**Scenario:** Change device timezone for correct time display

**cURL:**
```bash
curl -X PATCH http://otgw.local/api/v3/config/device \
  -H 'Content-Type: application/json' \
  -d '{"ntp_timezone": "America/New_York"}'
```

**Python:**
```python
client.update_timezone('America/New_York')
print("Timezone updated to America/New_York")
```

### Get Current Configuration

**Scenario:** Export device settings for backup or documentation

**cURL:**
```bash
curl -s http://otgw.local/api/v3/export/settings | jq '.' > device_backup.json
```

**Python:**
```python
import json
from datetime import datetime

def backup_settings():
    settings = client.export_settings()
    filename = f"otgw_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    
    with open(filename, 'w') as f:
        json.dump(settings, f, indent=2)
    
    print(f"Settings backed up to {filename}")
    return settings

# Usage
backup_settings()
```

## Integration with Home Assistant

### Get Data via REST

**Scenario:** Home Assistant REST sensor for current temperature

**YAML for Home Assistant:**
```yaml
sensor:
  - platform: rest
    resource: http://otgw.local/api/v3/otgw/data
    name: OTGW Boiler Temperature
    unit_of_measurement: "°C"
    value_template: "{{ value_json.boiler_temp }}"
    scan_interval: 30

  - platform: rest
    resource: http://otgw.local/api/v3/otgw/status
    name: OTGW Heating Status
    value_template: "{{ value_json.flame }}"
    scan_interval: 10
```

### Send Commands from Home Assistant

**Scenario:** Home Assistant automation to set temperature

**YAML for Home Assistant:**
```yaml
automation:
  - alias: "Comfort Mode Evening"
    trigger:
      platform: time
      at: "18:00:00"
    action:
      - service: rest_command.otgw_set_temperature
        data:
          temp: 22

script:
  otgw_set_temperature:
    sequence:
      - service: rest_command.otgw_command
        data_template:
          command: "TT={{ temp }}"

rest_command:
  otgw_command:
    url: "http://otgw.local/api/v3/otgw/command"
    method: POST
    content_type: "application/json"
    payload: '{"command": "{{ command }}"}'
```

## Data Logging and Analytics

### Export to InfluxDB

**Scenario:** Send OTGW data to InfluxDB for analytics and graphing

**Python (using Telegraf format):**
```python
import subprocess
import time

def push_to_influxdb(interval=60, duration=None):
    """Push OTGW data to InfluxDB every interval seconds"""
    start = time.time()
    
    while True:
        try:
            telegraf_data = client.export_telegraf()
            
            # Write to InfluxDB using influx CLI
            result = subprocess.run(
                ['influx', 'write', '--file', '-'],
                input=telegraf_data.encode(),
                capture_output=True
            )
            
            if result.returncode == 0:
                print(f"[{time.strftime('%H:%M:%S')}] Data pushed to InfluxDB")
            else:
                print(f"Error: {result.stderr.decode()}")
            
            time.sleep(interval)
            
            if duration and (time.time() - start) > duration:
                break
        
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(interval)

# Usage: Push every minute for 24 hours
push_to_influxdb(interval=60, duration=86400)
```

### CSV Export for Analysis

**Scenario:** Export temperature data to CSV for spreadsheet analysis

**Python:**
```python
import csv
from datetime import datetime

def export_to_csv(filename='otgw_data.csv', duration_hours=1, interval=5):
    """Export OTGW data to CSV"""
    with open(filename, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['timestamp', 'room_temp', 'boiler_temp', 'dhw_temp', 'flame'])
        writer.writeheader()
        
        start = datetime.now()
        end_time = start.timestamp() + (duration_hours * 3600)
        
        while datetime.now().timestamp() < end_time:
            try:
                data = client.get_otgw_data()
                status = client.get_gateway_status()
                
                writer.writerow({
                    'timestamp': datetime.now().isoformat(),
                    'room_temp': data['room_temp'],
                    'boiler_temp': data['boiler_temp'],
                    'dhw_temp': data['dhw_temp'],
                    'flame': 'ON' if status['flame'] else 'OFF'
                })
                
                time.sleep(interval)
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(interval)
    
    print(f"Data exported to {filename}")

# Usage
export_to_csv('otgw_data.csv', duration_hours=24, interval=60)
```

## Automation

### Auto-Adjust Temperature Based on Time

**Scenario:** Automatically set different temperatures for different times of day

**Python:**
```python
from datetime import datetime
import time

def schedule_temperatures():
    """Set temperature based on time of day"""
    schedule = {
        '06:00': 20.0,   # 6 AM: wake up
        '09:00': 18.0,   # 9 AM: energy saving
        '17:00': 21.0,   # 5 PM: comfort time
        '23:00': 17.0    # 11 PM: sleep mode
    }
    
    while True:
        now = datetime.now().strftime('%H:%M')
        
        for time_str, temp in schedule.items():
            if now == time_str:
                result = client.set_temperature(temp)
                print(f"[{now}] Temperature set to {temp}°C: {result['status']}")
        
        time.sleep(60)  # Check every minute

# Run in background
schedule_temperatures()
```

### Alert on Abnormal Conditions

**Scenario:** Notify user if boiler temperature gets too high

**Python:**
```python
import smtplib
from email.mime.text import MIMEText
import time

def monitor_with_alerts(temp_max=75, check_interval=30):
    """Monitor and send email alert if temperature exceeds limit"""
    
    while True:
        try:
            data = client.get_otgw_data()
            boiler_temp = float(data['boiler_temp'])
            
            if boiler_temp > temp_max:
                send_alert_email(
                    subject="OTGW Alert: High Boiler Temperature",
                    message=f"Boiler temperature is {boiler_temp}°C (limit: {temp_max}°C)"
                )
                print(f"ALERT: Temperature {boiler_temp}°C exceeds limit {temp_max}°C")
            
            time.sleep(check_interval)
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(check_interval)

def send_alert_email(subject, message):
    # Implementation would send actual email
    print(f"Email: {subject}")
    print(f"Message: {message}")
```

## Dashboard Building

### Create a Simple Status Dashboard (HTML)

**Scenario:** Build a simple web page showing current status

**HTML with JavaScript:**
```html
<!DOCTYPE html>
<html>
<head>
    <title>OTGW Dashboard</title>
    <script src="javascript_examples.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .card { border: 1px solid #ccc; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .status { font-size: 24px; font-weight: bold; }
        .temp { font-size: 20px; color: #e74c3c; }
        .ok { color: #27ae60; }
        .alert { color: #e74c3c; }
    </style>
</head>
<body>
    <h1>OTGW Dashboard</h1>
    
    <div class="card">
        <h3>System Status</h3>
        <p>Status: <span id="status" class="status ok">UP</span></p>
        <p>Free Heap: <span id="heap">-</span> bytes</p>
    </div>
    
    <div class="card">
        <h3>Temperatures</h3>
        <p>Room: <span id="room-temp" class="temp">-</span>°C</p>
        <p>Boiler: <span id="boiler-temp" class="temp">-</span>°C</p>
        <p>DHW: <span id="dhw-temp" class="temp">-</span>°C</p>
    </div>
    
    <div class="card">
        <h3>Heating</h3>
        <p>Status: <span id="heating" class="status">-</span></p>
        <p>Flame: <span id="flame" class="status">-</span></p>
    </div>
    
    <script>
        const client = new OTGWClient();
        
        async function updateDashboard() {
            try {
                // Get health
                const health = await client.getHealthStatus();
                document.getElementById('status').textContent = health.status;
                document.getElementById('status').className = 'status ' + 
                    (health.status === 'UP' ? 'ok' : 'alert');
                document.getElementById('heap').textContent = health.heap_free;
                
                // Get temperatures
                const data = await client.getOTGWData();
                document.getElementById('room-temp').textContent = data.room_temp;
                document.getElementById('boiler-temp').textContent = data.boiler_temp;
                document.getElementById('dhw-temp').textContent = data.dhw_temp;
                
                // Get status
                const status = await client.getGatewayStatus();
                document.getElementById('heating').textContent = 
                    status.ch_enabled ? 'ON' : 'OFF';
                document.getElementById('flame').textContent = 
                    status.flame ? 'BURNING' : 'OFF';
                document.getElementById('flame').className = 'status ' + 
                    (status.flame ? 'alert' : 'ok');
            } catch (error) {
                console.error('Dashboard update failed:', error);
            }
        }
        
        // Update immediately and then every 30 seconds
        updateDashboard();
        setInterval(updateDashboard, 30000);
    </script>
</body>
</html>
```

---

**Documentation Version:** 1.0.0  
**Last Updated:** 2026-01-31
