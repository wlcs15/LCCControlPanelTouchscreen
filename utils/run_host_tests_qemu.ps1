# On-target Espressif QEMU firmware smoke (esp32s3 / LCC Control Panel).
# Same as utils/run_host_tests_qemu.py / .sh.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File utils\run_host_tests_qemu.ps1
#   powershell ... -File utils\run_host_tests_qemu.ps1 --build
#
# Native host unit tests: utils\run_host_tests.ps1 (not QEMU).
# Espressif qemu-system-xtensa is typically Linux CI; clear error if missing.
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
if (-not $py) { throw "python not found (python -u utils\run_host_tests_qemu.py)" }
$script = Join-Path $Root "utils\run_host_tests_qemu.py"
if ($py -eq "py") {
    & $py -3 -u $script @args
} else {
    & $py -u $script @args
}
exit $LASTEXITCODE
