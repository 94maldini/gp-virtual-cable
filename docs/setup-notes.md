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

## Konsekuensi untuk rencana (usulan, menunggu keputusan)

1. **VM Hyper-V tidak muat** (Windows 10 di VM butuh 20 GB lebih).
   Usulan: tes langsung di PC ini setelah membuat **System Restore point**, atau pakai PC/laptop lain bila ada.
2. **Aplikasi meter pakai tkinter** (bawaan Python) sebagai ganti PySide6, hemat ±0,4 GB, tampilan lebih sederhana.

## Yang tetap wajib

- **Secure Boot dimatikan** di BIOS agar test-signing bisa aktif.
- Instalasi toolchain dan driver butuh admin — dijalankan oleh pengguna.
