# Mosquitto Provisioning

This guide covers provisioning to the public Mosquitto test broker.

The project uses X.509 certificates to securely communicate with Mosquitto over a mutual TLS (mTLS) connection.
Device certificates generated in this flow are valid for 90 days.

## Prerequisites

- Board connected through STLink Virtual COM
- Internet access
- Wi-Fi credentials in `bin/config.json`

Helpful links:

- https://test.mosquitto.org/
- https://mosquitto.org/

## Recommended Flow (`run_all.ps1`)

1. Edit `bin/config.json`:
   - `"broker_type": "mosquitto"`
   - Wi-Fi credentials
2. Run:

```powershell
cd bin
.\run_all.ps1
```

The script flow:

- Flashes firmware
- Runs Mosquitto provisioning
- Generates/imports certificate material
- Stores endpoint/port and reboots the device

## Provisioning Only

If firmware is already flashed:

```powershell
bin\provision_mosquitto.ps1
```

## Notes

- If COM port is busy, close other serial tools and rerun.
- Some certificate steps may require manual confirmation.
