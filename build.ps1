param(
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$Fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=max_app,PSRAM=opi,USBMode=hwcdc,UploadMode=default,UploadSpeed=921600",
    [string]$BuildPath = "",
    [string]$ArduinoCli = "",
    [string]$ConfigFile = ""
)

$ErrorActionPreference = "Stop"

function Resolve-ArduinoCli {
    param([string]$UserValue)
    if ($UserValue -and (Test-Path $UserValue)) { return (Resolve-Path $UserValue).Path }
    if ($env:ARDUINO_CLI -and (Test-Path $env:ARDUINO_CLI)) { return (Resolve-Path $env:ARDUINO_CLI).Path }

    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        (Join-Path $PSScriptRoot "tools\\arduino-cli-bin\\arduino-cli.exe"),
        (Join-Path $PSScriptRoot "..\\tools\\arduino-cli-bin\\arduino-cli.exe"),
        (Join-Path $PSScriptRoot "..\\arduino-cli.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }

    throw "arduino-cli not found. Set -ArduinoCli or ARDUINO_CLI env var."
}

function Resolve-ConfigFile {
    param([string]$UserValue)
    if ($UserValue -and (Test-Path $UserValue)) { return (Resolve-Path $UserValue).Path }
    if ($env:ARDUINO_CLI_CONFIG -and (Test-Path $env:ARDUINO_CLI_CONFIG)) { return (Resolve-Path $env:ARDUINO_CLI_CONFIG).Path }

    $candidates = @(
        (Join-Path $PSScriptRoot "arduino-cli.yaml"),
        (Join-Path $PSScriptRoot "..\\arduino_cli_2_0_11\\arduino-cli.yaml")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return ""
}

$project = (Resolve-Path $ProjectRoot).Path
if (-not $BuildPath) {
    $BuildPath = Join-Path $project "build_out"
}
New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null

$cli = Resolve-ArduinoCli -UserValue $ArduinoCli
$cfg = Resolve-ConfigFile -UserValue $ConfigFile

$args = @()
if ($cfg) {
    $args += @("--config-file", $cfg)
}
$args += @(
    "compile",
    "--fqbn", $Fqbn,
    "--build-path", $BuildPath,
    $project
)

Write-Host "[build] cli: $cli"
if ($cfg) { Write-Host "[build] config: $cfg" }
Write-Host "[build] project: $project"
Write-Host "[build] fqbn: $Fqbn"
Write-Host "[build] out: $BuildPath"

& $cli @args
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "[build] done"

