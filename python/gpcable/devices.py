"""Locate the WASAPI devices of the GP cable: 12 Playback outputs and 1 Recording input."""
from __future__ import annotations

import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass

from gpcable.config import CableConfig

WASAPI = "Windows WASAPI"


@dataclass(frozen=True)
class CableDeviceMap:
    outputs: dict[str, int | None]  # keyed by Cable.id, all ids present
    capture: int | None


def name_matches(device_name: str, wanted: str) -> bool:
    """Whole-word containment: "GP Mono 1" matches "Speakers (GP Mono 1)" but not "... Mono 10"."""
    rx = r"(?<![0-9A-Za-z])" + re.escape(wanted) + r"(?![0-9A-Za-z])"
    return re.search(rx, device_name) is not None


def find_cable_devices(
    devices: Sequence[Mapping],
    hostapis: Sequence[Mapping],
    cfg: CableConfig,
) -> CableDeviceMap:
    wasapi = {i for i, h in enumerate(hostapis) if h.get("name") == WASAPI}
    outputs: dict[str, int | None] = {}
    for cable in cfg.cables:
        outputs[cable.id] = next(
            (idx for idx, d in enumerate(devices)
             if d.get("hostapi") in wasapi
             and d.get("max_output_channels", 0) > 0
             and name_matches(d.get("name", ""), cable.name)),
            None,
        )
    capture = next(
        (idx for idx, d in enumerate(devices)
         if d.get("hostapi") in wasapi
         and d.get("max_input_channels", 0) > 0
         and name_matches(d.get("name", ""), cfg.capture_name)),
        None,
    )
    return CableDeviceMap(outputs=outputs, capture=capture)
