param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$root=(Resolve-Path -LiteralPath $DeviceRoot).Path
if (!(Test-Path -LiteralPath (Join-Path $root '.adds/nm') -PathType Container)) {
    Write-Output 'NickelMenu is absent; both plugin settings default to On.'
    return
}
# Called only after stage-device's fresh backup. Existing NM config is untouched.
$files=@{
    'integration/nickelmenu/doubletap'='.adds/nm/doubletap'
    'integration/nickelmenu/toggle-japanese-longpress.sh'='.adds/doubletap/toggle-japanese-longpress.sh'
}
foreach ($source in $files.Keys) {
    $origin=Join-Path $workspace $source
    $target=Join-Path $root $files[$source]
    $pending=$target+'.pending'
    if (Test-Path -LiteralPath $pending) { throw 'Incomplete NickelMenu copy exists; nothing was overwritten.' }
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $origin -Destination $pending
    if ((Get-FileHash -LiteralPath $pending).Hash -ne (Get-FileHash -LiteralPath $origin).Hash) { throw 'NickelMenu copy checksum differs' }
    Move-Item -LiteralPath $pending -Destination $target -Force
}
Write-Output 'Verified long-press and rendering toggles installed in NickelMenu (Home and reader).'
