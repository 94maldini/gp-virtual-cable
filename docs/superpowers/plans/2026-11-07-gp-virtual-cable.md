# GP Virtual Cable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Driver virtual audio pribadi untuk Windows 10 yang memunculkan 12 cable loopback (8 mono + 4 stereo) yang bisa dibuka GP Mix-Lite lewat WASAPI, ditambah aplikasi meter Python.

**Architecture:** Fork sampel Microsoft `audio/simpleaudiosample` (PortCls WaveRT). Satu device `Root\GPVirtualCable` mendaftarkan 24 endpoint (render+capture per cable); render stream menyalin data DMA ke ring buffer per cable, capture stream membacanya. Satu file konfigurasi `cables.toml` menjadi sumber kebenaran; generator Python membuat tabel minipair C dan file INX. Paket Python `gpcable` juga berisi tes loopback end-to-end dan aplikasi meter.

**Tech Stack:** C++ / WDK (PortCls, WaveRT), Visual Studio 2022, PowerShell, Python 3.13 (`tomllib`, `numpy`, `sounddevice`, `PySide6`, `pytest`).

**Spec:** `docs/superpowers/specs/2026-11-07-gp-virtual-cable-design.md`

## Global Constraints

- Target OS: Windows 10 x64 build 19045 (dan lebih baru). INF harus `NT$ARCH$.10.0...19041`, bukan `...22000` seperti sampel.
- Format setiap endpoint **tepat satu**: PCM 48000 Hz, 24-bit packed (`wBitsPerSample=24`, `wValidBitsPerSample=24`, `nBlockAlign=3*channels`). Render dan capture satu cable harus identik formatnya agar salinan byte mentah benar.
- Cable: `Mono1`..`Mono8` (1 ch, nama tampil "GP Mono 1".."GP Mono 8"), `Stereo1`..`Stereo4` (2 ch, "GP Stereo 1".."GP Stereo 4"), urutan indeks 0..11 sesuai urutan itu.
- Hardware ID: `Root\GPVirtualCable`; nama driver/binary: `GPVirtualCable.sys`.
- Kapasitas ring buffer per cable: 100 ms (`48000 * 3 * channels / 10` byte).
- Instalasi hanya via test-signing (`bcdedit /set testsigning on`, Secure Boot OFF). Tidak ada signing produksi.
- Aplikasi meter membuka endpoint dalam WASAPI **shared mode** saja.
- Commit di branch `feat/gp-virtual-cable`; pesan commit diakhiri `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Aplikasi memutar stereo 44.1 kHz ke cable mono 48 kHz → harus tetap terdengar di capture (konversi oleh audio engine). Diuji di Task 5 (`test_mono_cable_accepts_stereo_44k`).
2. Capture dibuka tanpa ada yang memutar → harus silence bersih (< −90 dBFS), bukan noise/sisa data. Diuji di Task 2 (`underrun_zero_fills`) dan Task 5 (`test_idle_cable_is_silent`).
3. Render jalan lama tanpa pendengar, lalu capture dibuka → tidak ada audio basi lebih dari 100 ms, tidak ada pertumbuhan memori. Diuji di Task 2 (`overflow_keeps_newest`).
4. Nama cable di `cables.toml` berisi karakter khusus INF (`%`, `"`) → INF tetap valid. Diuji di Task 3 (`test_inf_string_escaping`).
5. Audio bocor antar cable (indeks salah) → tiap cable hanya memuat tone-nya sendiri. Diuji di Task 5 (`test_no_crosstalk`).

---

### Task 1: Toolchain + baseline sampel yang bisa di-build

**Files:**
- Create: `driver/` (salinan `Windows-driver-samples/audio/simpleaudiosample`, termasuk `.sln`, `Package/`, `Source/`)
- Create: `driver/THIRD_PARTY_NOTICE.md` (atribusi + teks lisensi MIT Microsoft)
- Create: `tools/build-driver.ps1`
- Modify: `driver/Source/Main/SimpleAudioSample.inx` (baris `%MfgName%=...` dan header section: `22000` → `19041`)

**Interfaces:**
- Produces: `tools/build-driver.ps1 [-Configuration Debug|Release]` → menjalankan `msbuild driver\SimpleAudioSample.sln /p:Configuration=<cfg> /p:Platform=x64`, exit code ≠ 0 bila gagal. (Task 4 mengganti nama `.sln`; skrip ikut diubah di sana.)

- [ ] **Step 1: Pengguna memasang toolchain** (butuh admin/UAC — minta pengguna menjalankan; jangan jalankan diam-diam). Visual Studio 2022 Community dengan workload *Desktop development with C++*, Windows SDK 10.0.26100, komponen *Windows Driver Kit*, lalu WDK 10.0.26100 (ikuti halaman resmi "Download the WDK" untuk ID winget/komponen terkini).
- [ ] **Step 2: Verifikasi toolchain**
  Run: `& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath; Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.26100.0\km"`
  Expected: path VS 2022 tercetak dan `True`.
- [ ] **Step 3: Salin sampel** dari commit terbaru `main` repo `microsoft/Windows-driver-samples` ke `driver/`, ubah target INX ke `19041`, tulis `THIRD_PARTY_NOTICE.md`.
- [ ] **Step 4: Tulis `tools/build-driver.ps1`** (cari `msbuild` via `vswhere -find MSBuild\**\Bin\MSBuild.exe`).
- [ ] **Step 5: Build baseline**
  Run: `powershell -File tools\build-driver.ps1 -Configuration Debug`
  Expected: `Build succeeded`, 0 error; `driver\x64\Debug\package\` berisi `SimpleAudioSample.sys`, `.inf`, `.cat`.
- [ ] **Step 6: Commit** — `git add driver tools/build-driver.ps1 && git commit -m "chore: impor simpleaudiosample Microsoft sebagai baseline (target Win10 19041)"`

### Task 2: Ring buffer loopback (logika murni, diuji di user-mode)

**Files:**
- Create: `driver/Source/Utilities/gpring.h` (header-only, tanpa dependensi kernel kecuali makro salin/isi)
- Create: `driver/tests/gpring_test.cpp`
- Create: `tools/test-ringbuf.ps1` (compile dengan `cl /nologo /W4 /WX /EHsc` dari Developer environment via `vswhere` + `Launch-VsDevShell.ps1`, lalu jalankan exe)

**Interfaces:**
- Produces (C++, di `gpring.h`):
  ```cpp
  struct GpRing { BYTE* Data; ULONG Capacity; ULONG ReadPos; ULONG Fill; };
  void  GpRingInit(GpRing* r, BYTE* storage, ULONG capacity);
  void  GpRingReset(GpRing* r);                                 // Fill=0, ReadPos=0
  void  GpRingWrite(GpRing* r, const BYTE* src, ULONG n);       // penuh → buang data tertua; n > Capacity → simpan Capacity byte terakhir
  ULONG GpRingRead(GpRing* r, BYTE* dst, ULONG n);              // salin min(n, Fill), sisa dst diisi 0; return byte tersalin
  ULONG GpRingFill(const GpRing* r);
  ```
  Salin/isi memakai `RtlCopyMemory`/`RtlZeroMemory` bila `_KERNEL_MODE` terdefinisi, `memcpy`/`memset` bila tidak. Tidak ada locking di sini (Task 4 yang mengunci).

- [ ] **Step 1: Tulis test gagal** di `driver/tests/gpring_test.cpp` (main sederhana dengan makro `CHECK(cond)` yang mencetak nama test dan return 1 bila gagal), berisi:
  - `write_then_read_roundtrip`: capacity 8, tulis `{1,2,3}`, baca 3 → `{1,2,3}`, return 3, Fill 0.
  - `underrun_zero_fills`: capacity 8, tulis `{9,9}`, baca 5 ke buffer berisi `0xAA` → `{9,9,0,0,0}`, return 2.
  - `wraparound`: capacity 4, tulis `{1,2,3}`, baca 2, tulis `{4,5,6}`, baca 4 → `{3,4,5,6}`.
  - `overflow_keeps_newest`: capacity 4, tulis `{1,2,3,4,5,6}` → Fill 4, baca 4 → `{3,4,5,6}`; lalu tulis `{7}` dua kali ke ring penuh `{a,b,c,d}` → data tertua dibuang.
  - `reset_clears`: tulis 3 byte, `GpRingReset`, `GpRingFill == 0`, baca 2 → `{0,0}`.
- [ ] **Step 2: Jalankan, pastikan gagal** — Run: `powershell -File tools\test-ringbuf.ps1` · Expected: gagal compile (`gpring.h` belum ada).
- [ ] **Step 3: Implementasikan `gpring.h`** sesuai Interfaces.
- [ ] **Step 4: Jalankan, pastikan lulus** — Expected: `5/5 passed`, exit 0.
- [ ] **Step 5: Commit** — `git commit -m "feat(driver): ring buffer loopback + tes user-mode"`

### Task 3: Konfigurasi cable + generator kode (Python)

**Files:**
- Create: `cables.toml`
- Create: `python/pyproject.toml` (paket `gpcable`, Python ≥3.11; deps: `numpy`, `sounddevice`, `PySide6`; dev: `pytest`)
- Create: `python/gpcable/__init__.py`, `python/gpcable/config.py`, `python/gpcable/codegen.py`
- Create: `python/gpcable/templates/GPVirtualCable.inx.in` (dibuat di Task 4 dari INX sampel; di task ini cukup template minimal untuk tes)
- Test: `python/tests/test_config.py`, `python/tests/test_codegen.py`

**Interfaces:**
- `cables.toml`:
  ```toml
  sample_rate = 48000
  bits = 24
  [[cable]]
  id = "Mono1"
  name = "GP Mono 1"
  channels = 1
  # ... Mono2..Mono8, lalu Stereo1..Stereo4 (channels = 2)
  ```
- Produces (`config.py`):
  `@dataclass(frozen=True) class Cable: index: int; id: str; name: str; channels: int`
  `@dataclass(frozen=True) class CableConfig: sample_rate: int; bits: int; cables: tuple[Cable, ...]`
  `load_config(path: Path) -> CableConfig` — `ValueError` bila: id duplikat, id bukan `[A-Za-z][A-Za-z0-9]*`, channels ∉ {1,2}, nama kosong, jumlah cable 0 atau > 32.
  `default_config_path() -> Path` → `<repo>/cables.toml`.
- Produces (`codegen.py`):
  `render_header(cfg: CableConfig) -> str` → isi `driver/Source/Filters/cables_generated.h`.
  `render_inx(cfg: CableConfig, template: str) -> str` → mengganti placeholder `@@INTERFACE_SECTIONS@@`, `@@ADD_INTERFACES@@`, `@@STRINGS@@`.
  `inf_escape(s: str) -> str` → `%`→`%%`, `"`→`""`.
  CLI: `python -m gpcable.codegen [--config PATH]` menulis kedua file ke `driver/` dan mencetak path-nya.
- Nama KS per cable `<id>` (dipakai Task 4, harus persis): `WaveRender<id>`, `TopoRender<id>`, `WaveCapture<id>`, `TopoCapture<id>`. FriendlyName interface topologi dan wave = `name` cable.
- `cables_generated.h` mendefinisikan: `#define GP_CABLE_COUNT <n>`, satu `static ENDPOINT_MINIPAIR` render + satu capture per cable (DeviceType `eCableRenderDevice`/`eCableCaptureDevice`, nama KS di atas, wave descriptor `CableRenderWaveDescriptorMono|Stereo` / `CableCaptureWaveDescriptorMono|Stereo`, topo descriptor `SpeakerTopoMiniportFilterDescriptor` / `MicArray1TopoMiniportFilterDescriptor`, `DeviceMaxChannels = channels`, format table `CableRenderFormatsMono|Stereo` / `CableCaptureFormatsMono|Stereo`, field baru `CableIndex = index`), serta `g_RenderEndpoints[]` dan `g_CaptureEndpoints[]` dalam urutan indeks, plus `static const ULONG g_GpCableChannels[GP_CABLE_COUNT]`.

- [ ] **Step 1: Tulis test gagal**
  ```python
  def test_load_repo_config():
      cfg = load_config(default_config_path())
      assert [c.name for c in cfg.cables][:2] == ["GP Mono 1", "GP Mono 2"]
      assert [c.channels for c in cfg.cables] == [1]*8 + [2]*4
      assert (cfg.sample_rate, cfg.bits) == (48000, 24)

  @pytest.mark.parametrize("bad", [dup_ids, bad_channels_3, id_with_space, empty_name])
  def test_invalid_config_rejected(tmp_path, bad): ...  # pytest.raises(ValueError)

  def test_header_has_all_minipairs():
      h = render_header(load_config(default_config_path()))
      assert "#define GP_CABLE_COUNT 12" in h
      for cid in ["Mono1", "Mono8", "Stereo1", "Stereo4"]:
          for k in ["WaveRender", "TopoRender", "WaveCapture", "TopoCapture"]:
              assert f'L"{k}{cid}"' in h
      assert h.count("CableRenderWaveDescriptorMono") == 8
      assert h.count("CableCaptureWaveDescriptorStereo") == 4

  def test_inx_interfaces_and_names():
      inx = render_inx(load_config(default_config_path()), TEMPLATE)
      assert inx.count("AddInterface=%KSCATEGORY_RENDER%") == 12
      assert inx.count("AddInterface=%KSCATEGORY_CAPTURE%") == 12
      assert 'GP_Stereo4_Name="GP Stereo 4"' in inx   # pola kunci string: GP_<id>_Name
      assert "@@" not in inx

  def test_inf_string_escaping():
      assert inf_escape('50% "Mic"') == '50%% ""Mic""'
  ```
- [ ] **Step 2: Jalankan, pastikan gagal** — Run: `cd python; python -m pip install -e .[dev]; python -m pytest -q` · Expected: ImportError/FAIL.
- [ ] **Step 3: Implementasikan `config.py`, `codegen.py`, `cables.toml`** (12 cable sesuai Global Constraints). Kunci string INF: `GP_<id>_Name`.
- [ ] **Step 4: Jalankan, pastikan lulus** — Expected: semua test PASS.
- [ ] **Step 5: Commit** — `git commit -m "feat: cables.toml + generator tabel minipair dan INX"`

### Task 4: Driver GPVirtualCable dengan 12 cable loopback

**Files:**
- Rename: `SimpleAudioSample.sln` → `GPVirtualCable.sln`; output binary/INF → `GPVirtualCable` (TargetName di `Main.vcxproj` dan `package.VcxProj`); `SimpleAudioSample.inx` → `GPVirtualCable.inx` (sekarang **dihasilkan** dari `python/gpcable/templates/GPVirtualCable.inx.in`, yang dibuat dari INX sampel: section speaker/mic diganti placeholder, hardware ID `Root\GPVirtualCable`, nama produk "GP Virtual Cable").
- Modify: `driver/Source/Inc/common.h` — `eDeviceType { eCableRenderDevice = 0, eCableCaptureDevice, eMaxDeviceType }`; tambah `ULONG CableIndex;` sebagai field terakhir `ENDPOINT_MINIPAIR`. Ganti semua pemakaian `eSpeakerDevice`/`eMicArrayDevice1` (grep di `Source/`).
- Modify: `driver/Source/Main/minwavert.h` — `IsRenderDevice()` membandingkan dengan `eCableRenderDevice`; tambah `ULONG GetCableIndex() { return m_pMiniportPair->CableIndex; }`.
- Create: `driver/Source/Filters/cablewavtable.h` — 4 wave filter descriptor + 4 tabel format (nama sesuai Task 3), diturunkan dari `speakerwavtable.h` (render) dan `micarraywavtable.h` (capture); data range dan device format hanya 48000 Hz / 24-bit / channel tetap.
- Modify: `driver/Source/Filters/minipairs.h` — hapus `SpeakerMiniports`/`MicArray1Miniports` dan array endpoint; `#include "cablewavtable.h"` lalu `#include "cables_generated.h"`.
- Create: `driver/Source/Utilities/CableBuffer.h/.cpp` (tambahkan ke `Utilities.vcxproj`).
- Modify: `driver/Source/Main/adapter.cpp` — panggil `GpCablesInitialize()` di `DriverEntry` (gagal → return status) dan `GpCablesCleanup()` di `DriverUnload`.
- Modify: `driver/Source/Main/minwavertstream.cpp` — `ReadBytes` (sisi render): ganti `m_SaveData.WriteData(...)` dengan `GpCableWrite(m_pMiniport->GetCableIndex(), m_pDmaBuffer + bufferOffset, runWrite)`; `WriteBytes` (sisi capture): ganti `m_ToneGenerator.GenerateSine(...)` dengan `GpCableRead(...)`. Hapus inisialisasi ToneGenerator/SaveData dan pembuatan file data; pada transisi render ke `KSSTATE_RUN` panggil `GpCableReset(index)`.
- Modify: `tools/build-driver.ps1` → `GPVirtualCable.sln`.

**Interfaces:**
- Consumes: `gpring.h` (Task 2); `cables_generated.h` + `GPVirtualCable.inx` hasil `python -m gpcable.codegen` (Task 3).
- Produces (`CableBuffer.h`):
  ```cpp
  NTSTATUS GpCablesInitialize();   // alokasi NonPaged (ExAllocatePool2, tag 'cbPG') ring per cable, kapasitas 48000*3*ch/10
  VOID     GpCablesCleanup();
  VOID     GpCableWrite(ULONG cableIndex, const BYTE* src, ULONG n);  // KSPIN_LOCK per cable; indeks ≥ GP_CABLE_COUNT → abaikan
  VOID     GpCableRead (ULONG cableIndex, BYTE* dst, ULONG n);        // indeks invalid → isi nol
  VOID     GpCableReset(ULONG cableIndex);
  ```
  Dipanggil dari DPC (≤ DISPATCH_LEVEL): pakai `KeAcquireSpinLock`, tanpa alokasi.

- [ ] **Step 1: Jalankan generator** — Run: `cd python; python -m gpcable.codegen` · Expected: mencetak path `cables_generated.h` dan `GPVirtualCable.inx`.
- [ ] **Step 2: Terapkan semua perubahan Files** di atas.
- [ ] **Step 3: Build** — Run: `powershell -File tools\build-driver.ps1 -Configuration Debug` · Expected: `Build succeeded`, 0 warning (proyek WDK memakai warnings-as-errors), package berisi `GPVirtualCable.sys/.inf/.cat`.
- [ ] **Step 4: Code Analysis** — Run: build ulang dengan `/p:RunCodeAnalysis=true` · Expected: tidak ada warning baru di file yang diubah.
- [ ] **Step 5: Validasi INF** — Run: `& "${env:ProgramFiles(x86)}\Windows Kits\10\Tools\10.0.26100.0\x64\infverif.exe" /w driver\x64\Debug\package\GPVirtualCable.inf` · Expected: tidak ada error.
- [ ] **Step 6: Commit** — `git commit -m "feat(driver): GPVirtualCable 12 cable loopback (8 mono + 4 stereo)"`

### Task 5: Skrip instalasi + tes loopback end-to-end

**Files:**
- Create: `tools/enable-testsigning.ps1` (cek admin; `bcdedit /set testsigning on`; peringatan Secure Boot/BitLocker; minta reboot)
- Create: `tools/install.ps1` (cek admin; impor `package.cer` ke `LocalMachine\Root` dan `LocalMachine\TrustedPublisher`; `devcon install GPVirtualCable.inf Root\GPVirtualCable`; bila sudah terpasang, `devcon update`)
- Create: `tools/uninstall.ps1` (`devcon remove Root\GPVirtualCable`; `pnputil /delete-driver <oemXX.inf> /uninstall` untuk paket GPVirtualCable)
- Create: `python/gpcable/devices.py`, `python/gpcable/loopback.py`
- Test: `python/tests/test_devices.py`, `python/tests/test_loopback_e2e.py` (ditandai `@pytest.mark.e2e`, di-skip bila driver tidak terpasang)

**Interfaces:**
- Produces (`devices.py`, dipakai Task 6):
  `find_cable_devices(devices: Sequence[Mapping], hostapis: Sequence[Mapping], cfg: CableConfig) -> dict[str, CableDevices]`
  `@dataclass class CableDevices: input_index: int | None; output_index: int | None`
  Aturan: hanya host API bernama `"Windows WASAPI"`; device cocok bila `cable.name` muncul di nama device (`"Speakers (GP Mono 1)"` cocok untuk "GP Mono 1", tetapi "GP Mono 10" tidak boleh cocok untuk "GP Mono 1" — cocokkan sebagai kata utuh/diikuti `)` atau akhir string); input = `max_input_channels > 0`, output = `max_output_channels > 0`.
- Produces (`loopback.py`): `run_loopback(cable: Cable, devs: CableDevices, freq_hz: float, play_rate: int = 48000, play_channels: int | None = None, seconds: float = 1.0) -> LoopbackResult` dengan `LoopbackResult(peak_freq_hz: float, level_dbfs: float, channels: int)`; CLI `python -m gpcable.loopback` menjalankan semua cable dan mencetak `PASS/FAIL` per cable plus ringkasan `N/12 passed`. Frekuensi tone cable `i`: `300 + 150*i` Hz.

- [ ] **Step 1: Tulis test gagal** `test_devices.py` dengan daftar device palsu:
  ```python
  def test_matches_exact_cable_names_only(): ...   # "Microphone (GP Mono 1)" → Mono1.input; "GP Mono 10" tidak ikut
  def test_ignores_non_wasapi_hostapi(): ...       # device MME bernama sama diabaikan
  def test_missing_cable_reports_none(): ...       # Stereo4 tidak ada → CableDevices(None, None)
  ```
- [ ] **Step 2: Jalankan, pastikan gagal**, **Step 3: implementasikan `devices.py`**, **Step 4: jalankan, pastikan lulus** (`python -m pytest -q -m "not e2e"`).
- [ ] **Step 5: Tulis `test_loopback_e2e.py`**:
  ```python
  def test_every_cable_loops_back(cfg, devs):        # untuk semua 12: |peak_freq - freq| < 5 Hz, level > -20 dBFS, channels == cable.channels
  def test_no_crosstalk(cfg, devs):                  # putar ke Stereo1, rekam Mono1 → level < -90 dBFS
  def test_idle_cable_is_silent(cfg, devs):          # rekam Mono8 tanpa playback → level < -90 dBFS
  def test_mono_cable_accepts_stereo_44k(cfg, devs): # play_rate=44100, play_channels=2 ke Mono2 → |peak - freq| < 5 Hz
  ```
  lalu implementasikan `loopback.py` (`sounddevice.playrec` tidak bisa lintas device → jalankan `OutputStream` dan `InputStream` bersamaan, buang 200 ms pertama, FFT `numpy.fft.rfft` dengan jendela Hann).
- [ ] **Step 6: Pengguna menginstal di VM/PC uji** (admin): `tools\enable-testsigning.ps1` → reboot → `tools\install.ps1`. Verifikasi: Device Manager menampilkan "GP Virtual Cable"; `mmsys.cpl` menampilkan 12 entri Playback dan 12 Recording yang namanya memuat "GP Mono 1".."GP Stereo 4".
- [ ] **Step 7: Jalankan e2e** — Run: `cd python; python -m gpcable.loopback; python -m pytest -q -m e2e` · Expected: `12/12 passed`, semua test e2e PASS.
- [ ] **Step 8: Commit** — `git commit -m "feat: skrip install + tes loopback end-to-end"`

### Task 6: Logika level meter (TDD)

**Files:**
- Create: `python/gpcable/levels.py`
- Test: `python/tests/test_levels.py`

**Interfaces:**
- Produces:
  `FLOOR_DB = -120.0`
  `peak_dbfs(block: np.ndarray) -> float` (sampel float32 [-1,1], semua channel; silence → `FLOOR_DB`)
  `rms_dbfs(block: np.ndarray) -> float`
  `class PeakHold: __init__(self, hold_s: float = 1.5, decay_db_per_s: float = 20.0); update(self, value_db: float, now_s: float) -> float`
  `classify(peak_db: float, threshold_db: float = -60.0) -> Literal["signal", "silent"]`

- [ ] **Step 1: Tulis test gagal**
  ```python
  def test_full_scale_sine_peak_is_0db(): assert peak_dbfs(sine(1.0)) == pytest.approx(0.0, abs=0.01)
  def test_full_scale_sine_rms_is_minus3db(): assert rms_dbfs(sine(1.0)) == pytest.approx(-3.01, abs=0.05)
  def test_silence_hits_floor(): assert peak_dbfs(np.zeros((480, 2), np.float32)) == FLOOR_DB
  def test_half_amplitude_is_minus6db(): assert peak_dbfs(sine(0.5)) == pytest.approx(-6.02, abs=0.05)
  def test_peak_hold_holds_then_decays():
      ph = PeakHold(hold_s=1.5, decay_db_per_s=20.0)
      assert ph.update(-10.0, 0.0) == -10.0
      assert ph.update(-50.0, 1.0) == -10.0           # masih ditahan
      assert ph.update(-50.0, 2.0) == pytest.approx(-20.0)  # 0.5 s × 20 dB/s
      assert ph.update(-5.0, 2.1) == -5.0             # nilai baru lebih tinggi langsung naik
  def test_classify(): assert classify(-30.0) == "signal" and classify(-70.0) == "silent"
  ```
- [ ] **Step 2: Jalankan, pastikan gagal**, **Step 3: implementasikan**, **Step 4: jalankan, pastikan lulus** — `python -m pytest -q tests/test_levels.py`.
- [ ] **Step 5: Commit** — `git commit -m "feat(meter): logika level dBFS dan peak hold"`

### Task 7: Aplikasi meter GUI

**Files:**
- Create: `python/gpcable/meter.py` (entry point `gp-meter` di `pyproject.toml`)

**Interfaces:**
- Consumes: `load_config`, `default_config_path` (Task 3); `find_cable_devices` (Task 5); `peak_dbfs`, `rms_dbfs`, `PeakHold`, `classify` (Task 6).
- Perilaku: satu baris per cable (urutan `cables.toml`): nama, bar level peak (−60..0 dBFS), angka peak-hold, status `signal` / `silent` / `tidak ditemukan`. Satu `sounddevice.InputStream` shared-mode per cable yang ditemukan (`extra_settings=sd.WasapiSettings(exclusive=False)`); callback hanya menyimpan peak/RMS blok terakhir (tanpa alokasi berat); `QTimer` 30 Hz memperbarui UI. Tombol "Scan ulang" menutup dan membuka ulang stream. Error membuka satu stream → baris itu `tidak ditemukan`, cable lain tetap jalan.

- [ ] **Step 1: Implementasikan `meter.py`** sesuai perilaku di atas.
- [ ] **Step 2: Smoke test** — Run: `cd python; python -m gpcable.meter` sambil `python -m gpcable.loopback` berjalan · Expected: tiap cable menyala `signal` bergiliran; cable diam `silent`; jendela tetap responsif.
- [ ] **Step 3: Cek tidak mengganggu GP Mix** — dengan GP Mix-Lite membuka "GP Stereo 1" via WASAPI, buka meter → GP Mix tetap menerima audio tanpa putus.
- [ ] **Step 4: Commit** — `git commit -m "feat(meter): aplikasi meter PySide6 untuk 12 cable"`

### Task 8: Soak test + panduan pemakaian

**Files:**
- Create: `README.md` (bahasa Indonesia: apa ini, prasyarat, build, test-signing + Secure Boot, install/uninstall, ganti nama cable via `cables.toml` → codegen → build → `install.ps1`, setup GP Mix-Lite (WASAPI input dari "GP …") → OBS, menjalankan meter, troubleshooting)

- [ ] **Step 1: Driver Verifier di VM** — Run (admin): `verifier /standard /driver GPVirtualCable.sys` → reboot → putar audio ke 4 cable sekaligus + meter terbuka ±1 jam, termasuk sleep/resume sekali. Expected: tidak ada BSOD; `python -m gpcable.loopback` tetap `12/12 passed` sesudahnya. Lalu `verifier /reset`.
- [ ] **Step 2: Uji nyata** — Discord → GP Stereo 3, Spotify → GP Stereo 2, mic → GP Mono 1; ketiganya muncul sebagai channel terpisah di GP Mix-Lite; mix keluar ke OBS dan terekam.
- [ ] **Step 3: Tulis `README.md`**.
- [ ] **Step 4: Commit** — `git commit -m "docs: README instalasi dan setup GP Mix-Lite + OBS"`
