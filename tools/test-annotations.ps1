$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$linuxRoot = & wsl -d Ubuntu-24.04 -u root --exec wslpath -a -u $workspace
if ($LASTEXITCODE -ne 0) { throw 'Cannot translate workspace path' }
$linuxRoot = ($linuxRoot | Out-String).Trim()
& wsl -d Ubuntu-24.04 -u root --cd $linuxRoot --exec /bin/bash tools/test-annotations.sh
if ($LASTEXITCODE -ne 0) { throw 'ARM Qt annotation smoke test failed' }
