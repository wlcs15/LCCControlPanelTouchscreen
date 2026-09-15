# Fake-data tests for collect + host wrap. Never uses a real house PSK.
# Same checks as utils/test_wifi_wrap.sh (provisioner TTY check needs Git Bash).
#   powershell -NoProfile -ExecutionPolicy Bypass -File utils\test_wifi_wrap.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

function Invoke-Python {
    param([Parameter(Mandatory)][string[]]$PyArgs)
    $exe = $null
    $prefix = @()
    if (Get-Command python -ErrorAction SilentlyContinue) { $exe = "python" }
    elseif (Get-Command python3 -ErrorAction SilentlyContinue) { $exe = "python3" }
    elseif (Get-Command py -ErrorAction SilentlyContinue) { $exe = "py"; $prefix = @("-3") }
    else { throw "python not on PATH" }
    $all = $prefix + $PyArgs
    & $exe @all | ForEach-Object { Write-Host $_ }
    return ,$LASTEXITCODE
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("lcc_wrap_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    Write-Host "== wifi_wrap selftest =="
    $code = Invoke-Python @((Join-Path $Root "utils\wifi_wrap.py"), "selftest")
    if ($code -ne 0) { exit $code }

    Write-Host "== wifi_wrap golden vectors (Password!; node ID ignored) =="
    $code = Invoke-Python @((Join-Path $Root "utils\test_wifi_wrap_vectors.py"))
    if ($code -ne 0) { exit $code }

    Write-Host "== collect_hw_ids from fake log =="
    $ids = Join-Path $tmp "hw_ids.env"
    $code = Invoke-Python @(
        (Join-Path $Root "utils\collect_hw_ids.py"),
        "--from-log", (Join-Path $Root "utils\testdata\fake_debug_ids.txt"),
        "--out", $ids
    )
    if ($code -ne 0) { exit $code }
    $envText = Get-Content -Raw $ids
    foreach ($need in @(
        "WIFI_MAC=DE:AD:BE:EF:00:01",
        "WIFI_NODE_ID=05.01.01.01.A5.04",
        "WIFI_FLASH_UID=0123456789ABCDEF"
    )) {
        if ($envText -notmatch [regex]::Escape($need)) {
            Write-Host "missing $need in $ids" -ForegroundColor Red
            exit 1
        }
    }

    Write-Host "== host encrypt fake PSK to wrap include =="
    $fakePsk = "fake-psk-not-a-house-password"
    $wrap = Join-Path $tmp "wifi_psk_wrap.inc"
    $wrapPy = Join-Path $Root "utils\wifi_wrap.py"
    $pyExe = "python"
    $pyArgList = @($wrapPy, "encrypt", "--ids", $ids, "--ssid", "SRIF2333", "--out", $wrap)
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
        if (Get-Command python3 -ErrorAction SilentlyContinue) { $pyExe = "python3" }
        else { $pyExe = "py"; $pyArgList = @("-3") + $pyArgList }
    }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = (Get-Command $pyExe).Source
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput = $true
    $quoted = foreach ($a in $pyArgList) {
        if ($a -match '\s') { '"' + $a + '"' } else { $a }
    }
    $psi.Arguments = [string]::Join(" ", $quoted)
    $proc = [System.Diagnostics.Process]::Start($psi)
    $pskBytes = [System.Text.Encoding]::ASCII.GetBytes($fakePsk)
    $proc.StandardInput.BaseStream.Write($pskBytes, 0, $pskBytes.Length)
    $proc.StandardInput.Close()
    $proc.WaitForExit()
    if ($proc.ExitCode -ne 0) { exit $proc.ExitCode }
    $wrapText = Get-Content -Raw $wrap
    if ($wrapText.Contains($fakePsk)) {
        Write-Host "FAIL: plaintext PSK leaked into wrap include" -ForegroundColor Red
        exit 1
    }
    if ($wrapText -notmatch 'kWifiWrapBlob\[94\]') {
        Write-Host "missing kWifiWrapBlob[94] in wrap include" -ForegroundColor Red
        exit 1
    }
    if ($wrapText -notmatch 'kWifiWrapSsid') {
        Write-Host "missing kWifiWrapSsid in wrap include" -ForegroundColor Red
        exit 1
    }

    $bashPath = $null
    $b = Get-Command bash -ErrorAction SilentlyContinue
    if ($b) { $bashPath = $b.Source }
    elseif (Test-Path "C:\Program Files\Git\bin\bash.exe") {
        $bashPath = "C:\Program Files\Git\bin\bash.exe"
    }
    if ($bashPath) {
        Write-Host "== provision script refuses non-TTY =="
        $provErr = Join-Path $tmp "prov.err"
        $shUnix = ((Join-Path $Root "utils\provision_wifi_build.sh") -replace '\\', '/')
        cmd /c "type NUL | `"$bashPath`" `"$shUnix`" >NUL 2>`"$provErr`""
        if ($LASTEXITCODE -eq 0) {
            Write-Host "FAIL: provision_wifi_build.sh should refuse a pipe" -ForegroundColor Red
            exit 1
        }
        $errText = Get-Content -Raw $provErr -ErrorAction SilentlyContinue
        if ($errText -notmatch 'interactive terminal') {
            Write-Host "FAIL: provision_wifi_build.sh error should mention interactive terminal" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "skip provision_wifi_build.sh TTY check (no bash on PATH)"
    }

    Write-Host "All fake-data script tests passed."
    exit 0
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
