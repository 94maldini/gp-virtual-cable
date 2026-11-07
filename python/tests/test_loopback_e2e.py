import pytest

from gpcable.devices import CableDevices
from gpcable.loopback import analyse, record, run_loopback, tone_freq

pytestmark = pytest.mark.e2e


def _cable(cfg, cid):
    return next(c for c in cfg.cables if c.id == cid)


def test_every_cable_loops_back(cfg, devs):
    for cable in cfg.cables:
        freq = tone_freq(cable)
        r = run_loopback(cable, devs[cable.id], freq)
        assert abs(r.peak_freq_hz - freq) < 5, cable.name
        assert r.level_dbfs > -20, cable.name
        assert r.channels == cable.channels, cable.name


def test_no_crosstalk(cfg, devs):
    stereo, mono = _cable(cfg, "Stereo1"), _cable(cfg, "Mono1")
    played = devs[stereo.id]
    rec = record(mono, CableDevices(devs[mono.id].input_index, played.output_index),
                 tone_freq(stereo))
    assert analyse(rec)[1] < -90


def test_idle_cable_is_silent(cfg, devs):
    mono8 = _cable(cfg, "Mono8")
    assert analyse(record(mono8, devs[mono8.id], None))[1] < -90


def test_mono_cable_accepts_stereo_44k(cfg, devs):
    mono2 = _cable(cfg, "Mono2")
    freq = tone_freq(mono2)
    r = run_loopback(mono2, devs[mono2.id], freq, play_rate=44100, play_channels=2)
    assert abs(r.peak_freq_hz - freq) < 5
