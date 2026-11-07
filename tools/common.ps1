# Fungsi bersama untuk skrip GP Virtual Cable (di-dot-source oleh skrip lain).

function Assert-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($id)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Host "ERROR: skrip ini harus dijalankan sebagai Administrator." -ForegroundColor Red
        Write-Host "Buka PowerShell dengan 'Run as administrator', lalu jalankan ulang."
        exit 1
    }
}

function Find-Devcon {
    param([string]$DevconPath, [string[]]$SearchDirs)
    if ($DevconPath) {
        if (Test-Path -LiteralPath $DevconPath) { return (Resolve-Path -LiteralPath $DevconPath).Path }
        Write-Host "ERROR: devcon.exe tidak ditemukan di '$DevconPath'." -ForegroundColor Red
        exit 1
    }
    foreach ($d in $SearchDirs) {
        $c = Join-Path $d 'devcon.exe'
        if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path }
    }
    $kits = ${env:ProgramFiles(x86)}
    if ($kits) {
        $c = Get-ChildItem -Path (Join-Path $kits 'Windows Kits\10\Tools\*\x64\devcon.exe') -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending | Select-Object -First 1
        if ($c) { return $c.FullName }
    }
    Write-Host "ERROR: devcon.exe tidak ditemukan." -ForegroundColor Red
    Write-Host "VM tidak punya WDK, jadi salin devcon.exe dari mesin pengembang ke samping folder paket"
    Write-Host "(folder paket atau folder tools ini), atau tentukan lewat -DevconPath."
    exit 1
}

# Mengembalikan $true bila teks keluaran 'devcon hwids' memuat hardware ID Root\GPVirtualCable.
# Pola di-escape: '\G' bila tidak di-escape adalah anchor regex .NET.
function Test-HwidsOutput {
    param([string]$Text)
    $pattern = '(?im)^\s+' + [regex]::Escape('Root\GPVirtualCable') + '\s*$'
    return ($Text -match $pattern)
}

# devcon hwids dapat mengembalikan kode keluar bukan-nol saat tidak ada yang cocok,
# jadi keputusan diambil dari isi keluaran, bukan dari $LASTEXITCODE.
function Test-DeviceInstalled {
    param([string]$Devcon)
    $out = & $Devcon hwids 'Root\GPVirtualCable' 2>&1 | Out-String
    return (Test-HwidsOutput -Text $out)
}

# Menilai kode keluar alat native. devcon: 0 = ok, 1 = ok tetapi perlu reboot, 2 = gagal.
# pnputil: 0 = ok, 3010 = ok tetapi perlu reboot. Mengembalikan 'ok', 'reboot', atau 'fail'.
function Get-ExitStatus {
    param([ValidateSet('devcon','pnputil')][string]$Tool, [int]$Code)
    if ($Code -eq 0) { return 'ok' }
    if ($Tool -eq 'devcon' -and $Code -eq 1) { return 'reboot' }
    if ($Tool -eq 'pnputil' -and $Code -eq 3010) { return 'reboot' }
    return 'fail'
}

# Cetak hasil; keluar dengan 1 bila gagal. Mengembalikan $true bila perlu reboot.
function Assert-ExitOk {
    param([string]$Tool, [int]$Code, [string]$What)
    $st = Get-ExitStatus -Tool $Tool -Code $Code
    if ($st -eq 'fail') {
        Write-Host "ERROR: $What gagal (kode $Code)." -ForegroundColor Red
        exit 1
    }
    if ($st -eq 'reboot') {
        Write-Host "CATATAN: $What berhasil tetapi Windows meminta REBOOT." -ForegroundColor Yellow
        return $true
    }
    return $false
}
