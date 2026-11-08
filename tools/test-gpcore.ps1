<#
.SYNOPSIS
  Kompilasi dan jalankan tes user-mode logika inti (gpring.h, gpmix.h) dengan cl dari VS 2022.
  Exit code = exit code tes; non-nol bila VS/cl tidak ada atau kompilasi gagal.
#>
param(
    [string]$OutDir = 'out\gpcore-test'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail([string]$msg, [int]$code = 1) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit $code }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { Fail 'vswhere.exe tidak ditemukan' }
$vsPath = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $vsPath) {
    # Runner CI mungkin memakai VS versi lain; pakai yang terbaru yang punya toolset C++.
    $vsPath = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
}
if (-not $vsPath) { Fail 'Visual Studio dengan toolset C++ tidak ditemukan' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat tidak ada: $vcvars" }
Write-Host "VS: $vsPath"

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$inc = Join-Path $root 'driver\Source\Utilities'
$exe = Join-Path $OutDir 'gpcore_test.exe'
$exe2 = Join-Path $OutDir 'gpcore_winheader_check.exe'

# Satu sesi cmd: masuk dev environment, kompilasi kedua sumber.
$cl = "cl /nologo /W4 /WX /EHsc /I `"$inc`" /Fo`"$OutDir\\`""
$bat = Join-Path $OutDir 'build.cmd'
@(
    '@echo off',
    "call `"$vcvars`" >nul || exit /b 1",
    "$cl /Fe`"$exe`" driver\tests\gpcore_test.cpp || exit /b 1",
    "$cl /Fe`"$exe2`" driver\tests\gpcore_winheader_check.cpp || exit /b 1"
) | Set-Content $bat -Encoding ASCII
& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0) { Fail "Kompilasi gagal (exit $LASTEXITCODE)" $LASTEXITCODE }
if (-not (Test-Path $exe) -or -not (Test-Path $exe2)) { Fail 'Executable tes tidak terbentuk' }

& $exe2
if ($LASTEXITCODE -ne 0) { Fail "gpcore_winheader_check gagal (exit $LASTEXITCODE)" $LASTEXITCODE }
& $exe
$code = $LASTEXITCODE
if ($code -ne 0) { Write-Host "Tes gagal (exit $code)" -ForegroundColor Red }
exit $code
