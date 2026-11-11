# GP Virtual Cable

Driver audio virtual Windows (kernel, milik sendiri) untuk dipakai bersama **GP Mix-Lite** (gassanov.id) dan **OBS**. Pemakaian pribadi.

GP Mix-Lite hanya bisa memilih **satu** perangkat input. Karena itu driver ini membuat **12 perangkat Playback** yang semuanya dijumlahkan (dengan saturasi) ke **satu perangkat Recording 8 channel** bernama **GP Virtual Cable** (48 kHz / 24-bit). Tiap aplikasi diarahkan ke Playback tertentu; di GP Mix, tiap pasang channel input menjadi satu strip mixer.

## Pemetaan channel

| Playback (output Windows) | Channel | Masuk ke Recording "GP Virtual Cable" |
|---|---|---|
| GP Mono 1 .. GP Mono 8 | 1 ch | channel 1 .. 8 |
| GP Stereo 1 | 2 ch | channel 1 + 2 |
| GP Stereo 2 | 2 ch | channel 3 + 4 |
| GP Stereo 3 | 2 ch | channel 5 + 6 |
| GP Stereo 4 | 2 ch | channel 7 + 8 |

Aturan: **semua Playback selalu ada**; Playback yang menunjuk channel yang sama **dijumlahkan** (mis. GP Mono 3 dan GP Stereo 2-kiri sama-sama di channel 3). Nama dan pemetaan diatur di `cables.toml`.

## Persyaratan dan risiko

- Windows 10/11 x64, hak Administrator.
- Driver ini **kernel driver bertanda tangan uji (test-signed)**: Windows harus dalam **test-signing mode** dan **Secure Boot harus mati**. Kesalahan di driver kernel bisa menyebabkan BSOD.
- **Uji dulu di VM.** Di PC utama, buat **System Restore point** sebelum memasang (rollback lewat `rstrui`).
- Build lokal: Visual Studio 2022 (C++), WDK 10.0.26100 (lihat `docs/setup-notes.md`). Python 3.11+ untuk alat uji dan meter (`numpy`, `sounddevice`, `pycaw`).

## Build (di mesin pengembang)

```powershell
tools\build-driver.ps1                         # hasil di out\driver\
tools\build-driver.ps1 -Configuration Release  # opsional
tools\test-gpcore.ps1                          # tes user-mode ring buffer + mixer
```

Tes Python:

```powershell
cd python
pip install -e ".[dev]"
python -m pytest            # e2e otomatis dilewati bila driver belum terpasang
```

## Paket uji VM

```powershell
tools\make-vm-kit.ps1
```

Menghasilkan `out\vm-kit\GPVirtualCable-kit.zip` (driver, skrip, sumber Python, `cables.toml`, `BACA-SAYA.txt`). Salin ke VM Windows 11 di VMware lewat shared folder atau drag-and-drop (butuh VMware Tools), lalu ekstrak.

## Uji di VM (Windows 11, VMware)

Semua perintah di PowerShell **Run as administrator**.

> **Penting:** setiap jendela PowerShell Administrator **baru** (termasuk setelah reboot) harus diawali `Set-ExecutionPolicy -Scope Process Bypass`, kalau tidak skrip `.ps1` ditolak (kebijakan bawaan Windows 11 = Restricted). Pindah juga ke folder paket: `cd C:\...\GPVirtualCable-kit` (ganti dengan lokasi ekstrak).

Sebelum mulai, ambil **snapshot VMware**; bila VM gagal boot, kembalikan ke snapshot itu.

1. Matikan VM, lalu VM Settings -> Options -> Advanced -> hapus centang **Enable secure boot**. Bila BitLocker aktif, **suspend dulu sebelum test-signing + reboot**: `Suspend-BitLocker -MountPoint C: -RebootCount 1`.
2. Aktifkan test-signing, lalu **reboot**:
   ```powershell
   cd C:\...\GPVirtualCable-kit
   Set-ExecutionPolicy -Scope Process Bypass
   .\tools\enable-testsigning.ps1
   ```
3. Pasang driver (jendela Admin **baru** setelah reboot, jadi ulangi `cd` dan `Set-ExecutionPolicy`):
   ```powershell
   cd C:\...\GPVirtualCable-kit
   Set-ExecutionPolicy -Scope Process Bypass
   .\tools\install.ps1
   ```
   Tanpa `-PackageDir`, skrip memakai folder `driver\` di samping `tools\`. Opsi: `-PackageDir`, `-CertPath`, `-DevconPath`.
4. Buka `mmsys.cpl`. Harus ada **12 Playback**: "GP Mono 1".."GP Mono 8", "GP Stereo 1".."GP Stereo 4" (tampil sebagai mis. "GP Mono 1 (GP Virtual Cable)", jenis Line), dan **1 Recording "GP Virtual Cable"** (8 channel). Cek cepat: `Get-PnpDevice -Class AudioEndpoint | Format-Table Status,FriendlyName`. Bila Playback tampil "Speakers (GP Virtual Cable)", lihat Troubleshooting; untuk versi driver baru di VM, selalu mulai dari snapshot **sebelum** pemasangan pertama.
5. Pasang Python 3.11+ (python.org, centang "Add to PATH"), lalu di jendela PowerShell baru:
   ```powershell
   cd C:\...\GPVirtualCable-kit
   pip install -e ".\python[dev]"
   python -m gpcable.loopback
   ```
   Keluaran per cable: `PASS GP Mono 1: 300 Hz -> ch 1..1` (atau `FAIL ...`), diakhiri `12/12 passed`.
6. Tes end-to-end (dari folder `python` di dalam paket):
   ```powershell
   cd C:\...\GPVirtualCable-kit\python
   python -m pytest -m e2e
   ```
7. Meter level (dari folder mana saja setelah `pip install -e`): `python -m gpcable.meter` (atau `gp-meter`).
8. **Opsional - Driver Verifier:**
   ```powershell
   verifier /standard /driver GPVirtualCable.sys
   ```
   Reboot, putar audio ke 4 Playback sekaligus + jalankan meter selama sekitar 1 jam, lakukan satu kali sleep/resume. Tanpa BSOD dan `python -m gpcable.loopback` tetap `12/12 passed`. Selesai: `verifier /reset` lalu reboot.

## Pasang di PC utama (setelah lulus di VM)

1. Buat **System Restore point** (rollback: `rstrui`).
2. Matikan **Secure Boot** di BIOS/UEFI; suspend BitLocker bila dipakai.
3. Suspend BitLocker bila dipakai. Di PowerShell Administrator, `cd <folder repo>`, jalankan `Set-ExecutionPolicy -Scope Process Bypass`, lalu `tools\enable-testsigning.ps1` -> reboot. Buka PowerShell Administrator **baru**, `cd <folder repo>`, ulangi `Set-ExecutionPolicy -Scope Process Bypass`, lalu `tools\install.ps1` (skrip memakai `out\driver\`).
4. Periksa `mmsys.cpl` seperti di atas.

### Setup GP Mix-Lite

- **Setup -> Audio device type**: "Windows Audio" atau "Windows Audio (Exclusive Mode)".
- **Input**: "GP Virtual Cable".
- **Sample rate: 48000 Hz** (wajib; driver hanya mendukung 48 kHz).
- Aktifkan input channel **1+2, 3+4, 5+6, 7+8** (dan channel mono sesuai kebutuhan).
- Routing per aplikasi (Windows 10: Settings -> System -> Sound -> App volume and device preferences; Windows 11: Settings -> System -> Sound -> Volume mixer; atau output device di aplikasinya):
  - Discord -> output **GP Stereo 2** -> muncul di GP Mix input **3+4**
  - Spotify -> output **GP Stereo 3** -> muncul di input **5+6**
- **Mic**: Control Panel -> Sound -> tab Recording -> mic -> Properties -> Listen -> centang "Listen to this device" -> "Playback through this device" = **GP Mono 1** (tampil di input 1). Ada tambahan latensi dari jalur ini.
- **OBS**: ambil keluaran (output) GP Mix sebagai sumber audio.

## Meter

`python -m gpcable.meter` menampilkan level tiap channel (tkinter). Berguna untuk memastikan aplikasi masuk ke channel yang benar.

## Ganti nama / pemetaan cable

1. Edit `cables.toml`.
2. `cd python` lalu `python -m gpcable.codegen --out-root ..` (membuat ulang berkas sumber driver).
3. `tools\build-driver.ps1`
4. `tools\uninstall.ps1`, reboot, lalu `tools\install.ps1`.

## Uninstall

```powershell
Set-ExecutionPolicy -Scope Process Bypass   # wajib di setiap jendela Admin baru
tools\uninstall.ps1
bcdedit /set testsigning off
```

Hapus juga sertifikat uji yang masih tertinggal di Trusted Root dan Trusted Publisher (atau lewat `certmgr.msc`, cari "WDKTestCert"):

```powershell
foreach ($s in 'Root','TrustedPublisher') { Get-ChildItem "Cert:\LocalMachine\$s" | Where-Object Subject -like '*WDKTestCert*' | Remove-Item }
```

Reboot, lalu aktifkan kembali **Secure Boot** di BIOS bila sebelumnya dimatikan. Opsi uninstall: `-PackageDir`, `-DevconPath`.

## Windows tidak mau boot setelah pasang

Boot ke Safe Mode / Startup Settings (tahan Shift saat Restart), lalu jalankan `bcdedit /set testsigning off`, atau pulihkan dengan System Restore (`rstrui`). Di VM cukup kembalikan **snapshot VMware** (ambil snapshot sebelum memasang).

## Troubleshooting

- **Playback tampil "Speakers (GP Virtual Cable)"**: Windows selalu menamai endpoint berkategori *speaker* "Speakers" dan mengabaikan nama dari driver. Sejak fix round 2 kategori Playback adalah Line (`GP_RENDER_BRIDGE_CATEGORY` di `driver/Source/Filters/cabletoptable.h`), jadi nama yang benar "GP Mono 1 (GP Virtual Cable)" dst. Bila masih "Speakers", kemungkinan **nama lama masih tersimpan**:
  - Windows menyimpan data endpoint di `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\{id}`. ID endpoint dibentuk dari reference string filter + ID pin, dan keduanya tidak berubah oleh fix ini. Uninstall driver menghapus setelan pengguna endpoint, tetapi entri endpoint bisa tertinggal (status "not present") dan dipakai lagi saat pasang ulang.
  - **Di VM:** kembalikan **snapshot yang diambil sebelum pemasangan pertama**, lalu pasang paket baru. Ini cara paling bersih (tanpa sisa sertifikat, driver store, maupun endpoint).
  - **Tanpa snapshot (PC utama):** jalankan `tools\uninstall.ps1`, **reboot**, pasang paket baru, lalu reboot lagi. Bila masih "Speakers", ganti nama manual (klik kanan endpoint di `mmsys.cpl` -> Properties). Nama manual juga terbaca oleh meter dan loopback karena pencocokan nama memakai kata utuh. Jangan menghapus kunci MMDevices dengan tangan.
- **Recording tampil "Microphone Array"**: laporkan (nama custom Recording sudah terbukti bekerja di VM).
- **Playback tidak muncul atau disabled**: di `mmsys.cpl` klik kanan -> "Show Disabled Devices" -> Enable, lalu laporkan.
- **GP Mix tidak bisa membuka perangkat**: pastikan Sample rate 48000 Hz; tutup aplikasi lain yang memakai perangkat secara Exclusive.
- **Tidak ada suara di satu channel**: jalankan `python -m gpcable.loopback` dan lihat cable mana yang FAIL.
- **Suara retak**: perbesar buffer size di GP Mix.
- **Slider volume Windows tidak berpengaruh**: keterbatasan v1; atur level dengan fader GP Mix.
- **Audio berproteksi DRM senyap**: memang diblokir Windows pada perangkat virtual.
- **Pasang dua kali membuat perangkat kedua**: jalankan `tools\uninstall.ps1` dulu, baru pasang ulang.

## Lisensi

Kode driver diturunkan dari sampel audio Microsoft (MS-PL); lihat `driver/THIRD_PARTY_NOTICE.md`. Untuk pemakaian pribadi.

## Laporan hasil uji

Isi lalu kirim balik:

- [ ] Nama 12 Playback + 1 Recording di `mmsys.cpl` (tulis persis yang tampil): ...
- [ ] Keluaran `python -m gpcable.loopback` (tempel, termasuk baris `N/12 passed`): ...
- [ ] Hasil `python -m pytest -m e2e` (jumlah passed/failed/skipped): ...
- [ ] BSOD / glitch / retak / freeze (kapan, apa yang sedang dilakukan): ...
- [ ] Driver Verifier + sleep/resume (opsional): lulus / gagal: ...
- [ ] Hasil GP Mix: input "GP Virtual Cable" 48000 Hz terbuka? Discord di 3+4? Spotify di 5+6? Mic di 1? OBS menerima mix? ...
