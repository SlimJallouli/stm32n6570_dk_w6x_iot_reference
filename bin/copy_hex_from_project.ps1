# Copies application and FSBL .bin artifacts into .\bin.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$binDir = $scriptDir

$copiedCount = 0

$filesToCopy = @(
    @{
        Source = Join-Path $repoRoot "Appli\HW_Crypto\stm32n6570_dk_w6x_iot_reference_Appli.bin"
        Target = Join-Path $binDir "Appli\HW_Crypto\stm32n6570_dk_w6x_iot_reference_Appli.bin"
    }
    @{
        Source = Join-Path $repoRoot "FSBL\Release\stm32n6570_dk_w6x_iot_reference_FSBL.bin"
        Target = Join-Path $binDir "FSBL\Release\stm32n6570_dk_w6x_iot_reference_FSBL.bin"
    }
)

foreach ($item in $filesToCopy) {
    $sourcePath = $item.Source
    $targetPath = $item.Target

    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        Write-Error "Source file not found: $sourcePath"
        exit 1
    }

    $targetDir = Split-Path -Parent $targetPath
    if (-not (Test-Path -LiteralPath $targetDir -PathType Container)) {
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    }

    Copy-Item -LiteralPath $sourcePath -Destination $targetPath -Force
    $copiedCount++
    Write-Host "Copied: $sourcePath -> $targetPath"
}

Write-Host "Done. Copied $copiedCount .bin file(s) into: $binDir"
