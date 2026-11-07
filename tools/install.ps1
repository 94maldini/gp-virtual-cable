param(
    [string]$PackageDir = "$PSScriptRoot\..\driver\x64\Debug\package",
    [string]$CertPath,
    [string]$DevconPath
)
. "$PSScriptRoot\common.ps1"
Assert-Admin

if (-not (Test-Path -LiteralPath $PackageDir)) {
    Write-Host "ERROR: folder paket tidak ditemukan: $PackageDir" -ForegroundColor Red
    exit 1
}
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$inf = Join-Path $PackageDir 'GPVirtualCable.inf'
if (-not (Test-Path -LiteralPath $inf)) {
    Write-Host "ERROR: GPVirtualCable.inf tidak ada di $PackageDir" -ForegroundColor Red
    exit 1
}

if (-not $CertPath) {
    $CertPath = Join-Path $PackageDir 'package.cer'
    if (-not (Test-Path -LiteralPath $CertPath)) {
        $CertPath = Join-Path (Split-Path -Parent $PackageDir) 'package.cer'
    }
}
if (-not (Test-Path -LiteralPath $CertPath)) {
    Write-Host "ERROR: sertifikat package.cer tidak ditemukan ($CertPath)." -ForegroundColor Red
    exit 1
}

$devcon = Find-Devcon -DevconPath $DevconPath -SearchDirs @($PackageDir, $PSScriptRoot)

foreach ($store in 'Cert:\LocalMachine\Root', 'Cert:\LocalMachine\TrustedPublisher') {
    try {
        Import-Certificate -FilePath $CertPath -CertStoreLocation $store -ErrorAction Stop | Out-Null
        Write-Host "Sertifikat diimpor ke $store"
    } catch {
        Write-Host "ERROR: gagal impor sertifikat ke ${store}: $_" -ForegroundColor Red
        exit 1
    }
}

if (Test-DeviceInstalled -Devcon $devcon) {
    Write-Host "Perangkat sudah terpasang, menjalankan devcon update..."
    & $devcon update $inf 'Root\GPVirtualCable'
} else {
    Write-Host "Menjalankan devcon install..."
    & $devcon install $inf 'Root\GPVirtualCable'
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: devcon gagal (kode $LASTEXITCODE)." -ForegroundColor Red
    exit 1
}
Write-Host "Selesai. Periksa Device Manager ('GP Virtual Cable') dan mmsys.cpl." -ForegroundColor Green
