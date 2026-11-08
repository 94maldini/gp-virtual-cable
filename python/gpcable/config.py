"""Load and validate cables.toml (single source of truth for the cable list)."""
from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass
from pathlib import Path

MAX_CABLES = 32
MAX_CAPTURE_CHANNELS = 32
_ID_RE = re.compile(r"[A-Za-z][A-Za-z0-9]*")


@dataclass(frozen=True)
class Cable:
    index: int
    id: str
    name: str
    channels: int
    first_channel: int  # 1-based position in the capture endpoint


@dataclass(frozen=True)
class CableConfig:
    sample_rate: int
    bits: int
    capture_channels: int
    capture_name: str
    cables: tuple[Cable, ...]


def default_config_path() -> Path:
    return Path(__file__).resolve().parents[2] / "cables.toml"


def load_config(path: Path) -> CableConfig:
    with open(path, "rb") as f:
        data = tomllib.load(f)
    sample_rate = data.get("sample_rate", 48000)
    bits = data.get("bits", 24)
    if isinstance(sample_rate, bool) or sample_rate != 48000:
        raise ValueError(f"sample_rate must be 48000, got {sample_rate!r}")
    if isinstance(bits, bool) or bits != 24:
        raise ValueError(f"bits must be 24, got {bits!r}")
    cap_ch = data.get("capture_channels")
    if isinstance(cap_ch, bool) or not isinstance(cap_ch, int) or not 1 <= cap_ch <= MAX_CAPTURE_CHANNELS:
        raise ValueError(f"capture_channels must be 1..{MAX_CAPTURE_CHANNELS}, got {cap_ch!r}")
    cap_name = data.get("capture_name")
    if not isinstance(cap_name, str) or not cap_name.strip():
        raise ValueError("capture_name must not be empty")
    raw = data.get("cable", [])
    if not 1 <= len(raw) <= MAX_CABLES:
        raise ValueError(f"cable count must be 1..{MAX_CABLES}, got {len(raw)}")
    cables: list[Cable] = []
    seen: set[str] = set()
    for i, c in enumerate(raw):
        cid = c.get("id")
        if not isinstance(cid, str) or not _ID_RE.fullmatch(cid):
            raise ValueError(f"invalid cable id: {cid!r}")
        if cid in seen:
            raise ValueError(f"duplicate cable id: {cid}")
        seen.add(cid)
        name = c.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError(f"cable {cid}: name must not be empty")
        ch = c.get("channels")
        if isinstance(ch, bool) or ch not in (1, 2):
            raise ValueError(f"cable {cid}: channels must be 1 or 2, got {ch!r}")
        first = c.get("first_channel")
        if isinstance(first, bool) or not isinstance(first, int) or first < 1:
            raise ValueError(f"cable {cid}: first_channel must be >= 1, got {first!r}")
        if first + ch - 1 > cap_ch:
            raise ValueError(
                f"cable {cid}: first_channel {first} + channels {ch} exceeds capture_channels {cap_ch}")
        cables.append(Cable(i, cid, name, ch, first))
    return CableConfig(sample_rate, bits, cap_ch, cap_name, tuple(cables))
