param([string]$Distribution = 'Ubuntu-24.04')
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$linuxRoot = & wsl -d $Distribution -u root --exec wslpath -a -u $workspace
if ($LASTEXITCODE -ne 0) { throw 'Cannot translate workspace path in WSL' }
$linuxRoot = ($linuxRoot | Out-String).Trim()
$buildPath = 'PATH=/opt/kobo-double-tap/nickeltc/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
& wsl -d $Distribution -u root --cd $linuxRoot --exec /usr/bin/env $buildPath make all
if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
& wsl -d $Distribution -u root --cd $linuxRoot --exec /usr/bin/env $buildPath make koboroot
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
$previousPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = Join-Path $workspace '.tools/python'
    & python (Join-Path $PSScriptRoot 'verify_package.py') (Join-Path $workspace 'KoboRoot.tgz') (Join-Path $workspace 'src/libkobodoubletapprobe.so') --output (Join-Path $workspace 'build/package-verification.json')
    if ($LASTEXITCODE -ne 0) { throw 'Package verification failed' }
} finally {
    $env:PYTHONPATH = $previousPythonPath
}
