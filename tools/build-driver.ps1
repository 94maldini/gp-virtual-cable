<#
.SYNOPSIS
  Build driver\GPVirtualCable.sln (x64) dengan VS 2022 Build Tools + WDK terpasang, periksa INF hasil
  build dengan infverif /w, lalu kumpulkan ke out\driver\.
.PARAMETER CodeAnalysis
  Jalankan juga Code Analysis (/p:RunCodeAnalysis=true).
#>
param(
    [string]$Configuration = 'Debug',
    [string]$Solution = 'driver\GPVirtualCable.sln',
    [string]$LogFile = 'out\msbuild.log',
    [string]$WdkToolsRoot = (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools'),
    [switch]$CodeAnalysis
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail([string]$msg, [int]$code = 1) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit $code }

if ($Configuration -notin 'Debug', 'Release') { Fail "Configuration harus Debug atau Release (diberi '$Configuration')" }

# Bersihkan output lama sebelum build agar build gagal tidak meninggalkan hasil basi.
$dest = Join-Path $root 'out\driver'
if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
New-Item -ItemType Directory -Force $dest | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
# MSBuild 64-bit: tugas verifikasi INF bawaan WDK (DPVerifierTask) memuat bin\<arsitektur proses>\InfVerif.dll,
# dan WDK 10.0.26100 hanya memasang versi x64/arm64 (MSBuild x86 gagal dengan "Unable to load DLL 'x86\InfVerif.dll'").
$msbuild = if ($vsPath) { Join-Path $vsPath 'MSBuild\Current\Bin\amd64\MSBuild.exe' }
if (-not $msbuild -or -not (Test-Path $msbuild)) { Fail 'MSBuild VS 2022 tidak ditemukan' }
Write-Host "MSBuild: $msbuild"

# Alat WDK (devcon, infverif) dari versi terbaru di Tools, diurutkan sebagai [version].
function Find-WdkTool([string]$name) {
    Get-ChildItem $WdkToolsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -as [version] } | Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName "x64\$name" } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
$devcon = Find-WdkTool 'devcon.exe'
if (-not $devcon) { Fail "devcon.exe (x64) tidak ditemukan di $WdkToolsRoot" }
$infverif = Find-WdkTool 'infverif.exe'
if (-not $infverif) { Fail "infverif.exe (x64) tidak ditemukan di $WdkToolsRoot" }
Write-Host "devcon: $devcon"
Write-Host "infverif: $infverif"

$logDir = Split-Path -Parent $LogFile
if ($logDir) { New-Item -ItemType Directory -Force $logDir | Out-Null }

# Inf2CatUseLocalTime: inf2cat default memakai UTC; di zona waktu di depan UTC, DriverVer hari ini dianggap "masa depan".
# _NT_TARGET_VERSION juga ditetapkan di driver\Directory.Build.props (build dari VS ikut sama).
$msbuildArgs = @($Solution, "/p:Configuration=$Configuration", '/p:Platform=x64', '/p:_NT_TARGET_VERSION=0x0A000008',
    '/p:Inf2CatUseLocalTime=true', '/m', '/v:minimal', '/fl', "/flp:logfile=$LogFile;verbosity=detailed")
if ($CodeAnalysis) { $msbuildArgs += '/p:RunCodeAnalysis=true' }
Write-Host "msbuild $($msbuildArgs -join ' ')"
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) { Fail "Build gagal (exit $LASTEXITCODE), lihat $LogFile" $LASTEXITCODE }
# Beberapa tugas WDK mencatat error tanpa membuat exit code bukan-nol; periksa ringkasan log juga.
# Warning: build biasa harus 0. Dengan -CodeAnalysis, kode sampel Microsoft yang tidak kita ubah masih
# memunculkan peringatan Code Analysis (C26495 dll.), jadi hanya ditampilkan, bukan menggagalkan build.
$summary = Select-String -Path $LogFile -Pattern '^\s*(\d+) (Warning|Error)\(s\)' | Select-Object -Last 2
foreach ($m in $summary) {
    $n = [int]$m.Matches[0].Groups[1].Value
    $kind = $m.Matches[0].Groups[2].Value
    if ($n -ne 0 -and ($kind -eq 'Error' -or -not $CodeAnalysis)) {
        Fail "Build mencatat $n $kind(s), lihat $LogFile"
    }
}
Write-Host "Ringkasan build: $(($summary | ForEach-Object { $_.Line.Trim() }) -join ', ')"

$pkgDir = Join-Path $root "driver\x64\$Configuration\package"
Write-Host "Sumber paket: $pkgDir"
if (-not (Test-Path $pkgDir)) { Fail "Folder paket tidak ada: $pkgDir" }
foreach ($f in 'GPVirtualCable.sys', 'GPVirtualCable.inf', 'GPVirtualCable.cat') {
    if (-not (Test-Path (Join-Path $pkgDir $f))) { Fail "Output wajib tidak ada di ${pkgDir}: $f" }
}
# Sertifikat uji diekspor WDK di samping folder paket: driver\x64\<cfg>\package.cer
$cer = Join-Path $root "driver\x64\$Configuration\package.cer"
if (-not (Test-Path $cer)) { Fail "File .cer tidak ditemukan: $cer" }

# INF hasil build (sesudah stampinf) harus lolos aturan Windows Driver.
$inf = Join-Path $pkgDir 'GPVirtualCable.inf'
Write-Host "infverif /w $inf"
& $infverif /w $inf
if ($LASTEXITCODE -ne 0) { Fail "infverif /w gagal (exit $LASTEXITCODE)" $LASTEXITCODE }
Write-Host 'infverif /w: OK'

Copy-Item (Join-Path $pkgDir '*') $dest -Recurse -Force
Copy-Item $cer $dest -Force
Copy-Item $devcon $dest -Force

Write-Host "Hasil di $dest"
Get-ChildItem $dest | Format-Table Name, Length -AutoSize
