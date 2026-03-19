# Stop script if any command fails
$ErrorActionPreference = "Stop"

Write-Host "==============================="
Write-Host " Starting Flash + Config Flow"
Write-Host "==============================="

# Get current script directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$configPath = Join-Path $scriptDir "config.json"

# Resolve flash script path
$flashScript  = Join-Path $scriptDir "flash.ps1"
$logPath = Join-Path $scriptDir "log.txt"

# Verify scripts exist
if (-not (Test-Path $flashScript)) {
    Write-Error "flash.ps1 not found at $flashScript"
    exit 1
}

if (-not (Test-Path $configPath)) {
    Write-Error "config.json not found at $configPath"
    exit 1
}

# Read broker type to select provisioning flow
$config = Get-Content -Raw -Path $configPath | ConvertFrom-Json
$brokerType = $config.broker_type

function Resolve-BoardSerialPort {
    param(
        [Parameter(Mandatory = $true)]$Config
    )

    $autoPortName = Get-CimInstance Win32_SerialPort | Where-Object { $_.Name -like "*STLink Virtual COM Port*" } | Select-Object -ExpandProperty DeviceID
    if (-not [string]::IsNullOrWhiteSpace($autoPortName)) {
        return $autoPortName
    }

    if ($Config.PSObject.Properties.Name -contains "portName" -and -not [string]::IsNullOrWhiteSpace($Config.portName)) {
        Write-Warning "STLink Virtual COM Port not found. Using fallback from config.json."
        return [string]$Config.portName
    }

    throw "STLink Virtual COM Port not found and no fallback 'portName' is set in config.json."
}

function Assert-SerialPortAvailable {
    param(
        [Parameter(Mandatory = $true)][string]$PortName,
        [Parameter(Mandatory = $true)][int]$BaudRate
    )

    if ([string]::IsNullOrWhiteSpace($PortName)) {
        throw "Serial port is not configured. Set 'portName' in config.json or connect STLink Virtual COM Port."
    }

    $probePort = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, "None", 8, "One"
    try {
        $probePort.Open()
        $probePort.Close()
    }
    catch [System.UnauthorizedAccessException] {
        throw "Serial port '$PortName' is currently in use or access is denied. Close other serial terminals/tools using this COM port and retry."
    }
    catch [System.IO.IOException] {
        throw "Serial port '$PortName' cannot be opened. Verify the board connection and that no other program is using the COM port."
    }
    finally {
        if ($null -ne $probePort -and $probePort.IsOpen) {
            $probePort.Close()
        }
        if ($null -ne $probePort) {
            $probePort.Dispose()
        }
    }
}

# Select provision script based on broker_type
switch ($brokerType) {
    "mosquitto" { $provisionScript = Join-Path $scriptDir "provision_mosquitto.ps1" }
    "aws" { $provisionScript = Join-Path $scriptDir "provision_aws_single.ps1" }
    default {
        Write-Error "Unsupported broker_type '$brokerType'. Use 'mosquitto' or 'aws' in config.json."
        exit 1
    }
}

if (-not (Test-Path $provisionScript)) {
    Write-Error "Provision script not found at $provisionScript"
    exit 1
}

# Early serial port preflight
$portName = $null
try {
    $portName = Resolve-BoardSerialPort -Config $config
    Write-Host "Preflight: checking serial port availability ($portName)..."
    Assert-SerialPortAvailable -PortName $portName -BaudRate 115200
}
catch {
    Write-Host ""
    if (-not [string]::IsNullOrWhiteSpace($portName)) {
        Write-Host "COM port '$portName' is currently used by another application."
    } else {
        Write-Host "Unable to access the board COM port."
    }
    Write-Host "Close/disconnect the application using the COM port, then run this script again."
    exit 1
}

# Prompt user to set board to Dev mode before flashing
Write-Host ""
Write-Host "Board setup required:"
Write-Host "Set the STM32N6-DK board to Dev mode."
[void](Read-Host "Press Enter to continue")

# Run flash
Write-Host "`n--- Running Flash Script ---"
& $flashScript

if ($LASTEXITCODE -ne 0) {
    Write-Error "Flash failed. Aborting."
    exit $LASTEXITCODE
}

Write-Host "Flash completed successfully."

# Optional small delay
Start-Sleep -Seconds 2

# Prompt user to switch board mode before provisioning
Write-Host ""
Write-Host "Board setup required:"
Write-Host "Set the STM32N6-DK board to Flash mode, then power-cycle the board."
[void](Read-Host "Press Enter to continue")

# Run provision
Write-Host "`n--- Running Provision Script ($brokerType) ---"
# Show live output and append the same output to log.txt
& $provisionScript *>&1 | Tee-Object -FilePath $logPath -Append

if ($LASTEXITCODE -ne 0) {
    Write-Error "Provision failed."
    exit $LASTEXITCODE
}

Write-Host "`nAll steps completed successfully."
