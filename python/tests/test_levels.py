import numpy as np
import pytest
from gpcable.levels import FLOOR_DB, peak_dbfs, rms_dbfs, PeakHold, classify


def sine(amplitude, num_samples=4800, sample_rate=48000, freq=1000, channels=2):
    """Generate a sine wave test signal.

    Args:
        amplitude: Peak amplitude (0.0 to 1.0)
        num_samples: Number of time-domain samples per channel
        sample_rate: Sample rate in Hz
        freq: Frequency in Hz
        channels: Number of channels

    Returns:
        numpy array of shape (num_samples, channels) with float32 sine wave
    """
    t = np.arange(num_samples, dtype=np.float32) / sample_rate
    signal = amplitude * np.sin(2 * np.pi * freq * t, dtype=np.float32)
    return np.column_stack([signal] * channels).astype(np.float32)


def test_full_scale_sine_peak_is_0db():
    assert peak_dbfs(sine(1.0)) == pytest.approx(0.0, abs=0.01)


def test_full_scale_sine_rms_is_minus3db():
    assert rms_dbfs(sine(1.0)) == pytest.approx(-3.01, abs=0.05)


def test_silence_hits_floor():
    assert peak_dbfs(np.zeros((480, 2), np.float32)) == FLOOR_DB


def test_half_amplitude_is_minus6db():
    assert peak_dbfs(sine(0.5)) == pytest.approx(-6.02, abs=0.05)


def test_peak_hold_holds_then_decays():
    ph = PeakHold(hold_s=1.5, decay_db_per_s=20.0)
    assert ph.update(-10.0, 0.0) == -10.0
    assert ph.update(-50.0, 1.0) == -10.0           # masih ditahan
    assert ph.update(-50.0, 2.0) == pytest.approx(-20.0)  # 0.5 s × 20 dB/s
    assert ph.update(-5.0, 2.1) == -5.0             # nilai baru lebih tinggi langsung naik


def test_classify():
    assert classify(-30.0) == "signal"
    assert classify(-70.0) == "silent"


def test_linear_to_dbfs_values():
    from gpcable.levels import linear_to_dbfs
    assert linear_to_dbfs(1.0) == 0.0
    assert linear_to_dbfs(0.5) == pytest.approx(-6.02, abs=0.01)
    assert linear_to_dbfs(0.0) == FLOOR_DB
    assert linear_to_dbfs(-0.3) == FLOOR_DB
    assert linear_to_dbfs(1e-9) == FLOOR_DB
    assert type(linear_to_dbfs(0.5)) is float


def test_peak_hold_never_below_floor():
    h = PeakHold()
    h.update(-10.0, 0.0)
    assert h.update(FLOOR_DB, 1000.0) == FLOOR_DB
