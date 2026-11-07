"""Locate the WASAPI input/output devices that belong to each GP cable."""
from __future__ import annotations

import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass

from gpcable.config import CableConfig

WASAPI = "Windows WASAPI"


@dataclass(frozen=True)
class CableDevices:
    input_index: int | None
    output_index: int | None


def _name_regex(name: str) -> re.Pattern[str]:
    # Whole-word match so "GP Mono 1" does not match "GP Mono 10".
    return re.compile(r"(?<![0-9A-Za-z])" + re.escape(name) + r"(?![0-9A-Za-z])")


def find_cable_devices(
    devices: Sequence[Mapping],
    hostapis: Sequence[Mapping],
    cfg: CableConfig,
) -> dict[str, CableDevices]:
    wasapi = {i for i, h in enumerate(hostapis) if h.get("name") == WASAPI}
    result: dict[str, CableDevices] = {}
    for cable in cfg.cables:
        rx = _name_regex(cable.name)
        inp: int | None = None
        out: int | None = None
        for idx, d in enumerate(devices):
            if d.get("hostapi") not in wasapi or not rx.search(d.get("name", "")):
                continue
            if inp is None and d.get("max_input_channels", 0) > 0:
                inp = idx
            if out is None and d.get("max_output_channels", 0) > 0:
                out = idx
        result[cable.id] = CableDevices(input_index=inp, output_index=out)
    return result
