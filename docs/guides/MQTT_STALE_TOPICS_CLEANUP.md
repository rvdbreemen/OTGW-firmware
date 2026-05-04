# Cleaning Up Stale MQTT Retained Topics After Firmware Upgrade

## Why does this happen?

When the OTGW firmware publishes sensor and entity information to Home Assistant, it uses a
protocol called **MQTT discovery**: the firmware sends a configuration message to a specific
topic on your MQTT broker. Home Assistant reads that message and automatically creates the
entity (sensor, switch, etc.).

These discovery messages are **retained** on the broker — the broker remembers them and
replays them to any new subscriber. This means HA re-learns all entities automatically after
a restart.

**The problem:** when you upgrade the firmware, the new version may use slightly different
topic paths or entity names. The old retained discovery messages stay on the broker. Home
Assistant now sees both the old and the new, and creates duplicate entities — sometimes
appending `_2` to distinguish them.

**Common symptoms:**
- Two identical sensors (e.g. two `OTGW_Room_Temperature` at the same value)
- An entity showing a wrong/stale value or `43 °C` instead of the actual boiler setpoint
- Entities showing `unavailable` or `unknown` that should have a value

The fix is to remove the old retained discovery messages from the broker, then let the
firmware publish fresh ones.

---

## Step 1 — Find your device's Unique ID

Every OTGW device has a unique identifier (based on the ESP chip ID). You need this to
locate your specific discovery topics on the broker.

1. Open the OTGW web interface: go to `http://otgw.local` or `http://<your device IP>` in a browser.
2. Click **Settings** in the top menu.
3. Find the **MQTT** section.
4. Look for the field labeled **Unique ID** — it will look something like `otgw-a1b2c3`.

Write this value down. You will use it in all the steps below.

> If you changed the **HA prefix** setting from the default `homeassistant`, note that value
> too. The steps below assume the default.

---

## Step 2 — Remove the stale retained topics

Choose the method that works best for you. **Method A (MQTT Explorer)** is the easiest for
most users.

---

### Method A: MQTT Explorer (recommended)

[MQTT Explorer](https://mqtt-explorer.com) is a free graphical app that makes it easy to
browse and delete retained messages. It runs on Windows, macOS, and Linux.

**Install and connect:**
1. Download MQTT Explorer from **https://mqtt-explorer.com** and install it.
2. Open MQTT Explorer and click **+** to add a new connection.
3. Enter the same **host**, **port** (usually 1883), **username**, and **password** that
   Home Assistant uses for its MQTT integration.
4. Click **Connect**.

**Delete the stale topics:**
1. In the topic tree on the left, expand **`homeassistant`**.
2. Expand **`sensor`**, then look for a folder that matches your Unique ID
   (e.g. `otgw-a1b2c3`).
3. Right-click that folder and choose **Delete retained messages** (the exact wording
   may differ slightly by version — it is the option that sends an empty retained payload
   to all sub-topics).
4. Repeat this for each of these four folders (if they exist):
   - `homeassistant` → `binary_sensor` → `<your Unique ID>`
   - `homeassistant` → `climate` → `<your Unique ID>`
   - `homeassistant` → `number` → `<your Unique ID>`
   - `homeassistant` → `sensor` → `<your Unique ID>`

After deleting, the topics disappear from the tree — that is correct.

---

### Method B: Home Assistant Developer Tools (no extra software)

If you prefer not to install anything, you can use HA's built-in MQTT tools.

This method requires two steps per topic: first **listen** to discover which topics exist,
then **publish** an empty retained message to each one to delete it.

**Find the topics:**
1. In Home Assistant, go to **Settings → Developer Tools → MQTT**.
2. Under **Listen to a topic**, type:
   ```
   homeassistant/+/<your Unique ID>/+/config
   ```
   Replace `<your Unique ID>` with the value from Step 1, e.g.:
   ```
   homeassistant/+/otgw-a1b2c3/+/config
   ```
3. Click **Start listening**.
4. Wait about 30 seconds. Any retained discovery topics will appear in the log below.
5. Copy all the topic paths that appear (everything before the first space on each line).

**Delete each topic:**
1. Still in Developer Tools → MQTT, scroll down to **Publish a packet**.
2. For each topic path you collected:
   - **Topic**: paste the full topic path
   - **Payload**: leave completely empty
   - **Retain**: ✅ check this box
   - Click **Publish**

   Publishing an empty retained message removes the retained flag, which effectively
   deletes the discovery config from the broker's perspective.

3. Repeat for all topic paths you collected.

---

### Method C: mosquitto command line (advanced users)

If you have `mosquitto_sub` and `mosquitto_pub` installed on your system:

```bash
# Replace these with your actual values:
BROKER="192.168.1.100"
USER="mqttuser"
PASS="mqttpassword"
NODE_ID="otgw-a1b2c3"

# Step 1: collect all retained discovery topics for this device
mosquitto_sub -h "$BROKER" -u "$USER" -P "$PASS" \
  -t "homeassistant/+/$NODE_ID/+/config" \
  --retained-only -v -W 5 2>/dev/null \
  | awk '{print $1}' > stale_topics.txt

echo "Found $(wc -l < stale_topics.txt) retained topics"

# Step 2: delete each one by publishing an empty retained message
while IFS= read -r topic; do
  mosquitto_pub -h "$BROKER" -u "$USER" -P "$PASS" -t "$topic" -r -n
  echo "Deleted: $topic"
done < stale_topics.txt
```

---

## Step 3 — Let the firmware re-publish fresh discovery configs

After you have removed the stale topics, the firmware will automatically publish fresh
discovery configs the next time it connects to MQTT — usually within 1–2 minutes of
the broker being available.

To trigger it immediately without waiting:
- Open the OTGW web interface → **MQTT** tab → click **Re-publish discovery**.
- Or use the REST API: `curl -X POST http://otgw.local/api/v2/discovery/republish`

---

## Step 4 — Restart Home Assistant (if duplicates persist)

Home Assistant caches known entities in its internal registry. If duplicate entities are
still visible after the broker is clean and the firmware has re-published:

1. **Restart Home Assistant**: Settings → System → Restart.
2. After restart, the old stale entities should be gone.
3. If a duplicate entity still appears with `_2` in its name, delete it manually:
   Settings → Devices & Services → MQTT → find your OTGW device → click the entity →
   click the gear icon → Delete.

---

## When to do this cleanup

You only need to do this cleanup after a **firmware upgrade that changes entity names or
topic paths**. This typically happens on major version upgrades (e.g. 1.3.x → 1.4.x or
1.4.x → 1.5.x). Patch releases within the same minor version generally do not require
cleanup.

The release notes for each version will call out when a cleanup is recommended.

---

## Troubleshooting

| Symptom | Likely cause | What to do |
|---------|-------------|------------|
| Entities still duplicated after cleanup | HA entity registry not updated | Restart Home Assistant |
| No topics found when listening | Wrong Unique ID or HA prefix | Check OTGW Settings → MQTT for the exact values |
| Entities disappeared and did not come back | Firmware not yet re-published | Wait 2 min or use Re-publish discovery button |
| DHW setpoint shows 43 °C | Stale discovery config with old `initial` value | Complete this cleanup, then re-publish |
| `_2` entity still present after restart | HA entity registry has a ghost entry | Delete manually via Settings → Devices & Services |
