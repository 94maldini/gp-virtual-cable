import pytest

from gpcable.config import default_config_path, load_config

HEAD = 'sample_rate = 48000\nbits = 24\n'


def _cable(i, name="X", ch=1):
    return f'[[cable]]\nid = "{i}"\nname = "{name}"\nchannels = {ch}\n'


BAD = {
    "dup_ids": HEAD + _cable("A") + _cable("A"),
    "bad_channels_3": HEAD + _cable("A", ch=3),
    "id_with_space": HEAD + _cable("A B"),
    "empty_name": HEAD + _cable("A", name=""),
    "no_cables": HEAD,
    "too_many": HEAD + "".join(_cable(f"C{n}") for n in range(33)),
}


def test_load_repo_config():
    cfg = load_config(default_config_path())
    assert [c.name for c in cfg.cables][:2] == ["GP Mono 1", "GP Mono 2"]
    assert [c.channels for c in cfg.cables] == [1] * 8 + [2] * 4
    assert (cfg.sample_rate, cfg.bits) == (48000, 24)
    assert [c.index for c in cfg.cables] == list(range(12))


@pytest.mark.parametrize("key", list(BAD))
def test_invalid_config_rejected(tmp_path, key):
    p = tmp_path / "c.toml"
    p.write_text(BAD[key], encoding="utf-8")
    with pytest.raises(ValueError):
        load_config(p)
