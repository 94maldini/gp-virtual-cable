# GP Virtual Cable — Implementation Plan (revisi 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Driver virtual audio pribadi untuk Windows 10/11 dengan 12 Playback (GP Mono 1–8, GP Stereo 1–4) yang dijumlahkan ke satu Recording 8 channel "GP Virtual Cable" untuk GP Mix-Lite, di-build di GitHub Actions, plus tes loopback dan aplikasi meter Python.

**Architecture:** Fork `audio/simpleaudiosample` Microsoft (PortCls WaveRT). Satu device `Root\GPVirtualCable`, 13 endpoint. Render stream menyalin DMA ke ring per Playback; capture stream membaca ke-12 ring, menjumlahkan ke channel tujuan dengan saturasi 24-bit, lalu menulis 8 channel interleaved. `cables.toml` adalah sumber tunggal; generator Python membuat tabel C dan INX. Build di GitHub Actions memakai WDK/SDK dari NuGet (pola repo Windows-driver-samples).

**Tech Stack:** C++ / WDK 10.0.28000 (NuGet), MSBuild, GitHub Actions `windows-2025-vs2026`, PowerShell 5.1, Python 3.13 (`tomllib`, `numpy`, `sounddevice`, `pycaw`, `tkinter`, `pytest`).

**Spec:** `docs/superpowers/specs/2026-11-07-gp-virtual-cable-design.md` (revisi 2)

**Menggantikan:** `docs/superpowers/plans/2026-11-07-gp-virtual-cable.md`. Kode yang sudah ada dari rencana lama (config/codegen, devices/loopback, levels, meter, skrip install) **diubah**, bukan ditulis ulang dari nol.

## Global Constraints

- Target OS: Windows 10 x64 build 19041+ dan Windows 11. INF `NT$ARCH$.10.0...19041`. Proyek driver: `TargetVersion` Windows10, `_NT_TARGET_VERSION=0x0A000008` (NTDDI_WIN10_VB, 19041). Hanya API yang tersedia di 19041.
- Playback: `Mono1`..`Mono8` (1 ch, "GP Mono 1".."GP Mono 8", `first_channel` 1..8), lalu `Stereo1`..`Stereo4` (2 ch, "GP Stereo 1".."GP Stereo 4", `first_channel` 1, 3, 5, 7). Indeks 0..11 dalam urutan itu. `first_channel` di config 1-based; di C 0-based.
- Recording: satu endpoint, nama "GP Virtual Cable", 8 channel, channel mask `KSAUDIO_SPEAKER_7POINT1_SURROUND`.
- Format Playback: PCM 48000 Hz, 24-bit packed (`wBitsPerSample=24`, `nBlockAlign=3*ch`), mask `KSAUDIO_SPEAKER_MONO` / `KSAUDIO_SPEAKER_STEREO`. Format Recording: 48000 Hz, 8 ch, tiga pilihan: 16-bit, 24-bit packed, 32-bit container dengan 24 valid bit.
- Penjumlahan: akumulator `LONG` per sampel, saturasi ke `[-8388608, 8388607]` sebelum konversi ke format keluar. Tidak boleh wrap-around.
- Ring per Playback: kapasitas 100 ms (`48000 * 3 * ch / 10` byte); batas latensi 20 ms (`48000 * 3 * ch / 50` byte) — sebelum capture membaca, isi di atas batas dibuang dari yang tertua. Tidak ada resampling.
- Hardware ID `Root\GPVirtualCable`; binary `GPVirtualCable.sys`; INF `GPVirtualCable.inf`.
- Build di GitHub Actions (tidak ada Visual Studio lokal). Artefak: paket driver (`.sys`, `.inf`, `.cat`), `package.cer`, `devcon.exe`.
- Meter tidak membuka stream audio; level dari `IAudioMeterInformation` pada 12 endpoint Playback.
- Uji driver: VM Windows 11 (VMware Workstation) dulu, lalu PC utama Windows 10 19045 setelah System Restore point.
- Commit di branch `feat/gp-virtual-cable`; pesan commit diakhiri baris trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Dua Playback yang berbagi channel (GP Mono 1 + GP Stereo 1) diputar bersamaan dengan level tinggi → hasil di channel 1 harus jenuh di batas, bukan wrap. Diuji di Task 2 (`mix_saturates_positive`/`negative`) dan Task 5 (`test_shared_channel_mixes`).
2. Aplikasi memutar ke Playback beberapa detik sebelum GP Mix membuka Recording → latensi tidak boleh tertinggal 100 ms. Diuji di Task 2 (`discard_to_keeps_newest`) dan Task 5 (`test_latency_does_not_accumulate`).
3. GP Mix Exclusive Mode memilih format 16 atau 32-bit → channel tetap benar dan level sama. Diuji di Task 2 (`write16`/`write32`) dan Task 5 (`test_routes_in_all_capture_formats`, sejauh PortAudio bisa memaksa format).
4. Playback stereo yang dimainkan aplikasi mono/44.1 kHz → tetap muncul di kedua channel tujuannya (konversi audio engine). Diuji di Task 5 (`test_stereo_cable_accepts_mono_44k`).
5. Konfigurasi `first_channel` yang melewati channel 8 → ditolak di config, dan mixer tidak menulis di luar buffer. Diuji di Task 3 (`test_route_out_of_range_rejected`) dan Task 2 (`mix_rejects_out_of_range`).

---

### Task 1: Baseline sampel + build di GitHub Actions

**Prasyarat:** pengguna sudah membuat repo **private** di GitHub (akun `94maldini`) dan memberi URL-nya; controller menambahkan remote `origin` dan push branch `feat/gp-virtual-cable`. Push adalah aksi keluar yang sudah disetujui pengguna untuk repo itu.

**Files:**
- Create: `driver/` (salinan `Windows-driver-samples/audio/simpleaudiosample`), `driver/THIRD_PARTY_NOTICE.md` (atribusi + lisensi MIT Microsoft)
- Create: `driver/packages.config` dan `driver/Directory.Build.props` — pola persis dari root repo Windows-driver-samples, **hanya x64**: `Microsoft.Windows.SDK.CPP`, `Microsoft.Windows.SDK.CPP.x64`, `Microsoft.Windows.WDK.x64`, versi `10.0.28000.2526`.
- Modify: INX sampel `22000` → `19041`; properti proyek `TargetVersion`/`_NT_TARGET_VERSION` sesuai Global Constraints.
- Create: `.github/workflows/build-driver.yml`

**Interfaces:**
- Produces: workflow `build-driver` (trigger: `push` ke branch mana pun + `workflow_dispatch`), job di `windows-2025-vs2026`: checkout → `nuget restore driver\packages.config -PackagesDirectory driver\packages` → `msbuild driver\<sln> /p:Configuration=Debug /p:Platform=x64` → upload artefak `gp-virtual-cable-driver` berisi folder `package\` + `package.cer` + `devcon.exe`. Task 2 menambah job tes; Task 4 mengganti nama solusi.
- `devcon.exe`: ambil dari paket NuGet WDK bila ada; kalau tidak ada, build sampel `setup/devcon` dari Windows-driver-samples di job yang sama.

- [ ] **Step 1:** Salin sampel, tambah file NuGet + notice, ubah target OS.
- [ ] **Step 2:** Tulis workflow.
- [ ] **Step 3:** Push dan jalankan — Expected: job hijau; artefak berisi `SimpleAudioSample.sys`, `.inf`, `.cat` (bertanda tangan test cert), `package.cer`, `devcon.exe`. Cek di log bahwa `_NT_TARGET_VERSION` = `0xA000008`.
- [ ] **Step 4:** Commit — `chore: impor simpleaudiosample + build GitHub Actions (WDK NuGet, target Win10 19041)`

### Task 2: Ring buffer + mixer (logika murni, dites user-mode di CI)

**Files:**
- Create: `driver/Source/Utilities/gpring.h`, `driver/Source/Utilities/gpmix.h` (header-only; `RtlCopyMemory`/`RtlZeroMemory` bila `_KERNEL_MODE`, `memcpy`/`memset` bila tidak)
- Create: `driver/tests/gpcore_test.cpp`, `tools/test-gpcore.ps1` (compile `cl /nologo /W4 /WX /EHsc /I driver\Source\Utilities` lalu jalankan; dipakai CI)
- Modify: `.github/workflows/build-driver.yml` — job `core-tests` (Developer shell via `vswhere` + `Launch-VsDevShell.ps1`, lalu `tools\test-gpcore.ps1`); job build `needs: core-tests`.

**Interfaces:**
- Produces (`gpring.h`):
  ```cpp
  struct GpRing { BYTE* Data; ULONG Capacity; ULONG ReadPos; ULONG Fill; };
  void  GpRingInit(GpRing* r, BYTE* storage, ULONG capacity);
  void  GpRingReset(GpRing* r);
  void  GpRingWrite(GpRing* r, const BYTE* src, ULONG n);   // penuh → buang tertua; n > Capacity → simpan Capacity byte terakhir
  ULONG GpRingRead(GpRing* r, BYTE* dst, ULONG n);          // salin min(n, Fill), sisa diisi 0; return byte tersalin
  void  GpRingDiscardTo(GpRing* r, ULONG maxFill);          // Fill > maxFill → buang tertua sampai Fill == maxFill
  ULONG GpRingFill(const GpRing* r);
  ```
- Produces (`gpmix.h`; sampel 24-bit packed little-endian bertanda):
  ```cpp
  LONG    GpRead24(const BYTE* p);
  BOOLEAN GpMixAccumulate(LONG* acc, ULONG frames, ULONG accChannels,
                          const BYTE* src24, ULONG srcChannels, ULONG firstChannel); // acc[f*accChannels + firstChannel + c] += sampel; FALSE (tanpa menulis) bila firstChannel + srcChannels > accChannels
  void    GpMixWrite(const LONG* acc, ULONG frames, ULONG channels, BYTE* dst, ULONG bitsPerSample); // saturasi ke 24-bit; 16 → v>>8; 24 → packed; 32 → v<<8; bits lain → isi nol
  ```

- [ ] **Step 1: Tulis test gagal** di `gpcore_test.cpp` (makro `CHECK`, cetak `N/M passed`, exit 1 bila gagal):
  - ring: `write_then_read_roundtrip`, `underrun_zero_fills` (baca 5 setelah tulis `{9,9}` → `{9,9,0,0,0}`, return 2), `wraparound` (cap 4: tulis `{1,2,3}`, baca 2, tulis `{4,5,6}`, baca 4 → `{3,4,5,6}`), `overflow_keeps_newest` (cap 4, tulis `{1..6}` → baca `{3,4,5,6}`), `reset_clears`, `discard_to_keeps_newest` (tulis `{1..8}` cap 8, `DiscardTo(3)` → Fill 3, baca `{6,7,8}`), `discard_to_noop_when_below`.
  - mix: `read24_sign` (`{0xFF,0xFF,0xFF}` → −1, `{0x00,0x00,0x80}` → −8388608, `{0xFF,0xFF,0x7F}` → 8388607); `mono_into_channel_3` (acc 8 ch, 2 frame, sumber 1 ch nilai 100/200, first 2 → hanya `acc[2]`, `acc[10]` terisi); `stereo_into_5_6`; `two_sources_sum` (mono 1000 di ch 0 + stereo 500/−500 di ch 0–1 → `acc[0]==1500`, `acc[1]==−500`); `mix_saturates_positive` (2 × 8388607 → tulis 24-bit = 8388607); `mix_saturates_negative`; `mix_rejects_out_of_range` (stereo first 7 di acc 8 ch → FALSE, acc tidak berubah); `write16` (0x123456 → `0x1234`); `write24`; `write32` (0x123456 → `0x12345600`).
- [ ] **Step 2: Push, pastikan job `core-tests` merah** (header belum ada) — catat link run.
- [ ] **Step 3: Implementasikan** `gpring.h` dan `gpmix.h`.
- [ ] **Step 4: Push, pastikan `core-tests` hijau** — Expected log: `N/N passed`; job build tetap hijau.
- [ ] **Step 5: Commit** — `feat(driver): ring buffer + mixer penjumlah 8 channel, dites di CI`

### Task 3: Config + generator revisi 2 (Python, ubah kode yang ada)

**Files:**
- Modify: `cables.toml`, `python/gpcable/config.py`, `python/gpcable/codegen.py`, `python/gpcable/templates/GPVirtualCable.inx.in`
- Modify: `python/tests/test_config.py`, `python/tests/test_codegen.py`

**Interfaces:**
- `cables.toml`: top-level `sample_rate = 48000`, `bits = 24`, `capture_channels = 8`, `capture_name = "GP Virtual Cable"`; tiap `[[cable]]` menambah `first_channel` (1-based).
- Produces (`config.py`): `Cable(index, id, name, channels, first_channel)`; `CableConfig(sample_rate, bits, capture_channels, capture_name, cables)`. `load_config` menolak (`ValueError`) tambahan: `sample_rate != 48000`, `bits != 24`, `capture_channels` di luar 1..32, `first_channel < 1`, `first_channel + channels - 1 > capture_channels`, `capture_name` kosong. Overlap channel antar cable **diizinkan**.
- Produces (`codegen.py`, nama fungsi tetap `render_header`, `render_inx`, `inf_escape`, CLI `--config`/`--out-root`):
  - Header: `#define GP_CABLE_COUNT 12`, `#define GP_CAPTURE_CHANNELS 8`, `#define GP_CAPTURE_CABLE_INDEX ((ULONG)-1)`, `typedef struct _GP_CABLE_ROUTE { ULONG Channels; ULONG FirstChannel; } GP_CABLE_ROUTE;` dan `static const GP_CABLE_ROUTE g_GpCableRoutes[GP_CABLE_COUNT]` (FirstChannel **0-based**); 12 minipair render (`L"WaveRender<id>"`, `L"TopoRender<id>"`, descriptor `CableRenderWaveDescriptorMono|Stereo`, format `CableRenderFormatsMono|Stereo`, `CableIndex = index`); **satu** minipair capture (`L"WaveCapture"`, `L"TopoCapture"`, `CreateMicArrayMiniportTopology`, `MicArray1TopoMiniportFilterDescriptor`, `MicArray1TopologyPhysicalConnections`, descriptor `CableCaptureWaveDescriptor`, format `CableCaptureFormats`, `DeviceMaxChannels = GP_CAPTURE_CHANNELS`, `CableIndex = GP_CAPTURE_CABLE_INDEX`); `g_RenderEndpoints[]` (12, urutan indeks) dan `g_CaptureEndpoints[]` (1).
  - INX: 12 section interface render + 1 capture; string `GP_<id>_Name` per cable dan `GP_Capture_Name`; `KSNAME_<KsName>` untuk 26 nama KS.
  - `render_inx` melempar `ValueError` bila salah satu placeholder tidak ada di template (deferred minor dari Task 3 lama).

- [ ] **Step 1: Ubah test dulu**:
  ```python
  def test_load_repo_config():
      cfg = load_config(default_config_path())
      assert [c.first_channel for c in cfg.cables] == [1,2,3,4,5,6,7,8, 1,3,5,7]
      assert [c.channels for c in cfg.cables] == [1]*8 + [2]*4
      assert (cfg.capture_channels, cfg.capture_name) == (8, "GP Virtual Cable")
  def test_route_out_of_range_rejected(tmp_path): ...   # Stereo first_channel=8 → ValueError
  def test_format_must_be_48k_24bit(tmp_path): ...      # sample_rate=44100 → ValueError; bits=16 → ValueError
  def test_overlap_allowed(): ...                       # repo config memuat overlap; tidak error
  def test_header_routes_zero_based():
      h = render_header(load_config(default_config_path()))
      assert "{ 1, 0 }" in h and "{ 1, 7 }" in h and "{ 2, 6 }" in h
      assert h.count("L\"WaveRender") == 12 and h.count('L"WaveCapture"') == 1
      assert h.count("CableCaptureWaveDescriptor") == 1
  def test_inx_one_capture_twelve_render():
      inx = render_inx(load_config(default_config_path()), TEMPLATE)
      assert inx.count("AddInterface=%KSCATEGORY_RENDER%") == 12
      assert inx.count("AddInterface=%KSCATEGORY_CAPTURE%") == 1
      assert 'GP_Capture_Name="GP Virtual Cable"' in inx
  def test_render_inx_missing_placeholder_raises(): ...
  ```
  Hapus/ubah test lama yang mengharapkan 12 capture.
- [ ] **Step 2: Jalankan, pastikan gagal** — `cd python; .venv\Scripts\python.exe -m pytest -q -W error tests/test_config.py tests/test_codegen.py`
- [ ] **Step 3: Implementasikan perubahan.**
- [ ] **Step 4: Full suite hijau** — `.venv\Scripts\python.exe -m pytest -q -W error`
- [ ] **Step 5: Commit** — `feat: config revisi 2 (12 Playback → 1 Recording 8 ch) + generator`

### Task 4: Driver GPVirtualCable revisi 2

**Files:** (semua di `driver/`)
- Rename solusi/target ke `GPVirtualCable`; INX menjadi hasil generator (`python -m gpcable.codegen --out-root .`) dari template yang dibangun dari INX sampel (placeholder dipertahankan). Workflow: langkah codegen dijalankan di CI sebelum msbuild **atau** file hasil generator di-commit — pilih commit file hasil generator + job CI yang memeriksa `git diff --exit-code` setelah menjalankan codegen.
- Modify `Source/Inc/common.h`: `eDeviceType { eCableRenderDevice = 0, eCableCaptureDevice, eMaxDeviceType }`; `ULONG CableIndex;` sebagai field terakhir `ENDPOINT_MINIPAIR`.
- Modify `Source/Main/minwavert.h`: `IsRenderDevice()` → `eCableRenderDevice`; `ULONG GetCableIndex()`.
- Create `Source/Filters/cablewavtable.h`: `CableRenderWaveDescriptorMono|Stereo`, `CableRenderFormatsMono|Stereo` (48 kHz/24-bit, mono/stereo mask), `CableCaptureWaveDescriptor`, `CableCaptureFormats` (48 kHz, 8 ch, 16/24/32(24 valid), mask 7.1 surround), physical connection tables dipindah dari `minipairs.h`.
- Modify `Source/Filters/minipairs.h`: include `cablewavtable.h` + `cables_generated.h`.
- Create `Source/Utilities/CableBuffer.h/.cpp`:
  ```cpp
  NTSTATUS GpCablesInitialize();   // ring NonPaged (ExAllocatePool2, tag 'cbPG') per cable, kapasitas 100 ms
  VOID     GpCablesCleanup();
  VOID     GpCableWrite(ULONG cableIndex, const BYTE* src, ULONG n);   // spinlock per cable; indeks invalid → abaikan
  ULONG    GpCableReadFrames(ULONG cableIndex, BYTE* dst, ULONG frames); // DiscardTo(batas 20 ms) lalu baca frames*3*ch byte; return frame tersalin
  VOID     GpCableReset(ULONG cableIndex);
  ```
- Modify `Source/Main/adapter.cpp`: `GpCablesInitialize()` di `DriverEntry`, `GpCablesCleanup()` di `DriverUnload`.
- Modify `Source/Main/minwavertstream.cpp/.h`:
  - render `ReadBytes`: ganti SaveData dengan `GpCableWrite(GetCableIndex(), ...)`; render masuk `KSSTATE_RUN` → `GpCableReset(index)`.
  - capture `WriteBytes`: ganti ToneGenerator. Per potongan ≤ 480 frame: nolkan akumulator `LONG[480*8]`; untuk tiap cable `GpCableReadFrames` ke buffer scratch lalu `GpMixAccumulate(acc, frames, 8, scratch, route.Channels, route.FirstChannel)`; `GpMixWrite(acc, frames, 8, dmaPtr, bitsPerSample format stream)`; tangani wrap buffer DMA. Akumulator dan scratch dialokasikan NonPaged saat stream capture dibuat, dibebaskan saat ditutup; tidak ada alokasi di DPC.
  - Hapus inisialisasi ToneGenerator/SaveData.
- Modify `.github/workflows/build-driver.yml`: nama solusi baru + Code Analysis (`/p:RunCodeAnalysis=true`) + `infverif /w` pada INF hasil build; artefak berisi `GPVirtualCable.sys/.inf/.cat`.

- [ ] **Step 1:** Jalankan codegen, terapkan semua perubahan.
- [ ] **Step 2:** Push — Expected: `core-tests`, `codegen-check`, build (0 warning, Code Analysis bersih), `infverif` tanpa error; artefak berisi `GPVirtualCable.sys/.inf/.cat`, `package.cer`, `devcon.exe`.
- [ ] **Step 3:** Commit — `feat(driver): 12 Playback dijumlahkan ke Recording 8 channel`

### Task 5: Devices + tes loopback revisi 2 (Python, ubah kode yang ada)

**Files:**
- Modify: `python/gpcable/devices.py`, `python/gpcable/loopback.py`, `python/tests/test_devices.py`, `python/tests/test_loopback_e2e.py`, `tools/install.ps1` (pesan selesai menyebut setelan GP Mix: Input "GP Virtual Cable", 48000 Hz)

**Interfaces:**
- Produces (`devices.py`): `@dataclass class CableDeviceMap: outputs: dict[str, int | None]; capture: int | None`; `find_cable_devices(devices, hostapis, cfg) -> CableDeviceMap` — hanya host API "Windows WASAPI"; output = device dengan `max_output_channels > 0` yang namanya memuat nama cable sebagai kata utuh ("GP Mono 1" tidak cocok dengan "GP Mono 10"); capture = device input yang namanya memuat `cfg.capture_name`.
- Produces (`loopback.py`): `record_route(out_idx: int | None, cap_idx: int, tones: Sequence[tuple[int, float]], channels_out: int, play_rate: int = 48000, seconds: float = 1.0, preroll_s: float = 0.0, capture_dtype: str = "float32") -> np.ndarray` (frames × 8, setelah buang 200 ms awal; `tones` = daftar `(channel_out_index, freq_hz)` yang dimainkan); `channel_peak(rec, ch) -> tuple[float, float]` (frekuensi puncak Hann+rFFT, RMS dBFS); `measure_latency_ms(out_idx, cap_idx, channels_out, first_channel) -> float` (burst di output, waktu dari `outputBufferDacTime`/`inputBufferAdcTime` callback sounddevice, deteksi onset > −40 dBFS di channel tujuan); CLI `python -m gpcable.loopback` → `PASS/FAIL` per cable + `N/12 passed`. Frekuensi cable `i`: `300 + 150*i` Hz.

- [ ] **Step 1: Ubah test unit devices** — `test_finds_twelve_outputs_and_one_capture`, `test_mono_1_does_not_match_mono_10`, `test_ignores_non_wasapi`, `test_missing_capture_is_none`. Jalankan → gagal → implementasi → hijau.
- [ ] **Step 2: Ubah test e2e** (skip bila `capture is None`):
  ```python
  def test_each_cable_lands_only_on_its_channels(cfg, devmap):   # per cable: channel tujuan |peak-f|<5 Hz & > -20 dBFS; channel lain < -90 dBFS
  def test_shared_channel_mixes(cfg, devmap):                   # Mono1 (f=300) + Stereo1 (f=1500) bersamaan → channel 0 memuat dua puncak (keduanya > -30 dBFS)
  def test_latency_does_not_accumulate(cfg, devmap):            # preroll 2 s ke Stereo2, lalu measure_latency_ms <= 30
  def test_stereo_cable_accepts_mono_44k(cfg, devmap):          # play_rate=44100, channels_out=1 ke Stereo3 → tone di channel 4 dan 5
  def test_routes_in_all_capture_formats(cfg, devmap):          # capture_dtype int16 / int32 → routing Stereo4 tetap benar (skip format yang ditolak PortAudio)
  ```
  lalu ubah `loopback.py`.
- [ ] **Step 3: Full suite hijau** (e2e di-skip dengan alasan jelas, `-W error`).
- [ ] **Step 4: Commit** — `feat: tes loopback revisi 2 (routing 8 channel, campur, latensi)`

### Task 6: Meter revisi 2 — level dari Windows, tanpa stream audio

**Files:**
- Create: `python/gpcable/meters.py`
- Modify: `python/gpcable/levels.py`, `python/gpcable/meter.py`, `python/pyproject.toml` (tambah dependensi `pycaw`), `python/tests/test_levels.py`, `python/tests/test_meter.py`

**Interfaces:**
- Produces (`levels.py`, tambahan): `linear_to_dbfs(x: float) -> float` (≤ 0 → `FLOOR_DB`, 1.0 → 0.0, 0.5 → −6.02, tidak pernah di bawah `FLOOR_DB`, return `float`). Sekalian perbaiki deferred minor lama: `PeakHold.update` tidak pernah turun di bawah `FLOOR_DB`.
- Produces (`meters.py`): `class RenderMeter: name: str; def peak(self) -> float` (linear 0..1, `IAudioMeterInformation.GetPeakValue`); `open_render_meters(names: Sequence[str]) -> dict[str, RenderMeter | None]` — enumerasi endpoint render aktif lewat pycaw/Core Audio, cocokkan nama kata utuh seperti `devices.py`; error COM untuk satu endpoint → `None` untuk endpoint itu.
- `meter.py`: satu baris per Playback (urutan `cables.toml`), label channel tujuan ("ch 3" / "ch 3+4"), bar −60..0 dBFS, angka peak-hold, status `signal` / `silent` / `tidak ditemukan` dari peak saat itu (`row_status`, `bar_fraction`, `plan_*` tetap pure dan dites). Poll `root.after(33, ...)`; tanpa `sounddevice`. "Scan ulang" memanggil ulang `open_render_meters`; error ditampilkan di label merah. Hapus kode stream lama.

- [ ] **Step 1: Test gagal** — `test_linear_to_dbfs_values`, `test_peak_hold_never_below_floor`, `test_meter_rows_show_channel_labels` (pure: dari config → `["ch 1", …, "ch 1+2", "ch 3+4", "ch 5+6", "ch 7+8"]`).
- [ ] **Step 2: Jalankan → gagal; implementasikan; jalankan → hijau** (`-W error`).
- [ ] **Step 3: Cek peluncuran** — `.venv\Scripts\python.exe -m gpcable.meter --exit-after 3` → exit 0; 12 baris `tidak ditemukan` (driver belum ada); tanpa traceback.
- [ ] **Step 4: Commit** — `feat(meter): level dari IAudioMeterInformation, tanpa stream audio`

### Task 7: Uji di VM + PC utama, README

**Files:**
- Create: `README.md` (bahasa Indonesia: apa ini, pemetaan channel, unduh artefak dari GitHub Actions, test-signing + Secure Boot (VM dan PC), install/uninstall + salin `devcon.exe`, setelan GP Mix (Input "GP Virtual Cable", 48000 Hz, aktifkan input 1+2 … 7+8, mode Exclusive/Shared), memasukkan mic lewat "Listen to this device" → GP Mono 1, meter, ganti nama lewat `cables.toml` → codegen → push → unduh ulang, troubleshooting)

- [ ] **Step 1: Pengguna di VM Windows 11** (admin): unduh artefak, `enable-testsigning.ps1` → reboot → `install.ps1`. Verifikasi: `mmsys.cpl` menampilkan 12 Playback "GP Mono 1".."GP Stereo 4" dan 1 Recording "GP Virtual Cable" 8 channel.
- [ ] **Step 2: Pengguna menjalankan** `python -m gpcable.loopback` dan `pytest -m e2e` di VM — Expected `12/12 passed`, semua e2e PASS. Hasil dikirim ke controller.
- [ ] **Step 3: Driver Verifier di VM** — `verifier /standard /driver GPVirtualCable.sys` → reboot → putar ke 4 Playback + meter ±1 jam, sekali sleep/resume → tanpa BSOD; loopback tetap 12/12; `verifier /reset`.
- [ ] **Step 4: PC utama** — Restore point, Secure Boot off, test-signing, install; GP Mix Input "GP Virtual Cable" 48000 Hz; Discord → GP Stereo 2 terlihat di input 3+4, Spotify → GP Stereo 3 di 5+6, mic (Listen) → GP Mono 1 di input 1; mix keluar ke OBS.
- [ ] **Step 5: Tulis README, commit** — `docs: README instalasi dan setup GP Mix-Lite + OBS`
