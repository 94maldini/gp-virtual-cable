"""End-to-end loopback check: play tones into the 12 Playback endpoints and
record the single 8-channel Recording endpoint "GP Virtual Cable"."""
from __future__ import annotations

import sys
import threading
import time
from collections.abc import Sequence

import numpy as np
import sounddevice as sd

from gpcable.config import Cable, default_config_path, load_config
from gpcable.devices import find_cable_devices
from gpcable.levels import rms_dbfs

REC_RATE = 48000
CAPTURE_CHANNELS = 8
DISCARD_S = 0.2
AMPLITUDE = 0.5
ONSET_DBFS = -40.0

_INT_SCALE = {"int16": 32768.0, "int32": 2147483648.0}


def tone_freq(cable: Cable) -> float:
    return 300.0 + 150.0 * cable.index


def channel_peak(rec: np.ndarray, ch: int) -> tuple[float, float]:
    """Return (peak frequency Hz via Hann + rFFT, RMS dBFS) of one channel."""
    x = rec[:, ch]
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    peak = float(np.argmax(spec)) * REC_RATE / len(x)
    return peak, float(rms_dbfs(x))


def record_route(
    out_idx: int | None,
    cap_idx: int,
    tones: Sequence[tuple[int, float]],
    channels_out: int,
    play_rate: int = REC_RATE,
    seconds: float = 1.0,
    preroll_s: float = 0.0,
    capture_dtype: str = "float32",
) -> np.ndarray:
    """Play `tones` [(channel_out_index, freq_hz)] on the output stream (channel
    index within the played stream, 0..channels_out-1) and record the 8-channel
    capture endpoint. out_idx=None records only. Playback runs `preroll_s` before
    recording starts; the first 200 ms of the recording are discarded.
    Returns float32 (frames, 8)."""
    wasapi = sd.WasapiSettings(auto_convert=True)
    chunks: list[np.ndarray] = []
    phase = [0]

    def out_cb(outdata, frames, time_info, status):
        t = (phase[0] + np.arange(frames)) / play_rate
        outdata[:] = 0
        for ch, freq in tones:
            outdata[:, ch] += (AMPLITUDE * np.sin(2 * np.pi * freq * t)).astype(np.float32)
        phase[0] += frames

    def in_cb(indata, frames, time_info, status):
        chunks.append(indata.copy())

    out = None
    inp = None
    try:
        if out_idx is not None:
            out = sd.OutputStream(
                device=out_idx, samplerate=play_rate, channels=channels_out,
                dtype="float32", callback=out_cb, extra_settings=wasapi,
            )
        inp = sd.InputStream(
            device=cap_idx, samplerate=REC_RATE, channels=CAPTURE_CHANNELS,
            dtype=capture_dtype, callback=in_cb, extra_settings=wasapi,
        )
        if out is not None:
            out.start()
            time.sleep(max(preroll_s, 0.3))  # let playback settle before recording
        inp.start()
        time.sleep(DISCARD_S + seconds)
    finally:
        try:
            if inp is not None:
                inp.close()
        finally:
            if out is not None:
                out.close()
    if not chunks:
        raise RuntimeError("no audio captured")
    rec = np.concatenate(chunks)
    scale = _INT_SCALE.get(capture_dtype)
    rec = rec.astype(np.float32) / scale if scale else rec.astype(np.float32)
    return rec[int(DISCARD_S * REC_RATE):]


def first_onset_time(
    chunk: np.ndarray, adc_time: float, rate: int, threshold_db: float, not_before: float
) -> float | None:
    """Time of the first sample in a 1-D chunk that exceeds threshold_db and whose
    timestamp (adc_time + index / rate) is >= not_before; None if there is none.
    Earlier loud samples (noise, leftover audio) are skipped."""
    thresh = 10 ** (threshold_db / 20)
    idx = np.nonzero(np.abs(chunk) > thresh)[0]
    times = adc_time + idx / rate
    ok = np.nonzero(times >= not_before)[0]
    return float(times[ok[0]]) if ok.size else None


def measure_latency_ms(
    out_idx: int, cap_idx: int, channels_out: int, first_channel: int, preroll_s: float = 0.0
) -> float:
    """Latency from playing a burst to hearing it on capture channel first_channel-1.

    One output stream is opened first and plays continuously: a quiet 440 Hz tone
    on every output channel for `preroll_s` seconds (no input stream open, so any
    backlog the driver builds up during that time stays in the stream), then
    silence. Only after the preroll is the input stream opened; ~0.5 s later the
    output plays a 1 kHz burst. The output callback stores the DAC time of the
    burst's first frame (time_info.outputBufferDacTime + offset); the input
    callback takes the first sample >= -40 dBFS on the target channel whose time
    is not earlier than that, as time_info.inputBufferAdcTime + offset. Both use
    the same PortAudio stream clock, so latency = adc_onset - dac_start (ms)."""
    wasapi = sd.WasapiSettings(auto_convert=True)
    target = first_channel - 1
    preroll_n = int(preroll_s * REC_RATE)
    burst_len = int(0.3 * REC_RATE)
    pos = [0]
    burst_at: list[int | None] = [None]
    dac_start: list[float | None] = [None]
    adc_onset: list[float | None] = [None]
    done = threading.Event()

    def out_cb(outdata, frames, time_info, status):
        outdata[:] = 0
        n0 = pos[0]
        idx = np.arange(n0, n0 + frames)
        pre = idx < preroll_n
        if pre.any():
            quiet = 0.1 * np.sin(2 * np.pi * 440.0 * idx / REC_RATE)
            outdata[pre, :] = quiet[pre, None].astype(np.float32)
        start = burst_at[0]
        if start is not None:
            mask = (idx >= start) & (idx < start + burst_len)
            if mask.any():
                if dac_start[0] is None:
                    first = int(np.argmax(mask))
                    dac_start[0] = time_info.outputBufferDacTime + first / REC_RATE
                tone = AMPLITUDE * np.sin(2 * np.pi * 1000.0 * idx / REC_RATE)
                outdata[mask, :] = tone[mask, None].astype(np.float32)
        pos[0] += frames

    def in_cb(indata, frames, time_info, status):
        if dac_start[0] is None or adc_onset[0] is not None:
            return
        t = first_onset_time(indata[:, target], time_info.inputBufferAdcTime,
                             REC_RATE, ONSET_DBFS, dac_start[0])
        if t is not None:
            adc_onset[0] = t
            done.set()

    out = None
    inp = None
    try:
        out = sd.OutputStream(
            device=out_idx, samplerate=REC_RATE, channels=channels_out,
            dtype="float32", callback=out_cb, extra_settings=wasapi,
        )
        out.start()
        if preroll_s > 0:
            time.sleep(preroll_s)
        inp = sd.InputStream(
            device=cap_idx, samplerate=REC_RATE, channels=CAPTURE_CHANNELS,
            dtype="float32", callback=in_cb, extra_settings=wasapi,
        )
        inp.start()
        burst_at[0] = max(pos[0], preroll_n) + int(0.5 * REC_RATE)
        if not done.wait(timeout=3.0):
            raise RuntimeError("latency: burst onset not detected on capture channel")
    finally:
        try:
            if inp is not None:
                inp.close()
        finally:
            if out is not None:
                out.close()
    return (adc_onset[0] - dac_start[0]) * 1000.0


def main() -> int:
    cfg = load_config(default_config_path())
    found = find_cable_devices(sd.query_devices(), sd.query_hostapis(), cfg)
    if found.capture is None:
        print(f"FAIL {cfg.capture_name}: recording device not found")
        print(f"0/{len(cfg.cables)} passed")
        return 1
    passed = 0
    for cable in cfg.cables:
        out_idx = found.outputs[cable.id]
        freq = tone_freq(cable)
        if out_idx is None:
            print(f"FAIL {cable.name}: playback device not found")
            continue
        try:
            rec = record_route(out_idx, found.capture,
                               [(k, freq) for k in range(cable.channels)], cable.channels)
        except Exception as e:  # noqa: BLE001 - report any audio error per cable
            print(f"FAIL {cable.name}: {e}")
            continue
        dest = range(cable.first_channel - 1, cable.first_channel - 1 + cable.channels)
        problems = []
        for ch in range(CAPTURE_CHANNELS):
            peak, level = channel_peak(rec, ch)
            if ch in dest:
                if abs(peak - freq) >= 5 or level <= -20:
                    problems.append(f"ch{ch + 1}: {peak:.1f} Hz {level:.1f} dBFS")
            elif level >= -90:
                problems.append(f"ch{ch + 1} leak {level:.1f} dBFS")
        ok = not problems
        passed += ok
        print(f"{'PASS' if ok else 'FAIL'} {cable.name}: {freq:.0f} Hz -> ch "
              f"{dest[0] + 1}..{dest[-1] + 1}" + ("" if ok else " | " + "; ".join(problems)))
    print(f"{passed}/{len(cfg.cables)} passed")
    return 0 if passed == len(cfg.cables) else 1


if __name__ == "__main__":
    sys.exit(main())
