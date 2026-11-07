# Mengaktifkan test-signing mode agar driver uji (bertanda tangan sertifikat sendiri) bisa dimuat.
. "$PSScriptRoot\common.ps1"
Assert-Admin

Write-Host "PERINGATAN:" -ForegroundColor Yellow
Write-Host " - Secure Boot harus MATI. Di VMware: VM Settings > Options > Advanced > hapus centang 'Enable secure boot'."
Write-Host " - Bila BitLocker aktif, suspend dulu (Suspend-BitLocker -MountPoint C: -RebootCount 1)."
Write-Host ""

& bcdedit /set testsigning on
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: bcdedit gagal (kode $LASTEXITCODE). Kemungkinan Secure Boot masih aktif." -ForegroundColor Red
    exit 1
}
Write-Host ""
Write-Host "Test-signing diaktifkan. REBOOT Windows sekarang, lalu jalankan tools\install.ps1." -ForegroundColor Green
