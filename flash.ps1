param(
    [string]$Port = "COM5",
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$Fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=max_app,PSRAM=opi,USBMode=hwcdc,UploadMode=default,UploadSpeed=921600",
    [string]$BuildPath = "",
    [string]$ArduinoCli = "",
    [string]$ConfigFile = ""
)

$ErrorActionPreference = "Stop"

if (-not $BuildPath) {
    $BuildPath = Join-Path (Resolve-Path $ProjectRoot).Path "build_out"
}

$buildScript = Join-Path $PSScriptRoot "build.ps1"
if (-not (Test-Path $buildScript)) {
    throw "build.ps1 not found: $buildScript"
}

& $buildScript `
    -ProjectRoot $ProjectRoot `
    -Fqbn $Fqbn `
    -BuildPath $BuildPath `
    -ArduinoCli $ArduinoCli `
    -ConfigFile $ConfigFile

function Resolve-ArduinoCli {
    param([string]$UserValue)
    if ($UserValue -and (Test-Path $UserValue)) { return (Resolve-Path $UserValue).Path }
    if ($env:ARDUINO_CLI -and (Test-Path $env:ARDUINO_CLI)) { return (Resolve-Path $env:ARDUINO_CLI).Path }
    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($c in @(
        (Join-Path $PSScriptRoot "tools\\arduino-cli-bin\\arduino-cli.exe"),
        (Join-Path $PSScriptRoot "..\\tools\\arduino-cli-bin\\arduino-cli.exe"),
        (Join-Path $PSScriptRoot "..\\arduino-cli.exe")
    )) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    throw "arduino-cli not found. Set -ArduinoCli or ARDUINO_CLI env var."
}

function Resolve-ConfigFile {
    param([string]$UserValue)
    if ($UserValue -and (Test-Path $UserValue)) { return (Resolve-Path $UserValue).Path }
    if ($env:ARDUINO_CLI_CONFIG -and (Test-Path $env:ARDUINO_CLI_CONFIG)) { return (Resolve-Path $env:ARDUINO_CLI_CONFIG).Path }
    foreach ($c in @(
        (Join-Path $PSScriptRoot "arduino-cli.yaml"),
        (Join-Path $PSScriptRoot "..\\arduino_cli_2_0_11\\arduino-cli.yaml")
    )) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return ""
}

$cli = Resolve-ArduinoCli -UserValue $ArduinoCli
$cfg = Resolve-ConfigFile -UserValue $ConfigFile

$args = @()
if ($cfg) {
    $args += @("--config-file", $cfg)
}
$args += @(
    "upload",
    "-p", $Port,
    "--fqbn", $Fqbn,
    "--input-dir", $BuildPath
)

Write-Host "[flash] cli: $cli"
if ($cfg) { Write-Host "[flash] config: $cfg" }
Write-Host "[flash] port: $Port"
Write-Host "[flash] fqbn: $Fqbn"
Write-Host "[flash] input: $BuildPath"

& $cli @args
if ($LASTEXITCODE -ne 0) {
    throw "Upload failed with exit code $LASTEXITCODE"
}

Write-Host "[flash] done"

