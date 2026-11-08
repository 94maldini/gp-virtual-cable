# GP Virtual Cable — Desain

Tanggal: 2026-11-07 · Revisi 2: 2026-11-08 · Status: revisi 2 disetujui (2026-11-08)

## Maksud dan alasan

Pengguna streaming dengan **OBS** dan memakai **GP Mix-Lite** (gassanov.id) sebagai mixer digital di Windows.
GP Mix-Lite di Windows **hanya bisa memilih satu device Input** (dikonfirmasi pengguna 2026-11-08, terlihat di
Setup: *Audio device type* + satu dropdown *Input* + daftar *Active input channels* 1+2, 3+4, …). Saat ini Input-nya
"CABLE Output (VB-Audio Virtual Cable)" dalam mode *Windows Audio (Exclusive Mode)*, 44100 Hz.

VB-Cable tampil 8 channel di GP Mix, tetapi aplikasi Windows (Discord, Spotify, browser, game) selalu mengirim
stereo ke kiri/kanan depan, sehingga semua tercampur di channel 1–2 dan channel 3–8 tidak terpakai. Windows tidak
bisa mengarahkan aplikasi tertentu ke channel tertentu.

PC target: Windows 10 Pro build 19045. Fitur GP Mix untuk menangkap suara aplikasi langsung butuh build ≥ 20348,
jadi di PC ini virtual cable adalah satu-satunya cara memisahkan sumber.

## Tujuan

- Satu driver virtual audio milik sendiri dengan:
  - **12 endpoint Playback** (yang dipilih di aplikasi): "GP Mono 1".."GP Mono 8" (1 ch) dan
    "GP Stereo 1".."GP Stereo 4" (2 ch);
  - **1 endpoint Recording** "GP Virtual Cable" dengan **8 channel** (yang dipilih sebagai Input di GP Mix).
- Pemetaan channel (desain pengguna):

  | Playback | → Input channel di GP Mix |
  |---|---|
  | GP Mono *k* (k = 1..8) | channel *k* |
  | GP Stereo 1 | channel 1 + 2 |
  | GP Stereo 2 | channel 3 + 4 |
  | GP Stereo 3 | channel 5 + 6 |
  | GP Stereo 4 | channel 7 + 8 |

- **Ke-12 Playback selalu ada dan bebas dicampur.** Pengguna memilih per aplikasi, misalnya Mic → GP Mono 1,
  Gitar → GP Mono 2, Discord → GP Stereo 2 (ch 3+4), Spotify → GP Stereo 3 (ch 5+6). Kalau Playback yang memakai
  channel sama dipakai bersamaan (misal GP Mono 1 dan GP Stereo 1), suaranya **dijumlahkan** di channel itu.
- Format 48 kHz / 24-bit.
- **Aplikasi meter** kecil yang menampilkan level tiap Playback.

## Non-tujuan

- Tidak didistribusikan ke orang lain; tidak ada attestation signing Microsoft / sertifikat EV.
- Tidak ada endpoint ASIO, efek, volume per channel, atau routing yang bisa diubah dari GUI. Driver hanya
  menjumlahkan sesuai tabel; mixing sebenarnya tugas GP Mix-Lite.
- Tidak ada resampling di driver (semua endpoint 48 kHz).
- Tidak ada GUI untuk mengubah jumlah/nama/pemetaan (diubah lewat `cables.toml` lalu build ulang).

## Anti-tujuan (gagal walaupun "jalan")

- Driver yang sering BSOD atau membuat audio Windows macet.
- Suara masuk ke channel yang salah, drift/glitch berkala, atau latensi terasa (> ±30 ms tambahan) saat streaming.
- Latensi yang menumpuk (misalnya Playback diputar lama sebelum GP Mix dibuka lalu tertinggal terus).
- Distorsi wrap-around saat dua sumber dijumlahkan dan melewati 0 dBFS (harus clipping/saturasi, bukan wrap).
- Aplikasi meter yang mengganggu GP Mix.

## Batasan

- Driver kernel-mode wajib C++ dengan WDK (PortCls, WaveRT). Python tidak bisa dipakai untuk driver.
- Instalasi lewat **test-signing mode** (`bcdedit /set testsigning on`); **Secure Boot harus dimatikan**.
- Tidak ada Visual Studio di PC (ruang C: terbatas). Driver di-build di **GitHub Actions** (repo private) memakai
  WDK/SDK dari NuGet; pengguna mengunduh artefak hasil build.
- Uji pertama di VM Windows 11 (VMware Workstation), lalu PC utama Windows 10 19045.
- Kode ditulis Claude; pengguna yang build, install, dan tes.
- GP Mix memakai satu device Input. Karena itu **mic/instrumen juga harus lewat cable** kalau GP Mix memakai
  "GP Virtual Cable" sebagai Input (misalnya lewat "Listen to this device" Windows ke GP Mono 1). Dicatat di README;
  bukan bagian driver.

## Keputusan pengguna

- Driver sendiri, pemakaian pribadi.
- Nama bernomor (GP Mono 1..8, GP Stereo 1..4); Recording "GP Virtual Cable".
- Pemetaan channel di atas; semua Playback selalu ada dan bebas dicampur.
- Versi pertama: driver + aplikasi meter (tkinter).

## Desain

### Driver
- Basis: sampel Microsoft `Windows-driver-samples/audio/simpleaudiosample` (turunan SysVAD, MIT).
- Satu device root-enumerated `Root\GPVirtualCable` → 13 endpoint: 12 render + 1 capture.
- Per Playback: ring buffer nonpaged (kapasitas 100 ms) berisi sampel dalam format Playback itu.
  Sisi render (`ReadBytes` di sampel) menyalin data DMA ke ring-nya.
- Sisi capture (`WriteBytes` di sampel): untuk tiap blok, baca jumlah frame yang sama dari ke-12 ring (kosong →
  silence), **jumlahkan** ke channel tujuan sesuai tabel dengan saturasi ke rentang 24-bit, lalu tulis interleaved
  8 channel ke buffer DMA dalam format capture yang dipakai.
- **Kontrol latensi:** sebelum membaca, ring yang isinya lebih dari batas (±20 ms) dibuang bagian tertuanya sampai
  batas, supaya latensi tidak menumpuk.
- Semua stream memakai clock sistem yang sama → tidak ada drift.
- Format Playback: 48000 Hz, 24-bit, 1 atau 2 channel (satu format).
  Format Recording: 48000 Hz, 8 channel, ditawarkan dalam 16-bit, 24-bit, dan 32-bit (24 valid bit) supaya GP Mix
  mode Exclusive pasti menemukan format yang cocok.
- Konfigurasi di `cables.toml` (sumber tunggal): `capture_channels = 8` dan per Playback `first_channel`.
  Generator Python membuat tabel minipair C, tabel pemetaan channel, dan INX.

### Instalasi
- Skrip PowerShell: `enable-testsigning.ps1`, `install.ps1` (devcon), `uninstall.ps1`.
- Di GP Mix: Input = "GP Virtual Cable", **Sample rate 48000 Hz**, aktifkan input channel 1+2 … 7+8.

### Aplikasi meter
- Karena GP Mix membuka Recording secara Exclusive, meter **tidak membuka stream audio**. Meter membaca peak level
  tiap Playback lewat Windows Core Audio `IAudioMeterInformation` (seperti bar level di Sound Control Panel),
  memakai paket kecil `pycaw` (+ `comtypes`).
- tkinter; status per Playback: ada sinyal / diam / tidak ditemukan.
- Logika level (dBFS, peak hold) di modul terpisah, di-unit-test.

### Tes end-to-end
- Per Playback: putar tone, rekam 8 channel Recording, cek tone muncul **hanya** di channel tujuannya
  (channel lain < −90 dBFS).
- Tes campur: GP Mono 1 dan GP Stereo 1 bersamaan → channel 1 berisi kedua tone.
- Tes latensi tidak menumpuk: putar ke Playback 2 detik sebelum mulai merekam; latensi terukur tetap ≤ 30 ms.
- Driver Verifier di VM, streaming ±1 jam tanpa BSOD.

## Diserahkan ke builder (keputusan Claude)

- Basis sampel Microsoft, bukan fork pihak ketiga.
- Kapasitas ring 100 ms, batas latensi ±20 ms.
- Channel mask Recording: 7.1 surround (seperti VB-Cable 8 channel); Playback: mono / stereo.
- INF sampel menargetkan Windows 11 (`10.0...22000`); diturunkan ke `10.0...19041`.
- Meter lewat `IAudioMeterInformation` (`pycaw`), bukan stream audio.
- Detail struktur kode di dalam proyek driver mengikuti sampel.
