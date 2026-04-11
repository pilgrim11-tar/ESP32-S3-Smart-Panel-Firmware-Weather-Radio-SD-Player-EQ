param(
    [string]$OutRoot = "C:\Users\Taras\Documents\mou\sd_vendor_pack",
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$targets = @(
    @{
        Folder = "alarm1"
        FileName = "gentle_bell.wav"
        Url = "https://opengameart.org/sites/default/files/bell_ding2.wav"
        Source = "https://opengameart.org/content/bell-dingschimes"
        License = "CC0"
        Profile = "1 gentle"
    },
    @{
        Folder = "alarm2"
        FileName = "medium_alarm.wav"
        Url = "https://opengameart.org/sites/default/files/alarm_2.wav"
        Source = "https://opengameart.org/content/alarm-sound-effect"
        License = "CC0"
        Profile = "2 medium"
    },
    @{
        Folder = "alarm3"
        FileName = "serious_alarm.wav"
        Url = "https://opengameart.org/sites/default/files/alarm_0.wav"
        Source = "https://opengameart.org/content/alarm-2"
        License = "CC0"
        Profile = "3 serious"
    }
)

New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutRoot "alarm") | Out-Null

foreach ($item in $targets) {
    $folderPath = Join-Path $OutRoot $item.Folder
    New-Item -ItemType Directory -Force -Path $folderPath | Out-Null
    $targetPath = Join-Path $folderPath $item.FileName
    if (-not (Test-Path $targetPath) -or $Force) {
        Invoke-WebRequest -Uri $item.Url -OutFile $targetPath -MaximumRedirection 5 -TimeoutSec 60
    }
}

# Fallback folder /alarm: copy strongest tone for legacy fallback path.
$fallbackSrc = Join-Path (Join-Path $OutRoot "alarm3") "serious_alarm.wav"
$fallbackDst = Join-Path (Join-Path $OutRoot "alarm") "fallback_alarm.wav"
if (Test-Path $fallbackSrc) {
    Copy-Item -Path $fallbackSrc -Destination $fallbackDst -Force
}

$reportPath = Join-Path $OutRoot "ALARM_MELODIES_SOURCES.txt"
$reportLines = @(
    "Alarm melodies package",
    "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "",
    "Mapping used by firmware:",
    "/alarm1 -> gentle",
    "/alarm2 -> medium",
    "/alarm3 -> serious",
    "/alarm  -> fallback",
    "",
    "Sources:"
)
foreach ($item in $targets) {
    $reportLines += "- {0}: {1} ({2})" -f $item.Profile, $item.Source, $item.License
}
$reportLines | Set-Content -Path $reportPath -Encoding UTF8

foreach ($item in $targets) {
    $filePath = Join-Path (Join-Path $OutRoot $item.Folder) $item.FileName
    $size = (Get-Item $filePath).Length
    Write-Output ("{0}/{1} [{2} bytes]" -f $item.Folder, $item.FileName, $size)
}
if (Test-Path $fallbackDst) {
    Write-Output ("alarm/{0}" -f (Split-Path -Leaf $fallbackDst))
}
Write-Output ("sources: {0}" -f $reportPath)
