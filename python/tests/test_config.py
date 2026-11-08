import pytest

from gpcable.config import default_config_path, load_config

HEAD = ('sample_rate = 48000\nbits = 24\ncapture_channels = 8\n'
        'capture_name = "GP Virtual Cable"\n')


def _cable(i, name="X", ch=1, first=1):
    return (f'[[cable]]\nid = "{i}"\nname = "{name}"\nchannels = {ch}\n'
            f'first_channel = {first}\n')


BAD = {
    "dup_ids": HEAD + _cable("A") + _cable("A"),
    "bad_channels_3": HEAD + _cable("A", ch=3),
    "id_with_space": HEAD + _cable("A B"),
    "empty_name": HEAD + _cable("A", name=""),
    "no_cables": HEAD,
    "too_many": HEAD + "".join(_cable(f"C{n}") for n in range(33)),
    "stereo_first_8": HEAD + _cable("A", ch=2, first=8),
    "first_zero": HEAD + _cable("A", first=0),
    "mono_first_9": HEAD + _cable("A", first=9),
    "rate_44100": HEAD.replace("48000", "44100") + _cable("A"),
    "bits_16": HEAD.replace("bits = 24", "bits = 16") + _cable("A"),
    "capture_ch_0": HEAD.replace("capture_channels = 8", "capture_channels = 0") + _cable("A"),
    "capture_ch_33": HEAD.replace("capture_channels = 8", "capture_channels = 33") + _cable("A"),
    "capture_name_empty": HEAD.replace('"GP Virtual Cable"', '""') + _cable("A"),
}


def test_load_repo_config():
    cfg = load_config(default_config_path())
    assert [c.name for c in cfg.cables][:2] == ["GP Mono 1", "GP Mono 2"]
    assert [c.first_channel for c in cfg.cables] == [1, 2, 3, 4, 5, 6, 7, 8, 1, 3, 5, 7]
    assert [c.channels for c in cfg.cables] == [1] * 8 + [2] * 4
    assert (cfg.capture_channels, cfg.capture_name) == (8, "GP Virtual Cable")
    assert (cfg.sample_rate, cfg.bits) == (48000, 24)
    assert [c.index for c in cfg.cables] == list(range(12))


@pytest.mark.parametrize("key", list(BAD))
def test_invalid_config_rejected(tmp_path, key):
    p = tmp_path / "c.toml"
    p.write_text(BAD[key], encoding="utf-8")
    with pytest.raises(ValueError):
        load_config(p)


def test_route_out_of_range_rejected(tmp_path):
    p = tmp_path / "c.toml"
    p.write_text(HEAD + _cable("S", ch=2, first=8), encoding="utf-8")
    with pytest.raises(ValueError, match="first_channel"):
        load_config(p)


def test_format_must_be_48k_24bit(tmp_path):
    p = tmp_path / "c.toml"
    p.write_text(HEAD.replace("48000", "44100") + _cable("A"), encoding="utf-8")
    with pytest.raises(ValueError):
        load_config(p)
    p.write_text(HEAD.replace("bits = 24", "bits = 16") + _cable("A"), encoding="utf-8")
    with pytest.raises(ValueError):
        load_config(p)


def test_overlap_allowed():
    cfg = load_config(default_config_path())
    assert cfg.cables[0].first_channel == cfg.cables[8].first_channel == 1
