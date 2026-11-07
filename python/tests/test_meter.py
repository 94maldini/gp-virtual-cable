from gpcable.config import Cable, CableConfig
from gpcable.devices import CableDevices
from gpcable.meter import bar_fraction, plan_streams


def test_bar_fraction_clamped_and_linear():
    assert bar_fraction(0.0) == 1.0
    assert bar_fraction(6.0) == 1.0
    assert bar_fraction(-60.0) == 0.0
    assert bar_fraction(-120.0) == 0.0
    assert bar_fraction(-30.0) == 0.5


def test_plan_streams_maps_found_and_missing():
    cfg = CableConfig(48000, 24, (
        Cable(0, "Mono1", "GP Mono 1", 1),
        Cable(1, "Stereo1", "GP Stereo 1", 2),
        Cable(2, "Stereo2", "GP Stereo 2", 2),
    ))
    found = {
        "Mono1": CableDevices(input_index=7, output_index=8),
        "Stereo1": CableDevices(input_index=None, output_index=3),
    }
    plan = plan_streams(cfg, found)
    assert [(c.id, i) for c, i in plan] == [("Mono1", 7), ("Stereo1", None), ("Stereo2", None)]
