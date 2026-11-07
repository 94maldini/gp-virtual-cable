"""Load and validate cables.toml (single source of truth for the cable list)."""
from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass
from pathlib import Path

MAX_CABLES = 32
_ID_RE = re.compile(r"[A-Za-z][A-Za-z0-9]*")


@dataclass(frozen=True)
class Cable:
    index: int
    id: str
    name: str
    channels: int


@dataclass(frozen=True)
class CableConfig:
    sample_rate: int
    bits: int
    cables: tuple[Cable, ...]


def default_config_path() -> Path:
    return Path(__file__).resolve().parents[2] / "cables.toml"


def load_config(path: Path) -> CableConfig:
    with open(path, "rb") as f:
        data = tomllib.load(f)
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
        cables.append(Cable(i, cid, name, ch))
    return CableConfig(
        sample_rate=int(data.get("sample_rate", 48000)),
        bits=int(data.get("bits", 24)),
        cables=tuple(cables),
    )
