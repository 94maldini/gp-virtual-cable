import pytest

from gpcable.config import default_config_path, load_config
from gpcable.devices import find_cable_devices


@pytest.fixture(scope="session")
def cfg():
    return load_config(default_config_path())


@pytest.fixture(scope="session")
def devs(cfg):
    sd = pytest.importorskip("sounddevice")
    try:
        found = find_cable_devices(sd.query_devices(), sd.query_hostapis(), cfg)
    except Exception as e:  # PortAudio unavailable
        pytest.skip(f"cannot enumerate audio devices: {e}")
    if not any(d.input_index is not None or d.output_index is not None
               for d in found.values()):
        pytest.skip("GP Virtual Cable driver not installed (no GP cable devices found)")
    return found
