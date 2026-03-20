# STM32N6570-DK + ST67W611M1
### Secure MQTT-over-TLS Reference Firmware

[![Board: STM32N6570-DK](https://img.shields.io/badge/Board-STM32N6570--DK-03234B)](https://www.st.com/en/evaluation-tools/stm32n6570-dk.html)
[![RTOS: FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-1A73E8)](https://www.freertos.org/)
[![Network: LwIP](https://img.shields.io/badge/Network-LwIP-006064)](https://savannah.nongnu.org/projects/lwip/)
[![TLS: MbedTLS 3.1.1](https://img.shields.io/badge/TLS-MbedTLS%203.1.1-283593)](https://www.keil.arm.com/packs/mbedtls-arm/versions/)
[![Wi-Fi: ST67W611M1](https://img.shields.io/badge/Wi--Fi-ST67W611M1-0B8043)](https://www.st.com/content/st_com/en/campaigns/st67w-wifi6-bluetooth-thread-module-z13.html)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](./LICENSE.md)

This repository provides a complete MQTT-over-TLS reference for the [STM32N6570-DK](https://www.st.com/en/evaluation-tools/stm32n6570-dk.html) paired with the [ST67W611M1](https://www.st.com/content/st_com/en/campaigns/st67w-wifi6-bluetooth-thread-module-z13.html).

It is built for a repeatable bring-up workflow: flash, provision, validate, and then move to source-level build/debug in STM32CubeIDE.
Validated broker flows in this repository are AWS IoT Core and Mosquitto.

## What This Project Covers

- Hardware:
  - STM32N6570-DK
  - ST67W611M1 (T02 mission profile)
- Application demos:
  - LED control over MQTT
  - Button event reporting over MQTT
- Provisioning targets:
  - AWS IoT Core
  - Mosquitto

## Required Software

- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) (required for flashing)
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) (required for build/debug from source)
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) (required for project regeneration)

If you use AWS IoT Core:

- [AWS account](https://aws.amazon.com/)
- [AWS CLI installation](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
- [`aws configure` quickstart](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)

## Quick Start

1. Clone with submodules:
   - `git clone https://github.com/SlimJallouli/stm32n6570_dk_w6x_iot_reference.git --recurse-submodules`
2. Update ST67 to T02 mission profile using `NCP_update_mission_profile_t02`:
   - https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Projects/ST67W6X_Scripts/Binaries
3. Move to the scripts directory:
   - `cd bin`
4. Edit broker and Wi-Fi settings in [`config.json`](bin/config.json)
5. Run:
   - `.\run_all.ps1`
6. Open serial logs and validate LED/Button MQTT behavior.

For full scripted flashing/provisioning details, see [`bin/readme.md`](bin/readme.md).

## Runtime Architecture

```mermaid
flowchart TD
    A[StartDefaultTask] --> B[Task_CLI]
    A --> C[KVStore_init]
    A --> D[net_main - ST67]
    A --> E[vMQTTAgentTask]
    E --> F[vLEDTask]
    E --> G[vButtonTask]
```

## Documentation Guide

| Topic | File |
|---|---|
| Architecture and middleware | [docs/architecture.md](docs/architecture.md) |
| Software components | [docs/software_components.md](docs/software_components.md) |
| Build, debug, and flash | [docs/debug.md](docs/debug.md) |
| MQTT topic/data model | [docs/mqtt_data_model.md](docs/mqtt_data_model.md) |
| AWS provisioning | [docs/provisioning_aws.md](docs/provisioning_aws.md) |
| Mosquitto provisioning | [docs/provisioning_mosquitto.md](docs/provisioning_mosquitto.md) |
| Repository structure | [docs/repo_structure.md](docs/repo_structure.md) |
| Troubleshooting | [docs/troubleshooting.md](docs/troubleshooting.md) |

## Module Guides

- Button app: [Appli/Common/app/button/readme.md](Appli/Common/app/button/readme.md)
- LED app: [Appli/Common/app/led/readme.md](Appli/Common/app/led/readme.md)
- CLI: [Appli/Common/cli/ReadMe.md](Appli/Common/cli/ReadMe.md)
- Crypto: [Appli/Common/crypto/ReadMe.md](Appli/Common/crypto/ReadMe.md)
- KVStore: [Appli/Common/kvstore/ReadMe.md](Appli/Common/kvstore/ReadMe.md)

## Build and Flash Paths

- Import both projects into STM32CubeIDE:
  - `FSBL`
  - `Appli`
- Build and debug/flash details:
  - [docs/debug.md](docs/debug.md)

## Git Submodules Used

- [corePKCS11](https://github.com/FreeRTOS/corePKCS11)
- [littlefs](https://github.com/littlefs-project/littlefs)
- [tinycbor](https://github.com/intel/tinycbor)
