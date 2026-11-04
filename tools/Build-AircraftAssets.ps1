# Imports the aircraft blockouts and their engine sounds in a hidden Workbench with tools/workbench
# (ACR_AircraftAssetPlugin): tools/blender/build_aircraft.py writes the FBXs, the palette and aircraft_job.txt;
# tools/audio/synth_jets.py writes the engine loops, the sound project templates and audio_job.txt. This writes the
# palette emat and the .acp files, sets up the texture, builds the models and registers the samples and sound
# projects (the prefabs' hand-written metas are checked in a game: pak:recon). No game client, no window. A new
# texture or sample is registered on the first run and its resource name lands in the emat / acp on the next, so the
# script runs Workbench twice when it has to.
#   powershell -File tools/Build-AircraftAssets.ps1 [-Root <addon root>]
param(
    [string]$Root = (Split-Path $PSScriptRoot -Parent),
    [string]$Project = (Join-Path $PSScriptRoot 'workbench\addon.gproj'),
    [int]$TimeoutSec = 300
)
$ErrorActionPreference = 'Stop'

function Find-Steam([string]$relative) {
    foreach ($drive in 'C', 'D', 'E', 'F', 'G') {
        foreach ($lib in "${drive}:\SteamLibrary\steamapps\common", "${drive}:\Program Files (x86)\Steam\steamapps\common") {
            $p = Join-Path $lib $relative
            if (Test-Path -LiteralPath $p) { return $p }
        }
    }
    throw "$relative was not found."
}

$wb = Find-Steam 'Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe'
$gameAddons = Join-Path (Split-Path (Find-Steam 'Arma Reforger\ArmaReforgerSteam.exe') -Parent) 'addons'
$wbProfile = Join-Path $Root '.local-workbench'
$jobFile = Join-Path $wbProfile 'profile\aircraft_job.txt'
if (!(Test-Path -LiteralPath $jobFile)) { throw "No $jobFile (run tools/blender/build_aircraft.py first)." }
$audioJob = Join-Path $wbProfile 'profile\audio_job.txt'
$runFile = Join-Path $wbProfile 'profile\aircraft_run.txt'

# ACP <path>|<template>: the template with each @WAV:<path>@ replaced by that sample's resource name (from its
# .wav.meta). Written only once every sample is registered; returns $false until then.
function Write-Acps {
    $complete = $true
    if (!(Test-Path -LiteralPath $audioJob)) { return $true }
    foreach ($line in Get-Content -LiteralPath $audioJob) {
        if (!$line.StartsWith('ACP ')) { continue }
        $f = $line.Substring(4).Split('|')
        $text = Get-Content -LiteralPath $f[1] -Raw
        foreach ($m in [regex]::Matches($text, '@WAV:([^@]+)@')) {
            $meta = $m.Groups[1].Value + '.meta'
            if (!(Test-Path -LiteralPath $meta)) { $complete = $false; $text = $null; break }
            $name = [regex]::Match((Get-Content -LiteralPath $meta -Raw), 'Name "([^"]+)"').Groups[1].Value
            $text = $text.Replace($m.Value, $name)
        }
        if ($text) { [IO.File]::WriteAllText($f[0], $text) }
    }
    return $complete
}

# The plugin's job: the models' job, the samples, and the sound projects already written.
function Write-Run {
    $lines = @(Get-Content -LiteralPath $jobFile)
    if (Test-Path -LiteralPath $audioJob) {
        foreach ($line in Get-Content -LiteralPath $audioJob) {
            if ($line.StartsWith('ACP ')) {
                $dst = $line.Substring(4).Split('|')[0]
                if (Test-Path -LiteralPath $dst) { $lines += "ACP $dst" }
            }
            elseif ($line) { $lines += $line }
        }
    }
    [IO.File]::WriteAllLines($runFile, [string[]]$lines)
}

# EMAT <path>|<base>|<key>=<value>...: a .png value becomes its texture's resource name from <name>.edds.meta.
# Returns $false while a texture is still unregistered.
function Write-Emats {
    $complete = $true
    foreach ($line in Get-Content -LiteralPath $jobFile) {
        if (!$line.StartsWith('EMAT ')) { continue }
        $f = $line.Substring(5).Split('|')
        $body = @()
        foreach ($kv in $f[2..($f.Count - 1)]) {
            $k, $v = $kv.Split('=', 2)
            if ($v.EndsWith('.png')) {
                $meta = $v.Substring(0, $v.Length - 4) + '.edds.meta'
                if (!(Test-Path -LiteralPath $meta)) { $complete = $false; continue }
                $v = '"' + ([regex]::Match((Get-Content -LiteralPath $meta -Raw), 'Name "([^"]+)"').Groups[1].Value) + '"'
            }
            $body += " $k $v"
        }
        $head = 'MatPBRBasic {'
        if ($f[1]) { $head = 'MatPBRBasic : "' + $f[1] + '" {' }
        [IO.File]::WriteAllText($f[0], ((@($head) + $body + @('}')) -join "`r`n") + "`r`n")
    }
    return $complete
}


$addonsDir = [IO.Path]::GetFullPath($gameAddons) + ',' + (Split-Path (Split-Path $Project -Parent) -Parent) + ',' + (Split-Path $Root -Parent)
$wbArgs = '-gproj "' + $Project + '" -addonsDir "' + $addonsDir + '" -profile "' + $wbProfile + '" -noThrow -noBackend -wbModule=ResourceManager -plugin=ACR_AircraftAssetPlugin -run'
$result = Join-Path $wbProfile 'profile\aircraft_result.txt'
for ($pass = 1; $pass -le 2; $pass++) {
    $ready = Write-Emats
    $ready = (Write-Acps) -and $ready
    Write-Run
    Remove-Item -LiteralPath $result -ErrorAction SilentlyContinue
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Workbench pass $pass (hidden)"
    $proc = Start-Process -FilePath $wb -ArgumentList $wbArgs -WindowStyle Hidden -PassThru
    if (!$proc.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue; Write-Host 'Timed out.' -ForegroundColor Red }
    if (Test-Path -LiteralPath $result) { Get-Content -LiteralPath $result } else { Write-Host 'No result file.' -ForegroundColor Red; exit 1 }
    if ($ready) { break }
    if (!(Write-Emats) -or !(Write-Acps)) { Write-Host 'A texture or sample is still not registered.' -ForegroundColor Red; exit 1 }
}
