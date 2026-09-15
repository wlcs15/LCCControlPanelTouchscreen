# Host tests (same as utils/run_host_tests.sh). No board. No house PSK.
#   powershell -NoProfile -ExecutionPolicy Bypass -File utils\run_host_tests.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

function Get-Clangxx {
    $c = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $p = Join-Path ${env:ProgramFiles} "LLVM\bin\clang++.exe"
    if (Test-Path $p) { return $p }
    throw "clang++ not on PATH (install LLVM and add bin to PATH)"
}

$clang = Get-Clangxx
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("lcc_host_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    Write-Host "== wrap tests =="
    & (Join-Path $PSScriptRoot "test_wifi_wrap.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "== CDI Configure =="
    $cdi = Join-Path $tmp "test_s3_cdi_configure.exe"
    & $clang -std=c++11 -D_CRT_SECURE_NO_WARNINGS "-I$Root\tests" "$Root\tests\test_cdi_configure.cpp" -o $cdi
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cdi
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "== SvcReachPick =="
    $svc = Join-Path $tmp "test_s3_svc_reach.exe"
    & $clang -std=c++11 -D_CRT_SECURE_NO_WARNINGS "-I$Root\main\wifi" "$Root\tests\test_svc_reach.cpp" -o $svc
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $svc
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "host tests OK"
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
