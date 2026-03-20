# AWS Provisioning

This guide covers the AWS IoT Core onboarding flow used by this project.

## Prerequisites

- AWS account
- AWS CLI installed
- `aws configure` completed
- Board connected through STLink Virtual COM

Helpful links:

- AWS IoT Core: https://aws.amazon.com/iot-core/
- AWS CLI install: https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html
- `aws configure`: https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html

## Recommended Flow (`run_all.ps1`)

1. Edit `bin/config.json`:
   - `"broker_type": "aws"`
   - Wi-Fi credentials
2. Run:

```powershell
cd bin
.\run_all.ps1
```

The script flow:

- Flashes firmware
- Runs AWS provisioning
- Creates/configures AWS IoT resources (thing/certificate/policy)
- Stores endpoint/port and reboots the device

## Provisioning Only

If firmware is already flashed:

```powershell
bin\provision_aws_single.ps1
```

## Notes

- If COM port is busy, close other serial tools and rerun.
- If AWS CLI commands fail, verify IAM permissions for IoT operations.
