# Refresh the Workshop release copy of AC: Actual while stripping tests and dev files.
# Run from Windows PowerShell 5.1: powershell -File tools/make_release.ps1
[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$addonsRoot = Split-Path $sourceRoot -Parent
if (!$OutputDirectory) { $OutputDirectory = Join-Path (Split-Path $addonsRoot -Parent) '_ACR-release' }
$releaseParent = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $releaseParent 'ArmaCommanderReforger'))
if ($releaseParent -eq $addonsRoot -or $releaseParent.StartsWith($addonsRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Stage outside the Workbench addons directory to avoid duplicate resource GUIDs.'
}
for ($path = $releaseRoot; $path; $path = Split-Path $path -Parent) {
    if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing a release path containing a junction or symbolic link: $path"
    }
}
$sha = & git -C $sourceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source commit.' }
$dirty = & git -C $sourceRoot status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Commit or stash changes before building a Workshop candidate.' }
& python (Join-Path $PSScriptRoot 'check_mission_doc.py')
if ($LASTEXITCODE -ne 0) { throw 'Update MISSION-MAKING.md first.' }

$bbbCommit = & python (Join-Path $PSScriptRoot 'bbb_dependency.py')
if ($LASTEXITCODE -ne 0) { throw 'Resolve the pinned BBB dependency first.' }
$releaseFiles = @('resourceDatabase.rdb', 'LICENSE.md', 'README.md', 'MISSION-MAKING.md')
foreach ($name in @('Scripts', 'Configs', 'Missions', 'Prefabs', 'Worlds', 'addon.gproj') + $releaseFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $name))) { throw "Missing release source: $name" }
}

# Rebuild the entire candidate to remove stale files, but only our marked directory.
$marker = Join-Path $releaseParent 'WORKSHOP-BUILD.json'
if (Test-Path -LiteralPath $releaseRoot) {
    if (!(Test-Path -LiteralPath $marker)) { throw 'Existing candidate is unmarked; choose a fresh output directory.' }
    $previous = Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if ($previous.sourceRoot -ne $sourceRoot -or $previous.addonRoot -ne $releaseRoot) { throw 'Candidate marker belongs to another location.' }
    Get-ChildItem -LiteralPath $releaseRoot -Recurse -Force | ForEach-Object {
        if ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing linked candidate entry: $($_.FullName)" }
    }
    Remove-Item -LiteralPath $releaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null

$keepDirs = @('Configs', 'Missions', 'Prefabs', 'Worlds', 'Scripts', 'Assets', 'Sounds') # the console's SPECS['ACA'] dirs: 0.5.10's jets flew invisible and silent without Assets and Sounds
$skipFiles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$skipFiles.Add('Worlds/BBB_TestArland.ent') | Out-Null
$skipFiles.Add('Worlds/BBB_TestArland.ent.meta') | Out-Null
$skipFiles.Add('Missions/BBB_TestArland.conf') | Out-Null
$skipFiles.Add('Missions/BBB_TestArland.conf.meta') | Out-Null

function Is-TestScript($rel) {
    if ($rel.StartsWith('Scripts/Game/ACR_Tests/')) { return $true }
    if ($rel.StartsWith('Scripts/Game/BBB/') -and $rel -match 'Test\.c(\.meta)?$') { return $true }
    return $false
}

foreach ($d in $keepDirs) {
    $targetDir = Join-Path $releaseRoot $d
    if (Test-Path -LiteralPath $targetDir) {
        Remove-Item -LiteralPath $targetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    $srcDir = Join-Path $sourceRoot $d
    Get-ChildItem -LiteralPath $srcDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
        if (!$skipFiles.Contains($rel) -and !(Is-TestScript $rel)) {
            $destFile = Join-Path $releaseRoot $rel
            $destParent = Split-Path $destFile -Parent
            if (!(Test-Path -LiteralPath $destParent)) {
                New-Item -ItemType Directory -Path $destParent -Force | Out-Null
            }
            Copy-Item -LiteralPath $_.FullName -Destination $destFile -Force
        }
    }
}

foreach ($name in $releaseFiles) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination (Join-Path $releaseRoot $name) -Force
}

# Update addon.gproj TITLE for Workshop release
$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'addon.gproj') -Raw
if ($project -notmatch 'GUID\s+"AC230926A0010001"' -or ([regex]::Matches($project, '(?m)^\s*TITLE\s+".*"')).Count -ne 1) {
    throw 'Unexpected project identity or title.'
}
$project = $project -replace '(?m)^\s*TITLE\s+".*"', ' TITLE "Arma Commander: Actual"'
[IO.File]::WriteAllText((Join-Path $releaseRoot 'addon.gproj'), $project, [Text.UTF8Encoding]::new($false))

# Copy workshop description and thumbnails to _ACR-release
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'workshop') -File) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $releaseParent $file.Name) -Force
}

# One maintained page source; generated copy sits beside the candidate.
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/workshop-description.md') -Destination (Join-Path $releaseParent 'WORKSHOP-DESCRIPTION.txt') -Force
$files = @(Get-ChildItem -LiteralPath $releaseRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{ path = $_.FullName.Substring($releaseRoot.Length + 1).Replace('\', '/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
@{ sourceRoot = $sourceRoot; addonRoot = $releaseRoot; commit = $sha.Trim(); bbbCommit = $bbbCommit.Trim(); bbbModId = "BBB0261015000001"; builtUtc = [DateTime]::UtcNow.ToString('o'); files = $files } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $marker -Encoding UTF8
Write-Output "Workshop candidate: $releaseRoot"
Write-Output "Source: $sha"
Write-Output 'Page copy, preview images and SHA-256 manifest are beside the addon. Nothing uploaded.'
