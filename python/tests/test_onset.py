import numpy as np

from gpcable.loopback import first_onset_time


def test_skips_early_noise_and_finds_burst():
    rate = 48000
    chunk = np.zeros(1000, dtype=np.float32)
    chunk[10] = 0.5          # leftover noise, before not_before
    chunk[600:] = 0.3        # the burst
    t = first_onset_time(chunk, adc_time=10.0, rate=rate, threshold_db=-40.0,
                         not_before=10.0 + 500 / rate)
    assert t == 10.0 + 600 / rate


def test_none_when_nothing_after_not_before():
    chunk = np.zeros(100, dtype=np.float32)
    chunk[5] = 0.5
    assert first_onset_time(chunk, 0.0, 48000, -40.0, not_before=1.0) is None
