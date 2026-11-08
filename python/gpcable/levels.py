"""Level meter logic - pure math for audio level computation."""

import math
from typing import Literal
import numpy as np

FLOOR_DB = -120.0


def linear_to_dbfs(x: float) -> float:
    """Linear amplitude (0..1) to dBFS; <= 0 or tiny values give FLOOR_DB."""
    if x <= 0:
        return FLOOR_DB
    return float(max(20.0 * math.log10(x), FLOOR_DB))


def peak_dbfs(block: np.ndarray) -> float:
    """Compute peak dBFS of audio block.

    Args:
        block: numpy array of float32 samples in [-1, 1], any shape (all channels combined)

    Returns:
        Peak level in dBFS, never below FLOOR_DB
    """
    if block.size == 0:
        return FLOOR_DB

    peak = np.max(np.abs(block))

    if peak == 0:
        return FLOOR_DB

    return float(max(20 * np.log10(peak), FLOOR_DB))


def rms_dbfs(block: np.ndarray) -> float:
    """Compute RMS dBFS of audio block.

    Args:
        block: numpy array of float32 samples in [-1, 1], any shape (all channels combined)

    Returns:
        RMS level in dBFS, never below FLOOR_DB
    """
    if block.size == 0:
        return FLOOR_DB

    rms = np.sqrt(np.mean(block ** 2))

    if rms == 0:
        return FLOOR_DB

    return float(max(20 * np.log10(rms), FLOOR_DB))


class PeakHold:
    """Hold peak value with decay.

    Holds the highest value for hold_s seconds after it was set, then decays
    linearly at decay_db_per_s from the held value.
    """

    def __init__(self, hold_s: float = 1.5, decay_db_per_s: float = 20.0):
        """Initialize PeakHold.

        Args:
            hold_s: Hold time in seconds
            decay_db_per_s: Decay rate in dB/s
        """
        self.hold_s = hold_s
        self.decay_db_per_s = decay_db_per_s
        self._held_value = FLOOR_DB
        self._held_time = 0.0
        self._last_time = 0.0

    def update(self, value_db: float, now_s: float) -> float:
        """Update peak hold with new value at time now_s.

        A new value above the current displayed value takes over immediately.
        Never returns below the incoming value.

        Args:
            value_db: New input value in dB
            now_s: Current time in seconds

        Returns:
            Current held value in dB
        """
        # Check if hold period has expired
        time_since_held = now_s - self._held_time

        if time_since_held > self.hold_s:
            # Hold period expired, apply decay
            decay_amount = (time_since_held - self.hold_s) * self.decay_db_per_s
            current_value = max(self._held_value - decay_amount, FLOOR_DB)
        else:
            # Still in hold period
            current_value = self._held_value

        # New value takes over if higher
        if value_db > current_value:
            self._held_value = value_db
            self._held_time = now_s
            return value_db

        self._last_time = now_s
        return current_value


def classify(peak_db: float, threshold_db: float = -60.0) -> Literal["signal", "silent"]:
    """Classify audio as signal or silent.

    Args:
        peak_db: Peak level in dB
        threshold_db: Threshold for classification

    Returns:
        "signal" if peak_db >= threshold_db, else "silent"
    """
    return "signal" if peak_db >= threshold_db else "silent"
