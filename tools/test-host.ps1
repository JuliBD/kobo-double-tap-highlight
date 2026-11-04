param(
    [string]$Zig = (Join-Path $PSScriptRoot '../.tools/zig-windows-x86_64-0.13.0/zig.exe')
)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $previousGlobal = $env:ZIG_GLOBAL_CACHE_DIR
    $previousLocal = $env:ZIG_LOCAL_CACHE_DIR
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $PWD '.tools/zig-local'
    & $Zig cc -std=gnu11 -Wall -Wextra -Werror -Ivendor/NickelHook tests/probe_test.c src/double_tap.c -o build/probe_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed' }
    & ./build/probe_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Probe tests failed' }
    & $Zig cc -std=c11 -Wall -Wextra -Werror tests/report_test.c src/report.c -o build/report_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Report compilation failed' }
    & ./build/report_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Report tests failed' }
    & $Zig cc -std=c11 -Wall -Wextra -Werror tests/double_tap_test.c src/double_tap.c -o build/double_tap_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Recognizer compilation failed' }
    & ./build/double_tap_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Recognizer tests failed' }
    & $Zig cc -std=c11 -Wall -Wextra -Werror tests/replay_test.c src/double_tap.c -o build/replay_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Device replay compilation failed' }
    & ./build/replay_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Device replay tests failed' }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $previousGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $previousLocal
    Pop-Location
}
