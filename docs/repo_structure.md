# Repository Structure

This document maps the repository layout and the role of each major module.

## Top-Level Layout

- `FSBL/` - first-stage bootloader project
- `Appli/` - main firmware project (FreeRTOS, networking, TLS, MQTT, application tasks)
- `bin/` - script-based flashing and provisioning flow
- `docs/` - technical documentation
- `provision_*.md` - additional provisioning guides

## `Appli/Common` Overview

### `app/`

Application behavior on top of the shared MQTT session:

- `mqtt/` - MQTT agent task, reconnect logic, subscription dispatch
- `led/` - LED desired/reported control
- `button/` - button event reporting

### `cli/`

UART CLI framework and command handlers, including PKI and runtime configuration commands.

### `config/`

Central middleware/runtime configuration headers:

- `kvstore_config.h`
- `lwipopts.h`
- `mbedtls_config.h`
- `core_mqtt_config.h`

### `crypto/`

TLS and PKCS#11 integration:

- mbedTLS transport layer
- FreeRTOS integration hooks
- PKCS#11 PAL/helper components

### `include/`

Shared public headers used across modules.

### `kvstore/`

Non-volatile runtime configuration storage (KVS implementation).

### `net/`

ST67 networking integration:

- `W6X_ARCH_T02/` - ST67 T02 architecture path
- `lwip_port/` - LwIP and FreeRTOS glue

### `sys/`

Shared system utilities and support functions.

## `Appli/Common/net/W6X_ARCH_T02`

- `st67w6x_netconn.c` - network entry point (`net_main`) and event callback orchestration
- `lwip.c` - interface bring-up/teardown, DHCP/IP event handling
- `lwip_netif.c` - packet bridge between ST67 driver and LwIP netif
- `dhcp_server_raw.c` - DHCP server implementation
