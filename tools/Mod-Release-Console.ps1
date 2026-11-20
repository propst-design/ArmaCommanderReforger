# Human entry point. Backend operations never publish anything.
[CmdletBinding()]
param([string]$WorkbenchPath, [string]$InstalledAddons = 'E:\ArmaReforgerMods\addons', [switch]$NoMenu, [string]$Publish, [string]$PreviewImage, [string]$Screenshots)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$backend = Join-Path $PSScriptRoot 'release_console.py'
$consoleState = Join-Path $root '.release-console'
New-Item -ItemType Directory -Path $consoleState -Force | Out-Null
try { Start-Transcript -Path (Join-Path $consoleState 'console.log') -Append -Force | Out-Null } catch { Write-Warning 'Console log could not be started.' }
function Find-BaseGameAddons {
 $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
 $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
 if ($steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf'))) {
  foreach ($match in [regex]::Matches((Get-Content -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf') -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
 }
 foreach ($library in $libraries) {
  if (!$library) { continue }
  $candidate = Join-Path $library 'steamapps/common/Arma Reforger/addons'
  $baseProject = Join-Path $candidate 'data/ArmaReforger.gproj'
  if ((Test-Path -LiteralPath $baseProject) -and (Get-Content -LiteralPath $baseProject -Raw) -match 'GUID\s+"58D0FB3206B6F859"') { return $candidate }
 }
 throw 'Arma Reforger base-game addon files were not found in the Steam libraries.'
}
function Invoke-Guard([string[]]$Arguments) {
 $result = & python $backend @Arguments
 if ($LASTEXITCODE -ne 0) { throw 'Release blocked. The reason is printed above; ask your coding agent to resolve it.' }
 return $result
}
# Discord changelog post (Jacob, 7 Nov): the Workshop changelog relay (.github/workflows/workshop-relay.yml)
# is scheduled every 15 min, but GitHub started it only every 4-5 h, so 0.5.10 and 0.5.11 reached Discord
# late or by hand. Start it now, and again 10 and 30 minutes later in case reforgermods.net lists the new
# version late. The relay records what it has posted, so the extra runs never post twice.
function Start-ChangelogRelay([string]$Mod) {
 if ($Mod -notin @('ACA', 'BBB', 'AIO')) { return }
 $manual = 'Start "Workshop changelog relay" in the ACA repository''s GitHub Actions to post the changelog to Discord.'
 if (!(Get-Command gh -ErrorAction SilentlyContinue)) { Write-Warning ('GitHub CLI not found. ' + $manual); return }
 $run = 'gh workflow run workshop-relay.yml -R propst-design/ArmaCommanderReforger -f mode=poll'
 $ErrorActionPreference = 'Continue'
 & cmd.exe /c ($run + ' 2>&1') | Out-Host
 if ($LASTEXITCODE -ne 0) { Write-Warning ('The Discord changelog relay did not start. ' + $manual); return }
 Start-Process -FilePath powershell.exe -WindowStyle Hidden -ArgumentList @('-NoProfile', '-Command', ('Start-Sleep 600; ' + $run + '; Start-Sleep 1200; ' + $run))
 Write-Host 'Discord changelog relay started; it runs again in 10 and 30 minutes.' -ForegroundColor Green
}
function Open-CheckedProject {
 # -Confirmed: Jacob asked an agent to publish this update (Publish-AddOn.ps1, or -Publish for ACA/BBB/AiO; 19 Nov).
 # -PreviewImage / -Screenshots (Jacob, 20 Nov; opt-in): an update also replaces the page's preview image
 # (jpg/png) or adds the screenshots in a folder (jpg/png/bmp). Left out, the page's pictures stay as they are.
 param([string]$Mod, [switch]$PublishUpdate, [switch]$Confirmed, [string]$PreviewImage, [string]$Screenshots)
 if (Get-Process '*Workbench*' -ErrorAction SilentlyContinue) { throw 'Close Workbench first. This ensures the selected release opens as the active project.' }
 $project = Invoke-Guard @('check', $Mod, '--installed-root', $InstalledAddons)
 $notes = (Invoke-Guard @('notes', $Mod)) | ConvertFrom-Json
 if ($PublishUpdate) { $notes = (Invoke-Guard @('publish-info', $Mod, '--installed-root', $InstalledAddons)) | ConvertFrom-Json }
 if (!$script:WorkbenchPath) {
  $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
  if ($steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf'))) {
   foreach ($match in [regex]::Matches((Get-Content -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf') -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
  }
  foreach ($library in $libraries) {
   if (!$library) { continue }
   foreach ($relative in @('Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe', 'Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteam.exe', 'Arma Reforger Tools/ArmaReforgerWorkbenchSteam.exe')) {
    $candidate = Join-Path $library ('steamapps/common/' + $relative)
    if (Test-Path -LiteralPath $candidate) { $script:WorkbenchPath = $candidate; break }
   }
   if ($script:WorkbenchPath) { break }
  }
 }
 if (!$script:WorkbenchPath -or !(Test-Path -LiteralPath $script:WorkbenchPath)) { throw 'Workbench executable not found. Ask the coding agent to configure -WorkbenchPath.' }
 $addonDirs = Split-Path (Split-Path $project -Parent) -Parent
 if ($Mod -eq 'ACA') {
  $bbbProject = Invoke-Guard @('check', 'BBB')
  $addonDirs += ',' + (Split-Path (Split-Path $bbbProject -Parent) -Parent)
 }
 # ACA add-ons (maps, content packs) open beside the live Workshop ACA/BBB and their own dependencies.
 if ($Mod -match '^(MAP|PACK)-') { $addonDirs += ',' + (Invoke-Guard @('addon-dirs', '--installed-root', $InstalledAddons)) }
 # Tools supply core; the game supplies data. Neither may depend on the shortcut working folder.
 $addonDirs += ',' + (Find-BaseGameAddons)
 $launchArgs = '-gproj "' + $project + '" -addonsDir "' + $addonDirs + '"'
 if (!$PublishUpdate) {
  Set-Clipboard -Value (Get-Content -LiteralPath $notes.changelogFile -Raw -Encoding UTF8)
  Write-Host "Opening checked $Mod $($notes.version): $project" -ForegroundColor Green
  Write-Host 'Its changelog is copied. In Publish Project, click Change notes and press Ctrl+V.'
  Write-Host "Use version $($notes.version); review page settings before uploading."
  Write-Host 'After confirmed publication, use option 8 to record it, then download and use option 6.'
  Start-Process -FilePath $script:WorkbenchPath -WorkingDirectory (Split-Path $script:WorkbenchPath -Parent) -ArgumentList $launchArgs
  return
 }
 Write-Host "WORKSHOP UPDATE: $Mod $($notes.installedVersion) -> $($notes.version)" -ForegroundColor Cyan
 Get-Content -LiteralPath $notes.changelogFile -Encoding UTF8 | Out-Host
 if ($PreviewImage) {
  if (!(Test-Path -LiteralPath $PreviewImage -PathType Leaf) -or $PreviewImage -notmatch '\.(png|jpe?g)$') { throw "Preview image not found or not a png/jpg: $PreviewImage" }
  $PreviewImage = (Resolve-Path -LiteralPath $PreviewImage).Path
  Write-Host "Preview image: $PreviewImage"
 }
 if ($Screenshots) {
  if (!(Test-Path -LiteralPath $Screenshots -PathType Container)) { throw "Screenshots folder not found: $Screenshots" }
  $Screenshots = (Resolve-Path -LiteralPath $Screenshots).Path
  $shots = @(Get-ChildItem -LiteralPath $Screenshots -File | Where-Object { $_.Extension -match '^\.(png|jpe?g|bmp)$' })
  if ($shots.Count -eq 0) { throw "No jpg, png or bmp in $Screenshots" }
  Write-Host "Screenshots: $($shots.Count) from $Screenshots"
 }
 Write-Host 'This uploads an update using the existing Workshop item and your Workbench login.'
 $confirmation = 'PUBLISH ' + $Mod + ' ' + $notes.version
 if (!$Confirmed -and (Read-Host "Type $confirmation to upload, or press Enter to cancel") -cne $confirmation) {
  Write-Host 'Cancelled; nothing uploaded.'
  return
 }
 # Recheck after review so edits/version changes cannot slip between review and upload.
 $fresh = (Invoke-Guard @('publish-info', $Mod, '--installed-root', $InstalledAddons)) | ConvertFrom-Json
 if ($fresh.project -ne $project -or $fresh.version -ne $notes.version -or $fresh.changelogFile -ne $notes.changelogFile) { throw 'The selected release changed during review. Start again.' }
 $launchArgs += ' -wbmodule=ResourceManager -publishAddon -publishAddonVersion ' + $notes.version + ' -publishAddonChangeNoteFile "' + $notes.changelogFile + '"'
 if ($PreviewImage) { $launchArgs += ' -publishAddonPreviewImage "' + $PreviewImage + '"' }
 if ($Screenshots) { $launchArgs += ' -publishAddonScreenshots "' + $Screenshots + '"' }
 $started = Get-Date
 Write-Host 'Publishing checked update with its changelog file...'
 $process = Start-Process -FilePath $script:WorkbenchPath -WorkingDirectory (Split-Path $script:WorkbenchPath -Parent) -ArgumentList $launchArgs -WindowStyle Hidden -Wait -PassThru
 if ($process.ExitCode -ne 0) { throw "Workbench update exited with code $($process.ExitCode). Check its log before retrying." }
 $logsRoot = Join-Path (Split-Path (Split-Path $root -Parent) -Parent) 'logs'
 $log = Get-ChildItem -LiteralPath $logsRoot -Filter console.log -Recurse | Where-Object { $_.LastWriteTime -ge $started } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
 if (!$log) { throw 'Upload result could not be confirmed. Check Workbench before retrying; an upload may have occurred.' }
 $body = Get-Content -LiteralPath $log.FullName -Raw
 if (!$body.Replace('\','/').Contains($project.Replace('\','/')) -or $body -notmatch 'Project uploaded successfully' -or $body -notmatch 'Addon processing successful') { throw 'Upload/processing result could not be confirmed. Check Workbench before retrying; an upload may have occurred.' }
 @{ mod=$Mod; version=$notes.version; publication='CLI upload confirmed in Workbench log'; log=$log.FullName; installedVerification='pending' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $consoleState ($Mod + '-publication.json')) -Encoding UTF8
 $relativeLog = 'logs/' + $log.FullName.Substring($logsRoot.Length).TrimStart('\').Replace('\','/')
 Invoke-Guard @('record-publication', $Mod, '--version', $notes.version, '--candidate', (Split-Path (Split-Path $project -Parent) -Parent), '--evidence', ('Workbench upload and processing success: ' + $relativeLog)) | Out-Null
 Write-Host "Uploaded $Mod $($notes.version). Download its Workshop update, then use option 6." -ForegroundColor Green
 Start-ChangelogRelay $Mod
}
$mapMods = @('MAP-CHERNARUS','MAP-GULFCOAST','MAP-KUNAR','MAP-WESTZAGORIA','MAP-ZIMNITRITA','PACK-COLDWAR','PACK-MODERN','PACK-ZOMBIES','PACK-CIVPOP')
$allMods = @('ACA','BBB','AIO') + $mapMods
function Read-MapMod {
 for ($i = 0; $i -lt $mapMods.Count; $i++) { Write-Host ("  {0}  {1}" -f ($i + 1), $mapMods[$i]) }
 $pick = Read-Host 'Which ACA add-on (number)'
 $index = 0
 if (![int]::TryParse($pick, [ref]$index) -or $index -lt 1 -or $index -gt $mapMods.Count) { throw 'Choose one of the listed numbers.' }
 return $mapMods[$index - 1]
}
# Agent publish (Jacob, 19 Nov: the typed rule, "I want it gone"): when Jacob asks an agent to publish,
# -Publish <ACA|BBB|AIO|MAP-...|PACK-...> uploads the checked update with its changelog and no typed
# confirmation. Every other check still runs. BBB goes before ACA.
if ($Publish) { Open-CheckedProject -Mod $Publish.ToUpperInvariant() -PublishUpdate -Confirmed -PreviewImage $PreviewImage -Screenshots $Screenshots; return }
if ($NoMenu) { return }
while ($true) {
 Write-Host ''
 Write-Host 'MOD RELEASE CONSOLE' -ForegroundColor Cyan
 Write-Host 'Development stays in addons. This console selects checked release copies for you.'
 & python $backend status
 Write-Host ''
 Write-Host '1  Prepare ACA + BBB update (fresh copies + isolated startup checks)'
 Write-Host '2  Prepare AiO update (fresh copy + isolated startup check)'
 Write-Host '3  Open checked BBB release for Workshop upload -- first for an ACA/BBB update'
 Write-Host '4  Open checked ACA release -- requires downloaded BBB to match'
 Write-Host '5  Open checked AiO release for Workshop upload'
 Write-Host '6  Verify a downloaded Workshop update'
 Write-Host '7  Publish a checked update with automatic version/changelog (review first)'
 Write-Host '8  Record a confirmed manual Workshop publication and start the next version'
 Write-Host '9  Prepare an ACA add-on: map or Cold War pack (checked against the downloaded ACA, BBB and its mods)'
 Write-Host '10 Open a checked ACA add-on (map or pack) for Workshop upload'
 Write-Host '0  Exit'
 $choice = Read-Host 'Choose'
 try {
  switch ($choice) {
   '0' { return }
   '1' { Invoke-Guard @('prepare', 'ACA') | Out-Host; Write-Host 'Prepared. Next: option 3, upload BBB, then download its Workshop update.' -ForegroundColor Green }
   '2' { Invoke-Guard @('prepare', 'AIO') | Out-Host; Write-Host 'Prepared. Next: option 5.' -ForegroundColor Green }
   '3' { Open-CheckedProject 'BBB' }
   '4' { Open-CheckedProject 'ACA' }
   '5' { Open-CheckedProject 'AIO' }
   '6' {
    $mod = (Read-Host 'Which downloaded mod: ACA, BBB, AIO or a MAP-/PACK- name').ToUpperInvariant()
    if ($mod -notin $allMods) { throw ('Choose one of: ' + ($allMods -join ', ')) }
    Invoke-Guard @('installed', $mod, '--installed-root', $InstalledAddons) | Out-Host
   }
   '7' {
    $mod = (Read-Host 'Which mod to update: ACA, BBB, AIO or a MAP-/PACK- name').ToUpperInvariant()
    if ($mod -notin $allMods) { throw ('Choose one of: ' + ($allMods -join ', ')) }
    Open-CheckedProject -Mod $mod -PublishUpdate
   }
   '8' {
    $mod = (Read-Host 'Published mod: ACA, BBB, AIO or a MAP-/PACK- name').ToUpperInvariant()
    if ($mod -notin $allMods) { throw ('Choose one of: ' + ($allMods -join ', ')) }
    $version = Read-Host 'Confirmed published version'
    $evidence = Read-Host 'Publication confirmation (for example, Workshop page or owner confirmation)'
    Invoke-Guard @('record-publication', $mod, '--version', $version, '--evidence', $evidence) | Out-Host
    Write-Host 'Publication recorded in source; next version opened. Ask the coding agent to review and commit the records.'
    Start-ChangelogRelay $mod
   }
   '9' { $mod = Read-MapMod; Invoke-Guard @('prepare', $mod, '--installed-root', $InstalledAddons) | Out-Host; Write-Host 'Prepared. Next: option 10.' -ForegroundColor Green }
   '10' { Open-CheckedProject (Read-MapMod) }
   default { Write-Host 'Choose one of the listed numbers.' }
  }
 } catch {
  Write-Host $_.Exception.Message -ForegroundColor Red
  Write-Host ('Error saved in ' + (Join-Path $consoleState 'console.log'))
  Read-Host 'Press Enter to return to the menu' | Out-Null
 }
}
