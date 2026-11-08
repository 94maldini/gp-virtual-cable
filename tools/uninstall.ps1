param(
    [string]$PackageDir,
    [string]$DevconPath
)
. "$PSScriptRoot\common.ps1"
Assert-Admin
if (-not $PackageDir) { $PackageDir = Get-DefaultPackageDir }

$reboot = $false
$devcon = Find-Devcon -DevconPath $DevconPath -SearchDirs @($PackageDir, $PSScriptRoot)

if (Test-DeviceInstalled -Devcon $devcon) {
    & $devcon remove 'Root\GPVirtualCable'
    if (Assert-ExitOk -Tool 'devcon' -Code $LASTEXITCODE -What 'devcon remove') { $reboot = $true }
} else {
    Write-Host "Perangkat Root\GPVirtualCable tidak terpasang, lewati devcon remove."
}

# Label keluaran pnputil dilokalisasi, jadi blok dikenali dari nama berkas
# 'gpvirtualcable.inf' dan nama terbit dari pola oemNN.inf (tidak bergantung bahasa).
$text = (& pnputil /enum-drivers | Out-String)
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: pnputil /enum-drivers gagal (kode $LASTEXITCODE)." -ForegroundColor Red
    exit 1
}
$published = @()
foreach ($block in ($text -split '(?:\r?\n){2,}')) {
    if ($block -match '(?i)gpvirtualcable\.inf' -and $block -match '(?i)\b(oem\d+\.inf)\b') {
        $published += $Matches[1]
    }
}
if ($published.Count -eq 0) {
    Write-Host "Tidak ada paket driver GPVirtualCable di driver store."
    exit 0
}
foreach ($oem in $published) {
    Write-Host "Menghapus paket $oem ..."
    & pnputil /delete-driver $oem /uninstall /force
    if (Assert-ExitOk -Tool 'pnputil' -Code $LASTEXITCODE -What "pnputil /delete-driver $oem") { $reboot = $true }
}
if ($reboot) { Write-Host "Reboot Windows untuk menyelesaikan uninstall." -ForegroundColor Yellow }
Write-Host "Uninstall selesai." -ForegroundColor Green
