from gpcable.config import default_config_path, load_config
from gpcable.devices import CableDevices, find_cable_devices

HOSTAPIS = [{"name": "MME"}, {"name": "Windows WASAPI"}]


def dev(name, hostapi, i=0, o=0):
    return {"name": name, "hostapi": hostapi, "max_input_channels": i, "max_output_channels": o}


def cfg():
    return load_config(default_config_path())


def test_matches_exact_cable_names_only():
    devices = [
        dev("Microphone (GP Mono 1)", 1, i=1),
        dev("Speakers (GP Mono 1)", 1, o=1),
        dev("Microphone (GP Mono 10)", 1, i=1),
    ]
    res = find_cable_devices(devices, HOSTAPIS, cfg())
    assert res["Mono1"] == CableDevices(input_index=0, output_index=1)


def test_ignores_non_wasapi_hostapi():
    devices = [
        dev("Microphone (GP Mono 1)", 0, i=1),
        dev("Speakers (GP Mono 1)", 0, o=1),
    ]
    res = find_cable_devices(devices, HOSTAPIS, cfg())
    assert res["Mono1"] == CableDevices(None, None)


def test_missing_cable_reports_none():
    devices = [dev("Speakers (GP Mono 1)", 1, o=1)]
    res = find_cable_devices(devices, HOSTAPIS, cfg())
    assert len(res) == 12
    assert res["Stereo4"] == CableDevices(None, None)
