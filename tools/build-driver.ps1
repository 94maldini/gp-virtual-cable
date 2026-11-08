<#
.SYNOPSIS
  Build driver\SimpleAudioSample.sln (x64) dengan VS 2022 Build Tools + WDK terpasang, lalu kumpulkan ke out\driver\.
#>
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Solution = 'driver\SimpleAudioSample.sln',
    [string]$LogFile = 'out\msbuild.log'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { Write-Error 'MSBuild VS 2022 tidak ditemukan'; exit 1 }
Write-Host "MSBuild: $msbuild"

New-Item -ItemType Directory -Force out | Out-Null
$logDir = Split-Path -Parent $LogFile
if ($logDir) { New-Item -ItemType Directory -Force $logDir | Out-Null }

& $msbuild $Solution /p:Configuration=$Configuration /p:Platform=x64 /p:_NT_TARGET_VERSION=0x0A000008 /p:Inf2CatUseLocalTime=true /m /v:minimal "/fl" "/flp:logfile=$LogFile;verbosity=detailed"
if ($LASTEXITCODE -ne 0) { Write-Error "Build gagal (exit $LASTEXITCODE), lihat $LogFile"; exit $LASTEXITCODE }

$sys = Get-ChildItem driver -Recurse -Filter SimpleAudioSample.sys | Where-Object { $_.DirectoryName -match 'package' } | Select-Object -First 1
if (-not $sys) { Write-Error 'SimpleAudioSample.sys di folder package tidak ditemukan'; exit 1 }
$pkgDir = $sys.DirectoryName

$dest = Join-Path $root 'out\driver'
if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item (Join-Path $pkgDir '*') $dest -Recurse -Force

$cer = Get-ChildItem driver -Recurse -Include *.cer | Select-Object -First 1
if ($cer) { Copy-Item $cer.FullName $dest -Force } else { Write-Warning 'File .cer tidak ditemukan' }

$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools'
$devcon = Get-ChildItem $kits -Recurse -Filter devcon.exe | Where-Object { $_.FullName -match '[\\/]x64[\\/]' } | Select-Object -First 1
if ($devcon) { Copy-Item $devcon.FullName $dest -Force } else { Write-Warning 'devcon.exe tidak ditemukan di WDK' }

Write-Host "Hasil di $dest"
Get-ChildItem $dest | Format-Table Name, Length -AutoSize
