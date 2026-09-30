# List whether required ESP-IDF / OpenMRN-LCC build tools are on this Windows machine.
# Does not install anything. Does not flash hardware. Does not handle a Wi-Fi password.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1
#
# Writes local\check_tools-YYYYMMDD-HHMMSS-<host>.log (and check_tools-last.log)

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

if (-not $env:CHECK_TOOLS_INNER) {
    $localDir = Join-Path $Root "local"
    New-Item -ItemType Directory -Force -Path $localDir | Out-Null
    $ts = Get-Date -Format "yyyyMMdd-HHmmss"
    $hn = if ($env:COMPUTERNAME) { $env:COMPUTERNAME } else { "windows" }
    $hn = ($hn -replace "[^A-Za-z0-9._-]", "_")
    $log = Join-Path $localDir "check_tools-$ts-$hn.log"
    $last = Join-Path $localDir "check_tools-last.log"
    $header = @(
        "=== check_tools log (share this file with Grok; Grok CLI not required) ==="
        "file: $log"
        "time: $(Get-Date -Format o)"
        "host: $env:COMPUTERNAME"
        "os: $([Environment]::OSVersion.VersionString)"
        "ps: $($PSVersionTable.PSVersion)"
        "user: $env:USERNAME"
        "repo: $Root"
        "python: $((Get-Command python -ErrorAction SilentlyContinue).Source)"
        ""
    ) -join [Environment]::NewLine
    Set-Content -Path $log -Value $header -Encoding UTF8
    $env:CHECK_TOOLS_INNER = "1"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath @args *>&1 |
        Tee-Object -FilePath $log -Append
    $rc = $LASTEXITCODE
    if ($null -eq $rc) { $rc = 0 }
    Copy-Item -LiteralPath $log -Destination $last -Force
    Write-Host ""
    Write-Host "Share this file with Grok (no Grok CLI needed):"
    Write-Host "  $log"
    Write-Host "  $last"
    exit $rc
}

$script:MissingReq = 0
$script:MissingOpt = 0

function Write-Ok([string]$Name, [string]$Detail) {
    Write-Host ("  OK       {0,-22} {1}" -f $Name, $Detail)
}
function Write-Fail([string]$Name, [string]$Detail) {
    Write-Host ("  MISSING  {0,-22} {1}" -f $Name, $Detail)
    $script:MissingReq = 1
}
function Write-Warn([string]$Name, [string]$Detail) {
    Write-Host ("  WARN     {0,-22} {1}" -f $Name, $Detail)
    $script:MissingOpt = 1
}

function Find-Cmd([string[]]$Names) {
    foreach ($n in $Names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

Write-Host "Required tools check (Windows / ESP-IDF)  repo: $Root"
Write-Host "Target: ESP32-S3 (Waveshare 4.3 Inch LCC Control Panel)  (QEMU machine: esp32s3; Linux CI only)"
Write-Host ""
Write-Host "=== Host build tools (required) ==="

$git = Find-Cmd @("git")
if ($git) { Write-Ok "git" (& git --version 2>$null | Select-Object -First 1) }
else { Write-Fail "git" "Git for Windows" }

$py = Find-Cmd @("python", "python3")
if ($py) {
    $ver = & $py --version 2>&1 | Select-Object -First 1
    Write-Ok "python" "$ver ($py)"
} else {
    Write-Fail "python" "Python 3 from python.org or Microsoft Store"
}

$cmake = Find-Cmd @("cmake")
if ($cmake) { Write-Ok "cmake" (& cmake --version 2>$null | Select-Object -First 1) }
else { Write-Fail "cmake" "https://cmake.org/download/ (add to PATH)" }

$ninja = Find-Cmd @("ninja")
$make = Find-Cmd @("make", "mingw32-make")
if ($ninja) { Write-Ok "ninja" (& ninja --version 2>$null) }
elseif ($make) { Write-Ok "make" $make }
else { Write-Fail "generator" "Ninja (preferred) or make on PATH" }

Write-Host ""
Write-Host "=== ESP-IDF toolchain (required to build firmware) ==="

$idf = Find-Cmd @("idf.py")
if ($idf) {
    Write-Ok "idf.py" $idf
} elseif ($env:IDF_PATH -and (Test-Path (Join-Path $env:IDF_PATH "tools\idf.py"))) {
    Write-Ok "idf.py" "IDF_PATH=$env:IDF_PATH (run ESP-IDF export.ps1 / export.bat)"
} else {
    Write-Fail "idf.py" "ESP-IDF not on PATH. Install ESP-IDF v5.1.6 preferred (match GrokBot-CI-Espressif / utils/build_idf5.sh) and run export.ps1"
}

if ($env:IDF_PATH) { Write-Ok "IDF_PATH" $env:IDF_PATH }
else { Write-Warn "IDF_PATH" "not set; run ESP-IDF export.ps1 after install" }

Write-Host ""
Write-Host "=== Repo helpers (expected in this tree) ==="

if (Test-Path (Join-Path $Root "utils\build_idf5.sh")) {
    Write-Ok "build_idf5.sh" "utils/build_idf5.sh (Git Bash / WSL)"
} else {
    Write-Warn "build_idf5.sh" "utils/build_idf5.sh not found"
}

if (Test-Path (Join-Path $Root "utils\run_host_tests.sh")) {
    Write-Ok "run_host_tests.sh" "utils/run_host_tests.sh (Git Bash / WSL)"
} else {
    Write-Warn "run_host_tests.sh" "utils/run_host_tests.sh not found"
}

Write-Host ""
Write-Host "=== Optional host quality (WARN only) ==="

$clang = Find-Cmd @("clang")
if ($clang) { Write-Ok "clang" $clang }
else { Write-Warn "clang" "LLVM Clang optional for host tidy/coverage" }

Write-Host ""
Write-Host "=== Emulator (Espressif QEMU CI smoke) ==="

# qemu-system-xtensa is Linux CI only (GrokBot-CI-Espressif). Not on a typical Win PATH.
$qemu = Find-Cmd @("qemu-system-xtensa")
if ($qemu) {
    Write-Ok "qemu-system-xtensa" "$qemu  (-machine esp32s3; unusual on Windows)"
} else {
    Write-Host "  SKIP     qemu-system-xtensa   Linux CI only (GrokBot-CI-Espressif); not expected on Windows PATH"
    Write-Host "           Espressif QEMU smoke uses -machine esp32s3; not required to build/flash."
}

Write-Host ""
if ($script:MissingReq -ne 0) {
    Write-Host "Required tools are missing. See docs/REQUIRED_TOOLS.txt"
    exit 1
}
if ($script:MissingOpt -ne 0) {
    Write-Host "Host/IDF tools OK. Optional items listed as WARN above."
    Write-Host "Details: docs/REQUIRED_TOOLS.txt"
    exit 0
}
Write-Host "All required and optional tools found."
Write-Host "Details: docs/REQUIRED_TOOLS.txt"
exit 0
