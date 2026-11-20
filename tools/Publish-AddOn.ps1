# Publish an update of an ACA add-on (map add-on or content pack) from the command line.
# Jacob, 6 Nov: agents may run this whenever he asks for an add-on update; ACA, BBB and AiO
# still go through the console with him. The first version of an add-on is published by hand
# in Workbench (console option 10); when he says it is up, an agent records it:
# release_console.py record-publication <MOD> --version 0.1.0 --evidence <Workshop page>.
#
# 1. Changelog: bullets already under the next version are kept; otherwise they are written
#    from the add-on's commit subjects since the live release, and committed.
# 2. Prepare: a fresh copy, startup-checked on a hidden server against the ACA and BBB copies
#    that are live on the Workshop (not the development source), so an add-on that needs an
#    unreleased ACA fails here instead of on players' machines.
# 3. Upload with Workbench's command line (version, changelog), confirmed from its log, then
#    recorded in WORKSHOP-RELEASES.json and the add-on's CHANGELOG (committed, not pushed).
# The Workshop page (description, pictures) is left as it is, unless -WithPreview (the add-on's picture in
# tools/workshop/addons), -PreviewImage <file> or -Screenshots <folder> is given (Jacob, 20 Nov: opt-in).
# -DryRun stops before the upload.
# Add-ons: the MAP-/PACK- entries in release_console.py (PACK-COLDWAR, PACK-MODERN, ...). One with no
# release recorded yet (never uploaded, no Workshop item) stops at the first step with what to do.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^(MAP|PACK)-[A-Z]+$')][string]$Mod,
 [switch]$DryRun,
 [switch]$WithPreview,
 [string]$PreviewImage,
 [string]$Screenshots,
 [string]$InstalledAddons = 'E:\ArmaReforgerMods\addons'
)
$ErrorActionPreference = 'Stop'
$backend = Join-Path $PSScriptRoot 'release_console.py'
function Invoke-Backend([string[]]$Arguments) {
 $out = & python $backend @Arguments
 if ($LASTEXITCODE -ne 0) { throw ('Blocked at ' + $Arguments[0] + '. The reason is printed above.') }
 return $out
}
# A first version (no Workshop item yet) is Jacob's Workbench upload, never this script's.
Invoke-Backend @('addon-released', $Mod) | Out-Null
# Each add-on's Workshop picture (1920x1080), beside the others in tools/workshop/addons.
$previews = @{ 'MAP-CHERNARUS'='aca-chernarus-minus.png'; 'MAP-GULFCOAST'='aca-gulfcoast-islands.png'; 'MAP-KUNAR'='aca-kunar-province.png';
 'MAP-WESTZAGORIA'='aca-west-zagoria.png'; 'MAP-ZIMNITRITA'='aca-zimnitrita.png'; 'PACK-COLDWAR'='aca-coldwar.png';
 'PACK-MODERN'='aca-modern.png'; 'PACK-ZOMBIES'='aca-zombies.png'; 'PACK-CIVPOP'='aca-civpop.png' }
if ($WithPreview -and !$PreviewImage) {
 if (!$previews.ContainsKey($Mod)) { throw "No picture listed for $Mod in tools/workshop/addons; pass -PreviewImage <file>." }
 $PreviewImage = Join-Path (Join-Path $PSScriptRoot 'workshop\addons') $previews[$Mod]
}
if ($PreviewImage -and !(Test-Path -LiteralPath $PreviewImage -PathType Leaf)) { throw "Preview image not found: $PreviewImage" }
if ($Screenshots -and !(Test-Path -LiteralPath $Screenshots -PathType Container)) { throw "Screenshots folder not found: $Screenshots" }
if (Get-Process '*Workbench*' -ErrorAction SilentlyContinue) { throw 'Workbench is open. Close it first: the upload runs its own hidden Workbench.' }

$notes = (Invoke-Backend @('addon-notes', $Mod)) | ConvertFrom-Json
if ($notes.written) { Write-Host "$Mod $($notes.version): changelog written from its commits." }
else { Write-Host "$Mod $($notes.version): using the changelog already written." }

Write-Host 'Checking against the live ACA and BBB (hidden server, about a minute)...'
Invoke-Backend @('prepare', $Mod, '--installed-root', $InstalledAddons) | Out-Null
$info = (Invoke-Backend @('publish-info', $Mod, '--installed-root', $InstalledAddons)) | ConvertFrom-Json
Write-Host "WORKSHOP UPDATE: $Mod $($info.installedVersion) -> $($info.version)" -ForegroundColor Cyan
Get-Content -LiteralPath $info.changelogFile -Encoding UTF8 | Out-Host
if ($PreviewImage) { Write-Host "Preview image: $PreviewImage" }
if ($Screenshots) { Write-Host "Screenshots from: $Screenshots" }
if ($DryRun) {
 Write-Host 'Dry run: checked and ready, nothing uploaded.' -ForegroundColor Yellow
 return
}

. (Join-Path $PSScriptRoot 'Mod-Release-Console.ps1') -NoMenu -InstalledAddons $InstalledAddons
Open-CheckedProject -Mod $Mod -PublishUpdate -Confirmed -PreviewImage $PreviewImage -Screenshots $Screenshots
& git -C $notes.source add CHANGELOG.md
& git -C $notes.source commit -q -m "Record $($info.version) as released on the Workshop" -m 'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>'
if ($LASTEXITCODE -ne 0) { throw 'Uploaded and recorded, but the CHANGELOG commit failed: commit it by hand.' }
Write-Host "Published $Mod $($info.version)." -ForegroundColor Green
