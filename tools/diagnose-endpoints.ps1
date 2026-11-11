param(
    [string]$OutFile = "$PSScriptRoot\..\gp-endpoints.txt"
)
# Diagnostik read-only: menampilkan endpoint GP Virtual Cable yang dibuat Windows.
# Tidak mengubah apa pun. Hasil juga disimpan ke gp-endpoints.txt di folder paket.

$ErrorActionPreference = 'Continue'
$formFactors = @{
    0 = 'RemoteNetworkDevice'; 1 = 'Speakers'; 2 = 'LineLevel'; 3 = 'Headphones'
    4 = 'Microphone'; 5 = 'Headset'; 6 = 'Handset'; 7 = 'UnknownDigitalPassthrough'
    8 = 'SPDIF'; 9 = 'DigitalAudioDisplayDevice'; 10 = 'UnknownFormFactor'
}
$keyName    = '{a45c254e-df1c-4efd-8020-67d146a850e0},2'   # nama endpoint (bagian sebelum kurung), mis. "CABLE Input"
$keyDesc    = '{b3f8fa53-0004-438e-9003-51a46e139bfc},6'   # nama adapter (di dalam kurung), mis. "GP Virtual Cable"
$keyFF      = '{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},0'   # PKEY_AudioEndpoint_FormFactor

function Read-Prop($path, $name) {
    try { (Get-ItemProperty -LiteralPath $path -Name $name -ErrorAction Stop).$name } catch { $null }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("== Driver GP Virtual Cable ==")
$dev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like 'ROOT\*GPVIRTUALCABLE*' -or $_.FriendlyName -like '*GP Virtual Cable*' -and $_.Class -eq 'MEDIA' }
foreach ($d in $dev) {
    $ver  = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_DriverVersion' -ErrorAction SilentlyContinue).Data
    $date = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_DriverDate' -ErrorAction SilentlyContinue).Data
    $inf  = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction SilentlyContinue).Data
    $lines.Add(("{0} | {1} | versi={2} | tanggal={3} | inf={4}" -f $d.Status, $d.InstanceId, $ver, $date, $inf))
}
if (-not $dev) { $lines.Add("(perangkat Root\GPVirtualCable tidak ditemukan)") }

$lines.Add("")
$lines.Add("== Nama di HKLM MediaCategories (GP*) ==")
$mc = foreach ($k in (Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\MediaCategories' -ErrorAction SilentlyContinue)) {
    $n = (Get-ItemProperty -LiteralPath $k.PSPath -Name Name -ErrorAction SilentlyContinue).Name
    if ($n -like 'GP *') { "{0} = {1}" -f $k.PSChildName, $n }
}
if ($mc) { $mc | Sort-Object | ForEach-Object { $lines.Add($_) } } else { $lines.Add("(tidak ada)") }

foreach ($flow in 'Render', 'Capture') {
    $lines.Add("")
    $lines.Add("== $flow endpoints (registry MMDevices) ==")
    $base = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\$flow"
    $rows = foreach ($k in (Get-ChildItem -LiteralPath $base -ErrorAction SilentlyContinue)) {
        $p = Join-Path $k.PSPath 'Properties'
        $adapter = Read-Prop $p $keyDesc
        if ($adapter -notlike '*GP Virtual Cable*') { continue }
        $state = (Get-ItemProperty -LiteralPath $k.PSPath -Name DeviceState -ErrorAction SilentlyContinue).DeviceState
        $stateText = switch ($state) { 1 {'ACTIVE'} 2 {'DISABLED'} 4 {'NOTPRESENT'} 8 {'UNPLUGGED'} default {"state=$state"} }
        $ff = Read-Prop $p $keyFF
        $ffText = if ($null -ne $ff -and $formFactors.ContainsKey([int]$ff)) { $formFactors[[int]$ff] } else { "ff=$ff" }
        [pscustomobject]@{
            Name = Read-Prop $p $keyName
            FF   = $ffText
            St   = $stateText
            Id   = $k.PSChildName
        }
    }
    foreach ($r in ($rows | Sort-Object St, Name)) {
        $lines.Add(("{0,-10} | {1,-26} | nama={2,-22} | id={3}" -f $r.St, $r.FF, $r.Name, $r.Id))
    }
    if (-not $rows) { $lines.Add("(tidak ada)") }
}

$lines | ForEach-Object { Write-Host $_ }
try {
    $lines | Out-File -FilePath $OutFile -Encoding utf8
    Write-Host ""
    Write-Host ("Hasil juga disimpan ke: {0}" -f (Resolve-Path -LiteralPath $OutFile).Path) -ForegroundColor Green
} catch {
    Write-Host "Gagal menyimpan file hasil: $($_.Exception.Message)" -ForegroundColor Yellow
}
