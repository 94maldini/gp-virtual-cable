import numpy as np
import pytest

from gpcable.loopback import (
    AMPLITUDE, CAPTURE_CHANNELS, REC_RATE, channel_peak, measure_latency_ms,
    record_route, tone_freq,
)

pytestmark = pytest.mark.e2e


def _cable(cfg, cid):
    return next(c for c in cfg.cables if c.id == cid)


def _out(devmap, cable):
    idx = devmap.outputs[cable.id]
    if idx is None:
        pytest.skip(f"{cable.name}: playback device not found")
    return idx


def _level_near(rec, ch, freq):
    """Level (dBFS, sine amplitude) of the spectral component near freq on channel ch."""
    x = rec[:, ch]
    w = np.hanning(len(x))
    spec = np.abs(np.fft.rfft(x * w)) * 2 / w.sum()
    freqs = np.fft.rfftfreq(len(x), 1 / REC_RATE)
    band = (freqs > freq - 5) & (freqs < freq + 5)
    return 20 * np.log10(max(float(spec[band].max()), 1e-12))


def test_each_cable_lands_only_on_its_channels(cfg, devmap):
    for cable in cfg.cables:
        freq = tone_freq(cable)
        rec = record_route(_out(devmap, cable), devmap.capture,
                           [(k, freq) for k in range(cable.channels)], cable.channels)
        dest = range(cable.first_channel - 1, cable.first_channel - 1 + cable.channels)
        for ch in range(CAPTURE_CHANNELS):
            peak, level = channel_peak(rec, ch)
            if ch in dest:
                assert abs(peak - freq) < 5, (cable.name, ch)
                assert level > -20, (cable.name, ch)
            else:
                assert level < -90, (cable.name, ch)


def test_shared_channel_mixes(cfg, devmap):
    import sounddevice as sd

    mono1, stereo1 = _cable(cfg, "Mono1"), _cable(cfg, "Stereo1")
    f_mono, f_stereo = 300.0, 1500.0
    phase = [0]

    def cb(outdata, frames, time_info, status):
        t = (phase[0] + np.arange(frames)) / REC_RATE
        outdata[:, 0] = (AMPLITUDE * np.sin(2 * np.pi * f_mono * t)).astype(np.float32)
        phase[0] += frames

    # Mono1 plays in the background while Stereo1 is played and the capture recorded.
    mono_stream = sd.OutputStream(
        device=_out(devmap, mono1), samplerate=REC_RATE, channels=1,
        dtype="float32", callback=cb, extra_settings=sd.WasapiSettings(auto_convert=True),
    )
    try:
        mono_stream.start()
        rec = record_route(_out(devmap, stereo1), devmap.capture,
                           [(0, f_stereo), (1, f_stereo)], 2)
    finally:
        mono_stream.close()
    assert _level_near(rec, 0, f_mono) > -30
    assert _level_near(rec, 0, f_stereo) > -30


def test_latency_does_not_accumulate(cfg, devmap):
    stereo2 = _cable(cfg, "Stereo2")
    out = _out(devmap, stereo2)
    record_route(out, devmap.capture, [(0, 440.0), (1, 440.0)], 2, preroll_s=2.0)
    assert measure_latency_ms(out, devmap.capture, 2, stereo2.first_channel) <= 30


def test_stereo_cable_accepts_mono_44k(cfg, devmap):
    stereo3 = _cable(cfg, "Stereo3")  # capture channels 5 and 6 (indices 4, 5)
    freq = tone_freq(stereo3)
    rec = record_route(_out(devmap, stereo3), devmap.capture, [(0, freq)], 1, play_rate=44100)
    for ch in (stereo3.first_channel - 1, stereo3.first_channel):
        peak, level = channel_peak(rec, ch)
        assert abs(peak - freq) < 5, ch
        assert level > -20, ch


@pytest.mark.parametrize("dtype", ["int16", "int32"])
def test_routes_in_all_capture_formats(cfg, devmap, dtype):
    stereo4 = _cable(cfg, "Stereo4")
    freq = tone_freq(stereo4)
    try:
        rec = record_route(_out(devmap, stereo4), devmap.capture,
                           [(0, freq), (1, freq)], 2, capture_dtype=dtype)
    except Exception as e:  # PortAudio rejects this format for the device
        import sounddevice as sd
        if isinstance(e, sd.PortAudioError):
            pytest.skip(f"capture format {dtype} rejected by PortAudio: {e}")
        raise
    dest = range(stereo4.first_channel - 1, stereo4.first_channel + 1)
    for ch in range(CAPTURE_CHANNELS):
        peak, level = channel_peak(rec, ch)
        if ch in dest:
            assert abs(peak - freq) < 5 and level > -20, (dtype, ch)
        else:
            assert level < -90, (dtype, ch)
