# STM32U585 Home Assistant MQTT Discovery (AWS IoT Core + Mosquitto Bridge)

This guide explains how to enable **Home Assistant MQTT Auto-Discovery** for the **STM32U585 / B-U585I-IOT02A** IoT reference project, including MQTT topic structure and AWS IoT Core bridge setup.

It covers:

- MQTT discovery topic format
- Device identity format used by this firmware
- Home Assistant + Mosquitto bridge configuration for AWS IoT Core
- Discovery/state topics used by firmware features (OTA, LED, button, sensors, cover, availability)

For Home Assistant MQTT integration details, see:
- https://www.home-assistant.io/integrations/mqtt

## Table of Contents

- [1. Home Assistant MQTT Discovery Topic Format](#1-home-assistant-mqtt-discovery-topic-format)
- [2. Device Identity Format](#2-device-identity-format)
- [3. Home Assistant Bridge to AWS IoT Core](#3-home-assistant-bridge-to-aws-iot-core)
- [4. Verify Device Discovery in Home Assistant](#4-verify-device-discovery-in-home-assistant)
- [5. MQTT Topic Reference](#5-mqtt-topic-reference)

## 1. Home Assistant MQTT Discovery Topic Format

Home Assistant uses MQTT discovery to register entities automatically. Each discovery message is published to a discovery topic:

```
homeassistant/<component>/<device_id>_<entity>/config
```

## 2. Device Identity Format

The firmware uses a device identity format similar to:

- `stm32u585-<serial_number>`

Examples:

- Without STSAFE: `stm32u585-002C005B3332511738363236`
- With STSAFE-A110: `eval3-0209203A825AD52AC20139`
- With STSAFE-A120: `eval5-0209203A825AD52AC20139`

## 3. Home Assistant Bridge to AWS IoT Core

This section shows how to configure Home Assistant to receive discovery and state messages through a Mosquitto bridge connected to AWS IoT Core.

### 3.1 Prerequisites

- Home Assistant with File Editor and Mosquitto Broker add-ons
- AWS IoT Core Thing and certificates for the Home Assistant bridge client
- Devices publishing retained discovery messages

### 3.2 Step 1: Create a device in AWS IoT Core
![AWS_HomeAssistant_Thing.png](../../../../assets/AWS_HomeAssistant_Thing.png)

- Download the Thing certificate, private key, and [AmazonRootCA1.pem](https://www.amazontrust.com/repository/AmazonRootCA1.pem)
- Rename the Thing certificate to `certificate.pem.crt`
- Rename the private key to `private.pem.key`

![AWS_HomeAssistant_Certs.png](../../../../assets/AWS_HomeAssistant_Certs.png)

### 3.3 Step 2: Install File Editor Add-on

1. Go to **Settings > Apps > Add-on Store**
2. Search for **File Editor** and install it
3. Under the **Configuration** tab:
   - Disable `enforce_basepath` to allow editing any file
4. Under the **Info** tab:
   - Enable `Start on boot`
   - Enable `Show in sidebar`
   - Click **Start**

### 3.4 Step 3: Add Certificates to the SSL Folder
1. Open **File Editor**
2. Navigate to `/ssl`
3. Upload the certificates

![HomeAssistant_Upload_Certs.png](../../../../assets/HomeAssistant_Upload_Certs.png)

### 3.5 Step 4: Create Mosquitto Bridge Config
1. Open **File Editor**
2. Navigate to `/share`
3. Create a folder named `mosquitto`
4. Inside `mosquitto`, create a file named `aws_bridge.conf`
5. Paste the following configuration:
6. Update AWS `endpoint` and `clientid`. The `clientid` should match the Home Assistant bridge Thing name created in AWS IoT Core.

```ini
connection aws_bridge
address <your-aws-endpoint>:8883
clientid <your-clientid>

bridge_protocol_version mqttv311

bridge_cafile /ssl/AmazonRootCA1.pem
bridge_certfile /ssl/certificate.pem.crt
bridge_keyfile /ssl/private.pem.key

# Allow bidirectional traffic for all device topics
topic +/# both 0

# Allow bidirectional traffic for Home Assistant discovery
topic homeassistant/# both 0

start_type automatic
try_private false
notifications false

```

![HomeAssistant_aws_bridge.png](../../../../assets/HomeAssistant_aws_bridge.png)

### 3.6 Step 5: Install Mosquitto Broker Add-on

1. Go to **Settings > Apps > Add-on Store**
2. Search for **Mosquitto Broker** and install it
3. Under the **Configuration** tab:
   - Enable `Customize configuration`
   - This allows Mosquitto to load configs from `/share/mosquitto`
4. Under the **Info** tab:
   - Enable `Start on boot`
   - Click **Start**

### 3.7 Step 6: Restart Mosquitto Broker
- Go to **Settings > Add-ons > Mosquitto Broker**

1. Click Restart
2. Under the Log tab, confirm `aws_bridge.conf` was loaded successfully

### 3.8 Step 7: Enable MQTT Integration in Home Assistant
1. Go to **Settings > Devices & Services**
2. Locate the MQTT integration (or add it if not present)
3. Click Configure

* Ensure the following settings:

1. Enable Discovery
2. Discovery Prefix: `homeassistant`
3. Click Submit

### 3.9 Step 8: Validate Discovery
Go to **Developer Tools > MQTT**:

1. Subscribe to `homeassistant/#`
2. Confirm that retained config messages are received
3. Confirm that state messages are published to the correct topics

Entities should appear automatically under **Settings > Devices & Services > MQTT**.

## 4. Verify Device Discovery in Home Assistant

Reset your STM32 board(s). Home Assistant should discover devices/entities and display them in the dashboard.

![HomeAssistant_OverView.png](../../../../assets/HomeAssistant_OverView.png)

## 5. MQTT Topic Reference

This is a description of the MQTT messages exchanged between STM32 and Home Assistant through AWS IoT Core.

This section is a reference for topic/payload behavior. No additional setup is required.

For more details, see https://www.home-assistant.io/integrations/mqtt

### 5.1. Firmware state and revision

Source file: *ha_entities_ota.c*

#### 5.1.1. Home Assistant discovery
- **Topic**: `homeassistant/update/< device_id >_fw/config`
- **Example**: `homeassistant/update/stm32u585-003000523636500A20333342_fw/config`
- **Category**: Diagnostic

#### 5.1.2. Config Payload Example:
```json
{
  "name": "Firmware",
  "unique_id": "stm32u585-003000523636500A20333342_fw_update",
  "state_topic": "stm32u585-003000523636500A20333342/fw/state",
  "value_template": "{{ value_json.installed_version }}",
  "latest_version_topic": "stm32u585-003000523636500A20333342/fw/state",
  "latest_version_template": "{{ value_json.latest_version }}",
  "command_topic": "stm32u585-003000523636500A20333342/fw/update",
  "payload_install": "start_update",
  "availability_topic": "stm32u585-003000523636500A20333342/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device_class": "firmware",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": [
      "stm32u585-003000523636500A20333342"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "stm32u585-003000523636500A20333342",
    "sw_version": "1.25.0"
  }
}
```

#### 5.1.3. Device Message
- **Topic**: `< device_id >/fw/state`
- **Retained**: True

The message Sent by the device contains:

  - Current firmware revision
  - New firmware revision available to install
  - Firmware update status

#### 5.1.4. Example Payload:
```json
{
  "installed_version": "1.25.0",
  "latest_version": "1.25.0",
  "status": "completed"
}
```

#### 5.1.5. Firmware version
* If `latest_version` > `installed_version` means a new firmware is available

#### 5.1.6. Firmware status states:
```
* idle
* updating
* completed
* unknown
```

#### 5.1.7. Home Assistant Message
- **Topic**: `< device_id >/fw/update`
- **Retained**: False

A raw message sent by Home Assistant to start the firmware update:

```
start_update
```

### 5.2. LED Status and Control

Source file: *ha_entities_led.c*

#### 5.2.1. Home Assistant discovery
- **Topic**: `homeassistant/switch/< device_id >_< led >/config` 
- **Example**: `homeassistant/switch/eval3-0209E08B415AD42AC20139_LED_GREEN/config`
- **Category**: Diagnostic

#### 5.2.2. Config Payload Example:
```json
{
  "name": "LED_GREEN",
  "unique_id": "eval3-0209E08B415AD42AC20139_LED_GREEN",
  "command_topic": "eval3-0209E08B415AD42AC20139/led/desired",
  "state_topic": "eval3-0209E08B415AD42AC20139/led/reported",
  "value_template": "{{ value_json.ledStatus.LED_GREEN.reported }}",
  "payload_on": "LED_GREEN_ON",
  "payload_off": "LED_GREEN_OFF",
  "state_on": "ON",
  "state_off": "OFF",
  "availability_topic": "eval3-0209E08B415AD42AC20139/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "eval3-0209E08B415AD42AC20139"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "eval3-0209E08B415AD42AC20139"
  }
}
```

#### 5.2.3. Device Message

- Indicates the current LED status
- **Topic**: `< device_id >/led/reported`
- **Retained**: True
- **Task**: `vLEDTask()` in led_task.c

#### 5.2.4. Device Payload:
```json
{
  "ledStatus": {
    "LED_RED": {
      "reported": "OFF"
    },
    "LED_GREEN": {
      "reported": "ON"
    }
  }
}
```

#### 5.2.5. Home Assistant Message
- **Topic**: `< device_id >/led/desired`  
- **Retained**: True

#### 5.2.6. Example Payload:

```
LED_GREEN_ON 
```

Or

```
LED_GREEN_OFF
```

#### 5.2.7. Related Documentation

For more details see  [related README.md](../led/readme.md)

### 5.3. Button Status

Source file: *ha_entities_button.c*

#### 5.3.1. Home Assistant discovery
- **Topic**: `homeassistant/binary_sensor/< device_id >_< Button >/config`
- **Example**: `homeassistant/binary_sensor/eval3-0209E08B415AD42AC20139_USER_Button/config`

#### 5.3.2. Config Payload Example:
```json
{
  "name": "USER_Button",
  "unique_id": "eval3-0209E08B415AD42AC20139_USER_Button",
  "state_topic": "eval3-0209E08B415AD42AC20139/sensor/button/reported",
  "value_template": "{{ value_json.buttonStatus.USER_Button.reported }}",
  "payload_on": "ON",
  "payload_off": "OFF",
  "device_class": "occupancy",
  "availability_topic": "eval3-0209E08B415AD42AC20139/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "eval3-0209E08B415AD42AC20139"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "eval3-0209E08B415AD42AC20139"
  }
}
```


#### 5.3.3. Device Message
- Indicates the button status (pressed or released).
- **Topic**: `< device_id >/sensor/button/reported`
- **Retained**: True
- **Task**: `vButtonTask()` in button_task.c

#### Payload:
```json
{
  "buttonStatus": {
    "USER_Button": {
      "reported": "ON"
    }
  }
}
```
Or
```json
{
  "buttonStatus": {
    "USER_Button": {
      "reported": "OFF"
    }
  }
}
```

#### 5.3.4. Related Documentation

For more details see  [related README.md](../button/readme.md)

### 5.4. Reboot

Source file: *ha_entities_reset.c*

#### 5.4.1. Home Assistant discovery
- **Topic**: `homeassistant/button/< device_id >_reboot/config`
- **Example**: `homeassistant/button/stm32u585-003000523636500A20333342_reboot/config`
- **Category**: diagnostic

#### 5.4.2. Config Payload Example:
```json
{
  "name": "Reboot",
  "unique_id": "stm32u585-001C00444841500520363230_reboot",
  "command_topic": "stm32u585-001C00444841500520363230/cmd/action",
  "payload_press": "{\"action\":\"reboot\"}",
  "retain": false,
  "availability_topic": "stm32u585-001C00444841500520363230/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": [
      "stm32u585-001C00444841500520363230"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "stm32u585-001C00444841500520363230"
  }
}
```

#### 5.4.3. Home Assistant Message
- Send a command to reboot the device
- **Topic**: `< device_id >/cmd/action`
- **Retained**: False

#### 5.4.4. Example Payload:

```json
{
  "action":"reboot"
}
```

### 5.5. Env sensors (unified)

Source file: *ha_entities_sensors.c*

#### 5.5.1. Env Sensors Home Assistant discovery

Each environmental metric is registered as a separate sensor in Home Assistant:

  - **Topics**:
    - `homeassistant/sensor/< device_id >_temp_0_c/config`
    - `homeassistant/sensor/< device_id >_temp_1_c/config`
    - `homeassistant/sensor/< device_id >_rh_pct/config`
    - `homeassistant/sensor/< device_id >_baro_mbar/config`
    - `homeassistant/sensor/< device_id >_als_lux/config`
    - `homeassistant/sensor/< device_id >_white_lux/config`

  - **Example**: `homeassistant/sensor/stm32u585-003000523636500A20333342_temp_0_c/config`

#### 5.5.2. Env sensor Config Payload Example
```json
{
  "name": "Temperature 0",
  "unique_id": "stm32u585-001C00444841500520363230_temp_0_c",
  "state_topic": "stm32u585-001C00444841500520363230/sensor/env",
  "value_template": "{{ value_json.temp_0_c }}",
  "device_class": "temperature",
  "unit_of_measurement": "°C",
  "availability_topic": "stm32u585-001C00444841500520363230/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "stm32u585-001C00444841500520363230"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "stm32u585-001C00444841500520363230"
  }
}
```

#### 5.5.3. Device Message
- **Topic**: `< device_id >/sensor/env`
- **Retained**: False
- **Task**: `vEnvironmentSensorPublishTask()` in env_sensor_publish.c

#### 5.5.4. Device Payload:
```json
{
  "temp_0_c": 22.3,
  "temp_1_c": 22.1,
  "rh_pct": 50.2,
  "baro_mbar": 998.1,
  "als_lux": 0,
  "white_lux": 0
}
```

#### 5.5.5. Related Documentation

For more details see  [related README.md](../sensors/env_sensor_readme.md)

### 5.6. Motion Sensors (Accel, Gyro, Mag)

Source file: *ha_entities_sensors.c*

#### 5.6.1. Motion Sensors Home Assistant discovery

Each axis is registered as a separate sensor in Home Assistant:

  - **Topics**: 
    - `homeassistant/sensor/< device_id >_acceleration_x/config`
    - `homeassistant/sensor/< device_id >_acceleration_y/config`
    - `homeassistant/sensor/< device_id >_acceleration_z/config`

    - `homeassistant/sensor/< device_id >_gyro_x/config`
    - `homeassistant/sensor/< device_id >_gyro_y/config`
    - `homeassistant/sensor/< device_id >_gyro_z/config`

    - `homeassistant/sensor/< device_id >_magnetometer_x/config`
    - `homeassistant/sensor/< device_id >_magnetometer_y/config`
    - `homeassistant/sensor/< device_id >_magnetometer_z/config`

  - **Example**: `homeassistant/sensor/stm32u585-003000523636500A20333352_acceleration_x/config`

#### 5.6.2. Motion sensor Config Payload Example:
```json
{
  "name": "Acceleration_x",
  "unique_id": "stm32u585-001C00555851500520363230_acceleration_x",
  "state_topic": "stm32u585-001C00555851500520363230/sensor/motion",
  "value_template": "{{ value_json.acceleration_mG.x }}",
  "device_class": "Acceleration",
  "unit_of_measurement": "mG",
  "availability_topic": "stm32u585-001C00555851500520363230/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "stm32u585-001C00555851500520363230"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "stm32u585-001C00555851500520363230"
  }
}
```

#### 5.6.3. Device Message

- **Topic**: `< device_id >/sensor/motion`
- **Retained**: False
- **Task**: `vMotionSensorsPublish()` in motion_sensors_publish.c

#### 5.6.4. Device Payload:
```json
{
  "acceleration_mG":{
    "x": -553,
    "y": 855,
    "z": 969
  },
  "gyro_mDPS":{
    "x": 659,
    "y": 720,
    "z": -372
  },
  "magnetometer_mGauss":{
    "x": 560,
    "y": -555,
    "z": 609
  }
}
```
#### 5.6.5. Related Documentation
For more details see  [related README.md](../sensors/motion_sensor_readme.md)

### 5.7. Ranging Sensor

Source file: *ha_entities_sensors.c*

#### 5.7.1. Ranging Sensor Home Assistant discovery

- **Topic**: `homeassistant/sensor/< device_id >_distance_mm/config`
- **Example**: `homeassistant/sensor/stm32u585-003000523636500A20333352_distance_mm/config`

#### 5.7.2. Ranging sensor Config Payload Example:
```json
{
  "name": "Ranging Distance",
  "unique_id": "stm32u585-001C00444841500520363230_distance_mm",
  "state_topic": "stm32u585-001C00444841500520363230/sensor/ranging/reported",
  "value_template": "{{ value_json.distance_mm }}",
  "unit_of_measurement": "mm",
  "device_class": "distance",
  "state_class": "measurement",
  "availability_topic": "stm32u585-001C00444841500520363230/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "stm32u585-001C00444841500520363230"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "stm32u585-001C00444841500520363230"
  }
}
```

#### 5.7.3. Device Message
- **Topic**: `< device_id >/sensor/ranging/reported`
- **Retained**: True
- **Task**: `vRangingSensorTask()` in ranging_sensor.c

#### 5.7.4. Device Payload:
```json
{
  "distance_mm": 742
}
```

#### 5.7.5. Related Documentation
For more details see  [related README.md](../sensors/ranging_sensor_readme.md)

### 5.8. Device Availability

Source file: *ha_helpers.c*

#### 5.8.1. Device Message
- **Topic**: `< device_id >/status/availability`
- **Retained**: True
- **Example**: `stm32u585-003000523636500A20333352/status/availability`

#### 5.8.2. Example Payload:

```
online
```

Or 

```
offline
```

### 5.9. Cover (Garage Door)
The MQTT cover integration allows you to control an MQTT cover (such as blinds, a roller shutter or a garage door).

https://www.home-assistant.io/integrations/cover.mqtt/

Here we have an example of a garage door.

#### 5.9.1. Cover Home Assistant discovery

Source file: *ha_entities_cover.c*

1. Key points from the MQTT Cover, Home Assistant expects at minimum:

    - platform: "cover" (required for discovery)
    - command_topic
    - state_topic (optional but recommended)
    - payload_open, payload_close, payload_stop
    - state_open, state_opening, state_closed, state_closing  

2. Home Assistant Will Discover
- You will get three separate entities:

    - cover.garage_door_1
    - cover.garage_door_2
    - cover.garage_door_3

Each with its own:

Commands

- Commands
    - OPEN
    - CLOSE
    - STOP

- State reporting
    - open
    - opening
    - closed
    - closing

#### 5.9.2. Cover Payload Example:
```json
{
  "platform": "cover",
  "name": "GARAGE_DOOR_1",
  "unique_id": "eval3-02093088825AD42AC20139_GARAGE_DOOR_1",
  "command_topic": "eval3-02093088825AD42AC20139/cover/GARAGE_DOOR_1/desired",
  "state_topic": "eval3-02093088825AD42AC20139/cover/GARAGE_DOOR_1/state",
  "payload_open": "OPEN",
  "payload_close": "CLOSE",
  "payload_stop": "STOP",
  "state_open": "open",
  "state_opening": "opening",
  "state_closed": "closed",
  "state_closing": "closing",
  "availability_topic": "eval3-02093088825AD42AC20139/status/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "retain": false,
  "device": {
    "identifiers": [
      "eval3-02093088825AD42AC20139"
    ],
    "manufacturer": "STMicroelectronics",
    "model": "B_U585_IOTA02",
    "name": "eval3-02093088825AD42AC20139"
  }
}
```

#### 5.9.3. Topics
Command:
```
<thing>/cover/GARAGE_DOOR_1/desired
<thing>/cover/GARAGE_DOOR_2/desired
<thing>/cover/GARAGE_DOOR_3/desired
```

State:
```
<thing>/cover/GARAGE_DOOR_1/state
<thing>/cover/GARAGE_DOOR_2/state
<thing>/cover/GARAGE_DOOR_3/state
```

Availability:
```
<thing>/status/availability
```

#### 5.9.4. Related Documentation
For full details on the garage door cover implementation, including relay wiring, sensor configuration, MQTT topics, and firmware architecture, see the [cover README.md](../cover/README.md)
