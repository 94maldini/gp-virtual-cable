from gpcable.config import default_config_path, load_config
from gpcable.devices import CableDeviceMap, find_cable_devices, name_matches

HOSTAPIS = [{"name": "MME"}, {"name": "Windows WASAPI"}]


def dev(name, hostapi, i=0, o=0):
    return {"name": name, "hostapi": hostapi, "max_input_channels": i, "max_output_channels": o}


def cfg():
    return load_config(default_config_path())


def test_name_matches_whole_word():
    assert name_matches("Speakers (GP Mono 1)", "GP Mono 1")
    assert not name_matches("Speakers (GP Mono 10)", "GP Mono 1")
    assert not name_matches("Speakers (GP Mono 1)", "GP Mono 10")
    assert not name_matches("Speakers (XGP Mono 1)", "GP Mono 1")


def test_finds_twelve_outputs_and_one_capture():
    c = cfg()
    devices = [dev(f"Speakers ({cb.name})", 1, o=cb.channels) for cb in c.cables]
    devices.append(dev(f"Microphone ({c.capture_name})", 1, i=c.capture_channels))
    res = find_cable_devices(devices, HOSTAPIS, c)
    assert isinstance(res, CableDeviceMap)
    assert len(res.outputs) == 12
    for n, cb in enumerate(c.cables):
        assert res.outputs[cb.id] == n
    assert res.capture == 12


def test_mono_1_does_not_match_mono_10():
    devices = [
        dev("Speakers (GP Mono 10)", 1, o=1),
        dev("Speakers (GP Mono 1)", 1, o=1),
    ]
    res = find_cable_devices(devices, HOSTAPIS, cfg())
    assert res.outputs["Mono1"] == 1


def test_ignores_non_wasapi():
    c = cfg()
    devices = [
        dev("Speakers (GP Mono 1)", 0, o=1),
        dev(f"Microphone ({c.capture_name})", 0, i=8),
    ]
    res = find_cable_devices(devices, HOSTAPIS, c)
    assert res.outputs["Mono1"] is None
    assert res.capture is None


def test_missing_capture_is_none():
    devices = [dev("Speakers (GP Mono 1)", 1, o=1)]
    res = find_cable_devices(devices, HOSTAPIS, cfg())
    assert len(res.outputs) == 12
    assert res.outputs["Mono1"] == 0
    assert res.outputs["Stereo4"] is None
    assert res.capture is None


def test_name_matches_custom_endpoint_name_format():
    # Format nama setelah fix kategori pin: "<nama pin> (<nama perangkat>)".
    assert name_matches("GP Mono 1 (GP Virtual Cable)", "GP Mono 1")
    assert name_matches("GP Stereo 4 (GP Virtual Cable)", "GP Stereo 4")
    assert name_matches("GP Virtual Cable (GP Virtual Cable)", "GP Virtual Cable")
    assert not name_matches("GP Mono 1 (GP Virtual Cable)", "GP Mono 2")
    # Nama lama yang dipaksa Windows tidak boleh dianggap cocok dengan cable mana pun.
    assert not name_matches("Speakers (GP Virtual Cable)", "GP Mono 1")
