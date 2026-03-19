# Load configuration
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$configPath = Join-Path $scriptDir "config.json"
$config = Get-Content -Raw -Path $configPath | ConvertFrom-Json
$brokerType = $config.broker_type

# Guard: this script is AWS single-thing flow only
if ($brokerType -ne "aws") {
    throw "This script supports broker_type 'aws' only. Current value: '$brokerType'."
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
$defaultAwsRootCaUrl = "https://www.amazontrust.com/repository/SFC2CA-SFSRootCAG2.pem"
$awsRootCaUrl = $defaultAwsRootCaUrl
$awsRootCaPath = Join-Path $scriptDir "SFSRootCAG2.pem"
$awsMqttPort = 8883

# AWS IoT policy to attach to generated certificate
$awsPolicyName = $config.aws_policy_name
if ([string]::IsNullOrWhiteSpace($awsPolicyName)) {
    $awsPolicyName = "AllowAllDev"
}

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

function Invoke-AwsCli {
    param(
        [string[]]$CommandArgs,
        [switch]$AllowFailure
    )

    if ($null -eq $CommandArgs -or $CommandArgs.Count -eq 0) {
        throw "Invoke-AwsCli called without command arguments."
    }

    $fullArgs = @("--debug") + $CommandArgs
    Write-Host "Running AWS CLI: aws $($fullArgs -join ' ')"

    $stdoutFile = [System.IO.Path]::GetTempFileName()
    $stderrFile = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath "aws" -ArgumentList $fullArgs -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
        $stdout = ""
        $stderr = ""

        if (Test-Path -LiteralPath $stdoutFile) {
            $stdoutRaw = Get-Content -Raw -Path $stdoutFile -ErrorAction SilentlyContinue
            if ($null -ne $stdoutRaw) {
                $stdout = ([string]$stdoutRaw).Trim()
            }
        }
        if (Test-Path -LiteralPath $stderrFile) {
            $stderrRaw = Get-Content -Raw -Path $stderrFile -ErrorAction SilentlyContinue
            if ($null -ne $stderrRaw) {
                $stderr = ([string]$stderrRaw).Trim()
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host "AWS CLI stdout:"
            Write-Host $stdout
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Host "AWS CLI stderr/debug:"
            Write-Host $stderr
        }

        if (-not $AllowFailure -and $process.ExitCode -ne 0) {
            throw "AWS CLI command failed: aws $($CommandArgs -join ' ')`n$stderr`n$stdout"
        }

        return [PSCustomObject]@{
            ExitCode = $process.ExitCode
            StdOut   = $stdout
            StdErr   = $stderr
        }
    }
    finally {
        if (Test-Path -LiteralPath $stdoutFile) { Remove-Item -LiteralPath $stdoutFile -Force -ErrorAction SilentlyContinue }
        if (Test-Path -LiteralPath $stderrFile) { Remove-Item -LiteralPath $stderrFile -Force -ErrorAction SilentlyContinue }
    }
}

function Ensure-AwsThing {
    param(
        [string]$ThingName
    )

    $describe = Invoke-AwsCli -CommandArgs @("iot", "describe-thing", "--thing-name", $ThingName) -AllowFailure
    if ($describe.ExitCode -eq 0) {
        return
    }

    Write-Host "Creating AWS IoT Thing: $ThingName"
    Invoke-AwsCli -CommandArgs @("iot", "create-thing", "--thing-name", $ThingName) | Out-Null
}

function Ensure-AttachThingPrincipal {
    param(
        [string]$ThingName,
        [string]$PrincipalArn
    )

    $result = Invoke-AwsCli -CommandArgs @("iot", "attach-thing-principal", "--thing-name", $ThingName, "--principal", $PrincipalArn) -AllowFailure
    if ($result.ExitCode -eq 0) {
        return
    }

    $msg = "$($result.StdErr)`n$($result.StdOut)"
    if ($msg -match "already" -or $msg -match "ResourceAlreadyExistsException") {
        return
    }

    throw "Failed to attach thing principal.`n$msg"
}

function Ensure-AwsPolicyExists {
    param(
        [string]$PolicyName
    )

    $describe = Invoke-AwsCli -CommandArgs @("iot", "get-policy", "--policy-name", $PolicyName, "--output", "json") -AllowFailure
    if ($describe.ExitCode -eq 0) {
        return
    }

    $policyDoc = @'
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:*",
      "Resource": "*"
    }
  ]
}
'@

    $policyDocFile = [System.IO.Path]::GetTempFileName()
    try {
        $policyDoc | Out-File -Encoding ascii -FilePath $policyDocFile
        $policyDocUri = "file://$($policyDocFile -replace '\\','/')"
        Write-Host "AWS policy '$PolicyName' not found. Creating it with default policy document."
        Invoke-AwsCli -CommandArgs @("iot", "create-policy", "--policy-name", $PolicyName, "--policy-document", $policyDocUri, "--output", "json") | Out-Null
    }
    finally {
        if (Test-Path -LiteralPath $policyDocFile) { Remove-Item -LiteralPath $policyDocFile -Force -ErrorAction SilentlyContinue }
    }
}

function Ensure-AttachPolicy {
    param(
        [string]$PolicyName,
        [string]$TargetArn
    )

    Ensure-AwsPolicyExists -PolicyName $PolicyName

    $result = Invoke-AwsCli -CommandArgs @("iot", "attach-policy", "--policy-name", $PolicyName, "--target", $TargetArn) -AllowFailure
    if ($result.ExitCode -eq 0) {
        return
    }

    $msg = "$($result.StdErr)`n$($result.StdOut)"
    if ($msg -match "already" -or $msg -match "ResourceAlreadyExistsException") {
        return
    }

    throw "Failed to attach policy '$PolicyName' to certificate.`n$msg"
}

function Request-AwsClientCertFromCsr {
    param(
        [string]$CsrPath,
        [string]$CertOutPath,
        [string]$ThingName,
        [string]$PolicyName
    )

    if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
        throw "AWS CLI not found. Install and configure AWS CLI first (aws configure)."
    }

    $csrUri = "file://$($CsrPath -replace '\\','/')"
    Write-Host "Creating AWS IoT certificate from CSR..."
    $createResult = Invoke-AwsCli -CommandArgs @(
        "iot", "create-certificate-from-csr",
        "--certificate-signing-request", $csrUri,
        "--set-as-active",
        "--certificate-pem-outfile", $CertOutPath,
        "--output", "json"
    )

    if ([string]::IsNullOrWhiteSpace($createResult.StdOut)) {
        throw "AWS returned empty response for create-certificate-from-csr."
    }
    $createJson = ($createResult.StdOut | ConvertFrom-Json)
    $certArn = $createJson.certificateArn
    if ([string]::IsNullOrWhiteSpace($certArn)) {
        throw "AWS did not return certificateArn from create-certificate-from-csr."
    }

    Ensure-AwsThing -ThingName $ThingName
    Ensure-AttachThingPrincipal -ThingName $ThingName -PrincipalArn $certArn
    Ensure-AttachPolicy -PolicyName $PolicyName -TargetArn $certArn
}

function Get-AwsIotEndpoint {
    if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
        throw "AWS CLI not found. Install and configure AWS CLI first (aws configure)."
    }

    $result = Invoke-AwsCli -CommandArgs @("iot", "describe-endpoint", "--endpoint-type", "iot:Data-ATS", "--query", "endpointAddress", "--output", "text")
    $endpoint = ""
    if ($null -ne $result.StdOut) {
        $endpoint = ([string]$result.StdOut).Trim()
    }
    if ([string]::IsNullOrWhiteSpace($endpoint)) {
        throw "Failed to get AWS IoT endpoint using AWS CLI. Empty endpoint returned."
    }

    return $endpoint
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
    Ensure-RootCaFile -Url $awsRootCaUrl -OutPath $awsRootCaPath
    Send-Command "pki import cert root_ca_cert"
    Send-FileContent $awsRootCaPath

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

    $tlsCertPath = Join-Path $scriptDir "$thingName.cert.pem"
    Request-AwsClientCertFromCsr -CsrPath $csrFile -CertOutPath $tlsCertPath -ThingName $thingName -PolicyName $awsPolicyName
    Write-Host "AWS client certificate generated and saved to $tlsCertPath"

    # Import client cert
    Send-Command "pki import cert tls_cert"
    Send-FileContent $tlsCertPath
    Write-Host "AWS client certificate imported from $tlsCertPath"

    # MQTT config
    $awsEndpoint = Get-AwsIotEndpoint
    Write-Host "Using AWS IoT endpoint from AWS CLI: $awsEndpoint"
    Send-Command "conf set mqtt_endpoint $awsEndpoint"
    Send-Command "conf set mqtt_port $awsMqttPort"
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
