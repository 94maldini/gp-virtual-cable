<#
.SYNOPSIS
  Build driver\SimpleAudioSample.sln (x64) dengan VS 2022 Build Tools + WDK terpasang, lalu kumpulkan ke out\driver\.
#>
param(
    [string]$Configuration = 'Debug',
    [string]$Solution = 'driver\SimpleAudioSample.sln',
    [string]$LogFile = 'out\msbuild.log',
    [string]$WdkToolsRoot = (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools')
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
$msbuild = if ($vsPath) { Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe' }
if (-not $msbuild -or -not (Test-Path $msbuild)) { Fail 'MSBuild VS 2022 tidak ditemukan' }
Write-Host "MSBuild: $msbuild"

$logDir = Split-Path -Parent $LogFile
if ($logDir) { New-Item -ItemType Directory -Force $logDir | Out-Null }

# Inf2CatUseLocalTime: inf2cat default memakai UTC; di zona waktu di depan UTC, DriverVer hari ini dianggap "masa depan".
& $msbuild $Solution /p:Configuration=$Configuration /p:Platform=x64 /p:_NT_TARGET_VERSION=0x0A000008 /p:Inf2CatUseLocalTime=true /m /v:minimal "/fl" "/flp:logfile=$LogFile;verbosity=detailed"
if ($LASTEXITCODE -ne 0) { Fail "Build gagal (exit $LASTEXITCODE), lihat $LogFile" $LASTEXITCODE }

$pkgDir = Join-Path $root "driver\x64\$Configuration\package"
Write-Host "Sumber paket: $pkgDir"
if (-not (Test-Path $pkgDir)) { Fail "Folder paket tidak ada: $pkgDir" }
foreach ($f in 'SimpleAudioSample.sys', 'SimpleAudioSample.inf', 'simpleaudiosample.cat') {
    if (-not (Test-Path (Join-Path $pkgDir $f))) { Fail "Output wajib tidak ada di ${pkgDir}: $f" }
}
# Sertifikat uji diekspor WDK di samping folder paket: driver\x64\<cfg>\package.cer
$cer = Join-Path $root "driver\x64\$Configuration\package.cer"
if (-not (Test-Path $cer)) { Fail "File .cer tidak ditemukan: $cer" }

# devcon dari versi WDK yang cocok dengan build (versi terbaru, diurutkan sebagai [version]).
$devcon = Get-ChildItem $WdkToolsRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -as [version] } | Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Join-Path $_.FullName 'x64\devcon.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $devcon) { Fail "devcon.exe (x64) tidak ditemukan di $WdkToolsRoot" }
Write-Host "devcon: $devcon"

Copy-Item (Join-Path $pkgDir '*') $dest -Recurse -Force
Copy-Item $cer $dest -Force
Copy-Item $devcon $dest -Force

Write-Host "Hasil di $dest"
Get-ChildItem $dest | Format-Table Name, Length -AutoSize
