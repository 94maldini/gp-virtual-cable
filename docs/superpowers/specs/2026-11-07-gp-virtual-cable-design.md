# GP Virtual Cable — Desain

Tanggal: 2026-11-07 · Status: disetujui

## Maksud dan alasan

Pengguna streaming dengan **OBS** dan memakai **GP Mix-Lite** (gassanov.id) sebagai mixer digital di Windows.
Di Windows, GP Mix-Lite memakai ASIO untuk audio interface dan **WASAPI** untuk sumber lain, dan *tidak punya virtual
cable bawaan*. VB-Cable gratis yang dipakai sekarang hanya memberi 1 stereo / 2 mono — tidak cukup untuk memisahkan
mic, game, musik, Discord, browser, dll. ke channel GP Mix yang berbeda.

PC target: Windows 10 Pro build 19045. Fitur GP Mix untuk menangkap suara aplikasi langsung butuh build ≥ 20348,
jadi di PC ini virtual cable adalah satu-satunya cara memisahkan sumber.

## Tujuan

- Satu driver virtual audio milik sendiri yang memunculkan **12 cable**:
  **8 mono** ("GP Mono 1".."GP Mono 8") dan **4 stereo** ("GP Stereo 1".."GP Stereo 4").
- Tiap cable = 1 endpoint **Playback** + 1 endpoint **Recording**; audio yang diputar ke Playback keluar dari Recording
  pasangannya (loopback), dapat dibuka GP Mix-Lite lewat WASAPI.
- Format 48 kHz / 24-bit.
- **Aplikasi meter** kecil yang menampilkan level tiap cable.

## Non-tujuan

- Tidak didistribusikan ke orang lain; tidak ada attestation signing Microsoft / sertifikat EV.
- Tidak ada endpoint ASIO, mixing, efek, atau routing di dalam driver — itu tugas GP Mix-Lite.
- Tidak ada GUI untuk mengubah jumlah/nama cable (nama diubah lewat INF).

## Anti-tujuan (gagal walaupun "jalan")

- Driver yang sering BSOD atau membuat audio Windows macet.
- Suara bocor antar cable, drift/glitch berkala, atau latensi terasa (> ±30 ms tambahan) saat streaming.
- Aplikasi meter yang mengganggu GP Mix (mengunci endpoint secara exclusive).

## Batasan

- Driver kernel-mode wajib C++ dengan WDK (PortCls, WaveRT). Python tidak bisa dipakai untuk driver.
- Instalasi lewat **test-signing mode** (`bcdedit /set testsigning on`); **Secure Boot harus dimatikan**.
- Toolchain belum terpasang di PC: perlu Visual Studio 2022 (workload *Desktop development with C++*) + Windows SDK +
  WDK versi yang cocok.
- Kode ditulis Claude; pengguna yang build, install, dan tes di PC/VM-nya.

## Keputusan pengguna

- Driver sendiri, pemakaian pribadi.
- Nama bernomor dulu (GP Mono 1..8, GP Stereo 1..4), bisa diganti lewat INF.
- Versi pertama: driver + aplikasi meter.

## Desain

### Driver
- Basis: sampel Microsoft `Windows-driver-samples/audio/simpleaudiosample` (turunan SysVAD, MIT).
  Pembanding: `github.com/virtualdrivers/virtual-audio-driver`.
- Satu device root-enumerated `ROOT\GPVirtualCable` → 24 endpoint (12 render + 12 capture).
- `CableConfig.h`: tabel 12 entri `{index, channels, nama}`; minipair render+capture dibangkitkan dari tabel.
- `CableBuffer`: ring buffer nonpaged per cable, dilindungi spinlock. Di sampel, `ReadBytes` adalah sisi render
  (membaca buffer DMA) → salin ke ring; `WriteBytes` adalah sisi capture (mengisi buffer DMA) → ambil dari ring;
  underrun → silence; overrun → timpa data tertua.
  Render dan capture memakai clock sistem yang sama → tidak ada drift. Kapasitas ring 100 ms.
- Format tunggal per endpoint: 48000 Hz, 24-bit, channel 1 atau 2 (satu format saja supaya render dan capture
  satu cable selalu identik).
- Konfigurasi cable di `cables.toml` (sumber tunggal); generator Python membuat tabel minipair C dan INX.
  Nama endpoint ada di `[Strings]` INF; nama yang tampil di Windows memuat nama cable, misalnya "Speakers (GP Mono 1)".

### Instalasi
- Skrip PowerShell: `enable-testsigning.ps1`, `install.ps1` (devcon dari WDK), `uninstall.ps1`.
- Disarankan tes pertama di VM Hyper-V atau minimal System Restore point.

### Aplikasi meter
- Python 3 + `sounddevice` (WASAPI shared) + `numpy` + `PySide6`.
- Logika level (peak/RMS dBFS, peak hold, decay) di modul terpisah, di-unit-test dengan pytest.
- Status per cable: ada sinyal / diam / endpoint tidak ditemukan.

### Tes end-to-end
- `tools/loopback_test.py`: per cable, putar tone ke Playback, rekam dari Recording, cek puncak FFT, jumlah channel,
  dan tidak ada bocor antar cable. Target 12/12 lulus.
- Driver Verifier di VM, streaming ±1 jam tanpa BSOD.

## Diserahkan ke builder (keputusan Claude)

- Basis sampel Microsoft, bukan fork pihak ketiga.
- Kapasitas ring buffer 100 ms; format 16-bit dihapus (satu format 24-bit saja).
- INF sampel menargetkan Windows 11 (`10.0...22000`); diturunkan ke `10.0...19041` untuk Windows 10.
- `cables.toml` + generator Python sebagai sumber tunggal nama/jumlah cable.
- PySide6 untuk GUI meter.
- Detail struktur kode di dalam proyek driver mengikuti sampel.
