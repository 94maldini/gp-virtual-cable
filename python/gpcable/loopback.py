"""End-to-end loopback check: play a tone into a cable's Playback endpoint,
record it from the paired Recording endpoint, and analyse the result."""
from __future__ import annotations

import sys
import time
from dataclasses import dataclass

import numpy as np
import sounddevice as sd

from gpcable.config import Cable, default_config_path, load_config
from gpcable.devices import CableDevices, find_cable_devices
from gpcable.levels import rms_dbfs

REC_RATE = 48000
DISCARD_S = 0.2
AMPLITUDE = 0.5


@dataclass(frozen=True)
class LoopbackResult:
    peak_freq_hz: float
    level_dbfs: float
    channels: int


def tone_freq(cable: Cable) -> float:
    return 300.0 + 150.0 * cable.index


def analyse(rec: np.ndarray, rate: int = REC_RATE) -> tuple[float, float]:
    """Return (peak frequency Hz, RMS dBFS) of a (frames, channels) recording."""
    mono = rec.mean(axis=1) if rec.ndim > 1 else rec
    windowed = mono * np.hanning(len(mono))
    spec = np.abs(np.fft.rfft(windowed))
    peak = float(np.argmax(spec)) * rate / len(mono)
    return peak, float(rms_dbfs(rec))


def record(
    cable: Cable,
    devs: CableDevices,
    freq_hz: float | None,
    play_rate: int = REC_RATE,
    play_channels: int | None = None,
    seconds: float = 1.0,
) -> np.ndarray:
    """Record from the cable's input device. If freq_hz is None nothing is played."""
    if devs.input_index is None:
        raise RuntimeError(f"{cable.name}: recording device not found")
    if freq_hz is not None and devs.output_index is None:
        raise RuntimeError(f"{cable.name}: playback device not found")
    play_channels = play_channels or cable.channels
    wasapi = sd.WasapiSettings(auto_convert=True)
    chunks: list[np.ndarray] = []
    phase = [0]

    def out_cb(outdata, frames, time_info, status):
        t = (phase[0] + np.arange(frames)) / play_rate
        outdata[:] = (AMPLITUDE * np.sin(2 * np.pi * freq_hz * t))[:, None]
        phase[0] += frames

    def in_cb(indata, frames, time_info, status):
        chunks.append(indata.copy())

    out = None
    inp = None
    try:
        if freq_hz is not None:
            out = sd.OutputStream(
                device=devs.output_index, samplerate=play_rate, channels=play_channels,
                dtype="float32", callback=out_cb, extra_settings=wasapi,
            )
        inp = sd.InputStream(
            device=devs.input_index, samplerate=REC_RATE, channels=cable.channels,
            dtype="float32", callback=in_cb, extra_settings=wasapi,
        )
        if out is not None:
            out.start()
            time.sleep(0.3)  # let playback settle before recording
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
        raise RuntimeError(f"{cable.name}: no audio captured")
    rec = np.concatenate(chunks)
    return rec[int(DISCARD_S * REC_RATE):]


def run_loopback(
    cable: Cable,
    devs: CableDevices,
    freq_hz: float,
    play_rate: int = REC_RATE,
    play_channels: int | None = None,
    seconds: float = 1.0,
) -> LoopbackResult:
    rec = record(cable, devs, freq_hz, play_rate, play_channels, seconds)
    peak, level = analyse(rec)
    return LoopbackResult(peak_freq_hz=peak, level_dbfs=level, channels=rec.shape[1])


def main() -> int:
    cfg = load_config(default_config_path())
    found = find_cable_devices(sd.query_devices(), sd.query_hostapis(), cfg)
    passed = 0
    for cable in cfg.cables:
        devs = found[cable.id]
        freq = tone_freq(cable)
        if devs.input_index is None or devs.output_index is None:
            missing = "recording" if devs.input_index is None else "playback"
            print(f"FAIL {cable.name}: {missing} device not found")
            continue
        try:
            r = run_loopback(cable, devs, freq)
        except Exception as e:  # noqa: BLE001 - report any audio error per cable
            print(f"FAIL {cable.name}: {e}")
            continue
        ok = (abs(r.peak_freq_hz - freq) < 5 and r.level_dbfs > -20
              and r.channels == cable.channels)
        passed += ok
        print(f"{'PASS' if ok else 'FAIL'} {cable.name}: expected {freq:.0f} Hz, "
              f"got {r.peak_freq_hz:.1f} Hz, {r.level_dbfs:.1f} dBFS, {r.channels} ch")
    print(f"{passed}/{len(cfg.cables)} passed")
    return 0 if passed == len(cfg.cables) else 1


if __name__ == "__main__":
    sys.exit(main())
