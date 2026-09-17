# Host clang-tidy fail gate (same as utils/run_clang_tidy.py).
#   powershell -NoProfile -ExecutionPolicy Bypass -File utils\run_clang_tidy.ps1
# Does not scan LVGL / OpenMRN / panel UI.
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = $null
foreach ($name in @("python", "python3", "py")) {
    if (Get-Command $name -ErrorAction SilentlyContinue) {
        $py = $name
        break
    }
}
if (-not $py) { throw "python not found (python -u utils\run_clang_tidy.py)" }
if ($py -eq "py") {
    & $py -3 -u (Join-Path $Root "utils\run_clang_tidy.py") @args
} else {
    & $py -u (Join-Path $Root "utils\run_clang_tidy.py") @args
}
exit $LASTEXITCODE
