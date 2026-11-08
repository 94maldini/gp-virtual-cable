<#
.SYNOPSIS
  Kemas paket uji untuk VM: out\vm-kit\GPVirtualCable-kit\ lalu zip ke out\vm-kit\GPVirtualCable-kit.zip.
  Jalankan tools\build-driver.ps1 lebih dulu (mengisi out\driver\).
#>
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

$driverSrc = Join-Path $root 'out\driver'
$files = 'GPVirtualCable.sys', 'GPVirtualCable.inf', 'gpvirtualcable.cat', 'package.cer', 'devcon.exe'
$missing = @($files | Where-Object { -not (Test-Path -LiteralPath (Join-Path $driverSrc $_)) })
if ($missing.Count -gt 0) {
    Fail ("out\driver\ tidak lengkap, hilang: " + ($missing -join ', ') + ". Jalankan tools\build-driver.ps1 dulu.")
}

$kitRoot = Join-Path $root 'out\vm-kit'
$kit = Join-Path $kitRoot 'GPVirtualCable-kit'
$zip = Join-Path $kitRoot 'GPVirtualCable-kit.zip'
if (Test-Path -LiteralPath $kitRoot) { Remove-Item -LiteralPath $kitRoot -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $kit 'driver'), (Join-Path $kit 'tools') | Out-Null

foreach ($f in $files) { Copy-Item (Join-Path $driverSrc $f) (Join-Path $kit 'driver') }
foreach ($f in 'enable-testsigning.ps1', 'install.ps1', 'uninstall.ps1', 'common.ps1') {
    Copy-Item (Join-Path $PSScriptRoot $f) (Join-Path $kit 'tools')
}
Copy-Item (Join-Path $root 'cables.toml') $kit

# Sumber Python tanpa .venv, __pycache__, *.egg-info, .pytest_cache.
$pySrc = Join-Path $root 'python'
$pyDst = Join-Path $kit 'python'
New-Item -ItemType Directory -Force $pyDst | Out-Null
Copy-Item (Join-Path $pySrc 'pyproject.toml') $pyDst
foreach ($d in 'gpcable', 'tests') {
    $src = Join-Path $pySrc $d
    Get-ChildItem -LiteralPath $src -Recurse -File | Where-Object {
        $rel = $_.FullName.Substring($src.Length)
        $rel -notmatch '(^|\\)(__pycache__|\.pytest_cache|\.venv)(\\|$)' -and $rel -notmatch '\.egg-info(\\|$)' -and $_.Extension -ne '.pyc'
    } | ForEach-Object {
        $dest = Join-Path (Join-Path $pyDst $d) $_.FullName.Substring($src.Length)
        New-Item -ItemType Directory -Force (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath $_.FullName $dest
    }
}

$readme = @'
GP Virtual Cable - paket uji VM
================================
Jalankan semua perintah di PowerShell "Run as administrator" dari folder paket ini.

0. VM mati -> VM Settings > Options > Advanced > hapus centang "Enable secure boot".
   Bila BitLocker aktif, suspend dulu.
1. Set-ExecutionPolicy -Scope Process Bypass
   .\tools\enable-testsigning.ps1        (lalu REBOOT VM)
2. .\tools\install.ps1                   (lalu cek mmsys.cpl: 12 Playback "GP Mono 1".."GP Stereo 4"
                                          + 1 Recording "GP Virtual Cable" 8 channel)
3. Pasang Python 3.11+ (python.org, centang "Add to PATH"), lalu:
   pip install -e ".\python[dev]"
   python -m gpcable.loopback            (harap: 12/12 passed)
   cd python ; python -m pytest -m e2e ; cd ..
   python -m gpcable.meter
4. Opsional Driver Verifier: verifier /standard /driver GPVirtualCable.sys -> reboot -> putar audio
   ke 4 Playback + meter ~1 jam + sekali sleep/resume -> verifier /reset
5. Kirim balik: nama perangkat di mmsys.cpl, keluaran loopback, hasil e2e, BSOD/glitch bila ada.
Uninstall: .\tools\uninstall.ps1 ; bcdedit /set testsigning off
'@
Set-Content -LiteralPath (Join-Path $kit 'BACA-SAYA.txt') -Value $readme -Encoding UTF8

Compress-Archive -Path $kit -DestinationPath $zip -Force
$sz = (Get-Item -LiteralPath $zip).Length
Write-Host ("Zip: {0} ({1:N1} KB)" -f $zip, ($sz / 1KB)) -ForegroundColor Green
