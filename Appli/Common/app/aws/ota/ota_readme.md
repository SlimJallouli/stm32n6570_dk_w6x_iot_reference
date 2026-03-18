markdown
# 📦 Enabling MQTT-Based Firmware Updates in Home Assistant

This guide walks through how to expose a firmware update entity in Home Assistant using the `update.mqtt` integration. When configured correctly, users can trigger OTA updates with a single click via Home Assistant’s UI.

## 🔧 Overview

- Exposes an Update entity (e.g., `update.device_fw`)
- Publishes firmware version data over MQTT
- Triggers updates using a dedicated MQTT `command_topic`

---

## 🧠 Key Components

| Property              | Role                                                                 |
|----------------------|----------------------------------------------------------------------|
| `state_topic`         | Contains the device’s installed firmware version                    |
| `latest_version_topic`| Indicates the newest available firmware version                     |
| `command_topic`       | Receives the install trigger (from HA)                              |
| `payload_install`     | Value HA sends when user clicks "Install"                           |
| `value_template`      | Extracts data from the state topic JSON                             |

---

## 🔀 Home Assistant Discovery Payload

Publish a retained MQTT message to this topic:

homeassistant/update/<device_id>_fw/config


With the following JSON:

```json
{
  "name": "Firmware Update",
  "unique_id": "mydevice_fw_update",
  "state_topic": "mydevice/fw/state",
  "latest_version_topic": "mydevice/fw/state",
  "value_template": "{{ value_json.installed_version }}",
  "latest_version_template": "{{ value_json.latest_version }}",
  "command_topic": "mydevice/fw/update",
  "payload_install": "start_update",
  "retain": true,
  "device_class": "firmware",
  "device": {
    "identifiers": ["mydevice"],
    "manufacturer": "STMicroelectronics",
    "model": "STM32H573",
    "name": "MyDevice"
  }
}
✅ The discovery message must be published with the retain flag set to true so HA loads it on reboot.

📡 MQTT Message Flow
1. Device Publishes State
Topic: mydevice/fw/state

json
{
  "installed_version": "1.0.0",
  "latest_version": "1.1.0"
}
If versions mismatch, HA shows "Update available"

If they match, HA shows "Up to date"

2. User Clicks "Install"
Topic: mydevice/fw/update

Payload:

plaintext
start_update
Your device should be subscribed to mydevice/fw/update and begin the OTA flow when this message arrives.

3. (Optional) Progress Reporting
Publish intermediate states to state_topic:

json
{
  "installed_version": "1.0.0",
  "latest_version": "1.1.0",
  "update_percentage": 42,
  "in_progress": true
}
4. Update Complete
Final state update:

json
{
  "installed_version": "1.1.0",
  "latest_version": "1.1.0"
}
Home Assistant will now show the entity as fully updated.