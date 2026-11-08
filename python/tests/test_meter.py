from gpcable.config import Cable, CableConfig, default_config_path, load_config
from gpcable.meter import bar_fraction, channel_label, row_status


def test_bar_fraction_clamped_and_linear():
    assert bar_fraction(0.0) == 1.0
    assert bar_fraction(6.0) == 1.0
    assert bar_fraction(-60.0) == 0.0
    assert bar_fraction(-120.0) == 0.0
    assert bar_fraction(-30.0) == 0.5


def test_row_status_uses_instantaneous_peak():
    assert row_status(False, 0.0) == "tidak ditemukan"
    assert row_status(True, -20.0) == "signal"
    assert row_status(True, -90.0) == "silent"


def test_channel_label_mono_and_stereo():
    assert channel_label(Cable(0, "Mono3", "GP Mono 3", 1, 3)) == "ch 3"
    assert channel_label(Cable(1, "Stereo2", "GP Stereo 2", 2, 3)) == "ch 3+4"


def test_meter_rows_show_channel_labels():
    cfg = load_config(default_config_path())
    assert [channel_label(c) for c in cfg.cables] == [
        "ch 1", "ch 2", "ch 3", "ch 4", "ch 5", "ch 6", "ch 7", "ch 8",
        "ch 1+2", "ch 3+4", "ch 5+6", "ch 7+8"]
