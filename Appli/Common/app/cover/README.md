# STM32U585 MQTT Garage Door Cover Control (Home Assistant Discovery)

This module documents the `cover_task.c` implementation for **STM32U585 / B-U585I-IOT02A** garage door control over **MQTT**, with **Home Assistant MQTT Discovery** support.

It supports:

- 1 to 3 garage doors
- Relay pulse motor control
- Optional magnetic door sensors
- Optional ranging sensor mode
- Automatic Home Assistant cover entity discovery

## Hardware Overview

Recommended relay board:

- Seeed Studio Relay Shield V2: https://wiki.seeedstudio.com/Relay_Shield_V2/

Optional door sensor:

- Any NC/NO magnetic reed switch compatible with your GPIO voltage and wiring.

### Relay to STM32 Pin Mapping

The firmware uses up to 3 active-high relays.

```c
#define RELAY_1_Pin                  ARD_D07_Pin
#define RELAY_1_Port                 ARD_D07_GPIO_Port

#define RELAY_2_Pin                  ARD_D06_Pin
#define RELAY_2_Port                 ARD_D06_GPIO_Port

#define RELAY_3_Pin                  ARD_D05_Pin
#define RELAY_3_Port                 ARD_D05_GPIO_Port

#define RELAY_4_Pin                  ARD_D04_Pin
#define RELAY_4_Port                 ARD_D04_GPIO_Port
```

Each command triggers a 1-second relay pulse to emulate a physical wall-button press.

### Optional Magnetic Sensor Mapping

Note: the symbol names are intentionally `DOOR_SENSPR_*` to match the firmware source.

```c
#define DOOR_SENSPR_1_Pin            ARD_D03_Pin
#define DOOR_SENSPR_1_Port           ARD_D03_GPIO_Port
#define DOOR_SENSPR_1_IRQn           ARD_D03_EXTI_IRQn
#define DOOR_SENSPR_1_STATE_OPEN     GPIO_PIN_SET

#define DOOR_SENSPR_2_Pin            ARD_D08_Pin
#define DOOR_SENSPR_2_Port           ARD_D08_GPIO_Port
#define DOOR_SENSPR_2_IRQn           ARD_D08_EXTI_IRQn
#define DOOR_SENSPR_2_STATE_OPEN     GPIO_PIN_SET

#define DOOR_SENSPR_3_Pin            ARD_D09_Pin
#define DOOR_SENSPR_3_Port           ARD_D09_GPIO_Port
#define DOOR_SENSPR_3_IRQn           ARD_D09_EXTI_IRQn
#define DOOR_SENSPR_3_STATE_OPEN     GPIO_PIN_SET
```

## Compile-Time Configuration

Set number of covers:

```c
#define NUM_COVERS 1   // valid: 1, 2, 3
```

Sensor options:

```c
#define USE_MAGNETIC_SENSOR 1  // 0 or 1
#define USE_RANGING_SENSOR  0  // 0 or 1
```

Important constraints from firmware:

- `USE_MAGNETIC_SENSOR` and `USE_RANGING_SENSOR` are mutually exclusive.
- `USE_RANGING_SENSOR` supports only one cover (`NUM_COVERS == 1`).

If both sensor options are disabled, the reported state remains sensor-derived `unknown`:

- startup state: `unknown`
- after command: still reports `unknown` (because no sensor feedback is available)

## MQTT Topics and Payloads

Command topic:

```text
<thing_name>/cover/<COVER_NAME>/desired
```

State topic:

```text
<thing_name>/cover/<COVER_NAME>/state
```

Home Assistant discovery topic:

```text
homeassistant/cover/<thing_name>_<COVER_NAME>/config
```

Command payloads:

```text
OPEN
CLOSE
STOP
```

State payloads:

```text
open
closed
stopped
unknown
```

### Example

```text
Topic:   stm32u585-002C005B3332511738363236/cover/GARAGE_DOOR_1/desired
Payload: OPEN
```

```text
Topic:   stm32u585-002C005B3332511738363236/cover/GARAGE_DOOR_1/state
Payload: open
```

## Home Assistant Integration

The firmware publishes retained discovery data so Home Assistant can auto-create cover entities.

Expected entities:

- `cover.garage_door_1`
- `cover.garage_door_2`
- `cover.garage_door_3` (when configured)

No YAML entity definition is required.

For bridge/discovery setup, see:

- [Home Assistant Discovery Guide](../HomeAssistant/home_assistant_discovery.md)

## Firmware Behavior (cover_task.c)

Main flow:

1. `vCoverTask()` waits for MQTT readiness, loads ThingName from KVStore, subscribes to cover command topics, and publishes initial state.
2. `prvIncomingPublishCallback()` parses `.../cover/<name>/desired` and dispatches command handling.
3. `prvHandleCoverCommand()` maps `OPEN/CLOSE/STOP` to relay pulse actions.
4. `prvPublishCoverStates()` publishes per-cover state topics.
5. `prvReadDoorSensor()` maps sensor/ranging input to `open/closed/unknown`.

Sensor event model:

- Magnetic sensors use EXTI callbacks and event-bit wakeups.
- This path is event-driven (not fixed-rate polling).

## Relay Pulse Implementation

The relay pulse is implemented with FreeRTOS delay timing:

```c
HAL_GPIO_WritePin(pxRelay->pxPort, pxRelay->usPin, GPIO_PIN_SET);
vTaskDelay(pdMS_TO_TICKS(1000));
HAL_GPIO_WritePin(pxRelay->pxPort, pxRelay->usPin, GPIO_PIN_RESET);
```

## Monitoring MQTT Traffic

You can test with:

- AWS IoT MQTT test client
- [MQTTX Web Client](https://mqttx.app/web-client)

Subscribe:

```text
<thing_name>/cover/+/state
```

Publish:

```text
<thing_name>/cover/+/desired
```

You can use any MQTT client to monitor and control cover status.

For mosquitto and EMQX you can use MQTTX Web Client
- https://mqttx.app/web-client

Configuration for mosquitto
- ![mosquitto](../../../../assets/wqttx_conf_mosquiotto.png)

Configuration for EMQX
- ![EMQX](../../../../assets/wqttx_conf_emqx.png)

## Related Docs

- [Home Assistant Discovery](../HomeAssistant/home_assistant_discovery.md)
- [LED App README](../led/readme.md)
- [Button App README](../button/readme.md)
