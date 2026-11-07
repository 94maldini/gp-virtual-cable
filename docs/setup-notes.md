# Catatan Persiapan — GP Virtual Cable

Dicatat: 2026-11-07

## Ruang disk PC (dicek 2026-11-07)

| Drive | Kosong |
|---|---|
| C: | 22,9 GB |
| D: | 0,5 GB |
| E: | 0,9 GB |
| F: | 4 GB |
| Y: | 1,8 GB |

Semua tool harus dipasang di **C:**; drive lain praktis penuh.

## Perkiraan ukuran instalasi (angka kasar)

| Komponen | Kira-kira |
|---|---|
| Visual Studio 2022, komponen C++ minimal | 5–7 GB |
| Windows SDK 10.0.26100 | 2–3 GB |
| WDK 10.0.26100 | 1–2 GB |
| Cache unduhan sementara saat instalasi | 2–4 GB (bisa dihapus) |
| Library Python (numpy, sounddevice, PySide6) | ±0,6 GB |
| **Total setelah instalasi** | **±9–12 GB** (sempat ±15 GB saat proses instal) |

Sisa ruang C: setelah instalasi diperkirakan hanya ±10 GB.

## Cara instal hemat (Visual Studio Installer)

Jangan pilih workload penuh. Di tab **Individual components** cukup centang:

- MSVC v143 – VS 2022 C++ x64/x86 build tools (Latest)
- MSVC v143 – Spectre-mitigated libs (x64/x86) — wajib untuk proyek driver
- Windows 11 SDK (10.0.26100)
- Windows Driver Kit

Di tab **Installation locations**: hilangkan centang **"Keep download cache after installation"**.

Setelah itu pasang **WDK 10.0.26100** dari halaman resmi Microsoft "Download the Windows Driver Kit".

## Lingkungan uji

Pengguna **sudah punya VM Windows 11 di VMware Workstation** → tidak perlu VM baru (tidak makan ruang disk tambahan).

Urutan uji:
1. **VM Windows 11 (VMware)** — uji pertama driver, tes loopback, Driver Verifier. BSOD di sini tidak merusak PC utama.
2. **PC utama Windows 10 19045** — setelah lulus di VM; buat **System Restore point** dulu, lalu uji dengan GP Mix-Lite + OBS.

Driver yang sama jalan di keduanya (INF menargetkan `10.0...19041`, Windows 11 = build 22000+).

Persiapan VM:
- Matikan **Secure Boot di VM**: VM Settings → Options → Advanced → hilangkan centang *Enable secure boot* (VM dalam keadaan mati).
- Kalau VM memakai enkripsi (BitLocker/Device Encryption, vTPM), **suspend BitLocker dulu** atau siapkan recovery key sebelum mengubah pengaturan boot.
- Pastikan **VMware Tools** terpasang (untuk shared folder / drag-drop paket driver dari host ke VM).
- Di dalam VM: `bcdedit /set testsigning on` lalu reboot; pasang Python 3 (±100 MB + numpy/sounddevice) untuk tes loopback.
- Opsional: kernel debugging dari host ke VM (kdnet) untuk menganalisis BSOD.

Secure Boot **PC utama** baru perlu dimatikan saat driver dipasang di PC utama.

## Usulan yang menunggu keputusan

- **Aplikasi meter pakai tkinter** (bawaan Python) sebagai ganti PySide6, hemat ±0,4 GB, tampilan lebih sederhana.

## Yang tetap wajib

- **Secure Boot dimatikan** (di VM, lalu di BIOS PC utama) agar test-signing bisa aktif.
- Instalasi toolchain dan driver butuh admin — dijalankan oleh pengguna.
