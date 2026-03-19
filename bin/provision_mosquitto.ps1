# Load configuration
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$configPath = Join-Path $scriptDir "config.json"
$config = Get-Content -Raw -Path $configPath | ConvertFrom-Json
$brokerType = $config.broker_type

# Guard: this script is mosquitto-only
if ($brokerType -ne "mosquitto") {
    throw "This script supports broker_type 'mosquitto' only. Current value: '$brokerType'."
}

# Auto-detect STLink Virtual COM Port
$portName = Get-CimInstance Win32_SerialPort | Where-Object { $_.Name -like "*STLink Virtual COM Port*" } | Select-Object -ExpandProperty DeviceID

if (-not $portName) {
    Write-Warning "STLink Virtual COM Port not found. Using fallback from config.json."
    $portName = $config.portName
}

$baudRate = 115200
$serialPort = $null

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

# Root CA download settings
$defaultMosquittoRootCaUrl = "https://test.mosquitto.org/ssl/mosquitto.org.crt"
$mosquittoRootCaUrl = $defaultMosquittoRootCaUrl
$mosquittoRootCaPath = Join-Path $scriptDir "mosquitto.org.crt"
$mosquittoEndpoint = "test.mosquitto.org"
$mosquittoPort = 8884

function Send-Command {
    param (
        [string]$command,
        [int]$timeoutMs = 1500
    )

    $serialPort.WriteLine($command)

    $response = ""
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        if ($serialPort.BytesToRead -gt 0) {
            $response += $serialPort.ReadExisting()
        }
        Start-Sleep -Milliseconds 50
    }

    return $response
}

# Send a full PEM/text file over the CLI serial session
function Send-FileContent {
    param ([string]$filePath)
    $content = Get-Content -Raw -Path $filePath
    $serialPort.WriteLine($content)
    Start-Sleep -Milliseconds 500
}

# Extract a PEM block from command response text
function Get-PemBlock {
    param(
        [string]$Text,
        [string]$BeginMarker,
        [string]$EndMarker
    )

    $pattern = [Regex]::Escape($BeginMarker) + "(.|\s)*?" + [Regex]::Escape($EndMarker)
    if ($Text -match $pattern) {
        return $matches[0].Trim()
    }

    return $null
}

# Submit CSR to test.mosquitto.org and save returned client certificate
function Request-MosquittoClientCert {
    param(
        [string]$CsrPem,
        [string]$OutPath
    )

    try {
        $response = Invoke-WebRequest -Uri "https://test.mosquitto.org/ssl/index.php" -Method Post -Body @{ csr = $CsrPem } -UseBasicParsing
        $certPem = Get-PemBlock -Text $response.Content -BeginMarker "-----BEGIN CERTIFICATE-----" -EndMarker "-----END CERTIFICATE-----"

        if (-not $certPem) {
            if ($response.Content -match "Corrupted CSR") {
                Write-Warning "Mosquitto rejected CSR as corrupted. Falling back to manual certificate step."
            } else {
                Write-Warning "Mosquitto certificate response did not contain a PEM certificate. Falling back to manual certificate step."
            }
            return $false
        }

        $certPem | Out-File -Encoding ascii $OutPath
        Write-Host "Mosquitto client certificate auto-generated and saved to $OutPath"
        return $true
    }
    catch {
        Write-Warning "Automatic Mosquitto certificate generation failed: $($_.Exception.Message)"
        Write-Warning "Falling back to manual certificate step."
        return $false
    }
}

# Download root CA file before importing to device
function Ensure-RootCaFile {
    param(
        [string]$Url,
        [string]$OutPath
    )

    Write-Host "Downloading root CA: $Url"
    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutPath -UseBasicParsing
    }
    catch {
        throw "Failed to download root CA from '$Url': $($_.Exception.Message)"
    }
}

try {
    Assert-SerialPortAvailable -PortName $portName -BaudRate $baudRate

    # Open serial session to board CLI
    $serialPort = New-Object System.IO.Ports.SerialPort $portName, $baudRate, "None", 8, "One"
    $serialPort.Open()

    # Wi-Fi settings
    Send-Command "conf set wifi_ssid "
    Send-Command "conf set wifi_credential "
    Send-Command "conf commit"
    Send-Command "reset"
    Start-Sleep -Seconds 5

    # Root CA
    Ensure-RootCaFile -Url $mosquittoRootCaUrl -OutPath $mosquittoRootCaPath
    Send-Command "pki import cert root_ca_cert"
    Send-FileContent $mosquittoRootCaPath

    # Thing name
    $thingName = "unknown_device"
    $response = Send-Command "conf get thing_name"
    if ($response -match 'thing_name\s*=\s*"([^"]+)"') {
        $thingName = $matches[1]
        Write-Host "Thing Name detected: $thingName"
    } else {
        Write-Warning "Failed to read thing_name"
    }

    # Key + CSR generation
    Send-Command "pki generate key" 4000 | Out-Null
    $csrResponse = Send-Command "pki generate csr" 6000
    $csrPem = Get-PemBlock -Text $csrResponse -BeginMarker "-----BEGIN CERTIFICATE REQUEST-----" -EndMarker "-----END CERTIFICATE REQUEST-----"

    if (-not $csrPem) {
        throw "Failed to read CSR from device response. Try running `pki generate csr` manually and verify CLI output."
    }

    $csrFile = Join-Path $scriptDir "$thingName.csr.pem"
    $csrPem | Out-File -Encoding ascii $csrFile
    Write-Host "CSR saved to $csrFile"

    $tlsCertPath = $null
    if ($config.PSObject.Properties.Name -contains "tls_cert" -and -not [string]::IsNullOrWhiteSpace($config.tls_cert)) {
        if ([System.IO.Path]::IsPathRooted($config.tls_cert)) {
            $tlsCertPath = $config.tls_cert
        } else {
            $tlsCertPath = Join-Path $scriptDir $config.tls_cert
        }
    } else {
        $tlsCertPath = Join-Path $scriptDir "client.crt"
    }

    $certReady = $false
    if (Request-MosquittoClientCert -CsrPem $csrPem -OutPath $tlsCertPath) {
        $certReady = $true
    }

    if (-not $certReady) {
        Write-Host "Next step: Visit https://test.mosquitto.org/ssl/, paste CSR, submit, and download client certificate."
        while (-not (Test-Path -LiteralPath $tlsCertPath -PathType Leaf)) {
            $inputPath = Read-Host "Client certificate not found at '$tlsCertPath'. Enter certificate path after download"
            if ([string]::IsNullOrWhiteSpace($inputPath)) {
                continue
            }
            if ([System.IO.Path]::IsPathRooted($inputPath)) {
                $tlsCertPath = $inputPath
            } else {
                $tlsCertPath = Join-Path $scriptDir $inputPath
            }
        }
    }

    # Import client cert
    Send-Command "pki import cert tls_cert"
    Send-FileContent $tlsCertPath
    Write-Host "Mosquitto client certificate imported from $tlsCertPath"

    # MQTT config
    Send-Command "conf set mqtt_endpoint $mosquittoEndpoint"
    Send-Command "conf set mqtt_port $mosquittoPort"
    Send-Command "conf set wifi_ssid $($config.wifi_ssid)"
    Send-Command "conf set wifi_credential $($config.wifi_credential)"

    Send-Command "conf commit"
    Start-Sleep -Seconds 1
    Send-Command "reset"
}
catch {
    Write-Host ""
    Write-Host "COM port '$portName' is currently used by another application."
    Write-Host "Close/disconnect the application using the COM port, then run this script again."
    exit 1
}
finally {
    # Always close serial port, even on failure
    if ($null -ne $serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "Serial port closed."
    }
}
