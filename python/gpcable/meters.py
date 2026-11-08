"""Per-endpoint output level from Windows Core Audio (IAudioMeterInformation).

No audio stream is opened, so this never conflicts with an exclusive-mode
client on the Recording endpoint. All COM calls must happen on the calling
(tkinter) thread; comtypes initialises COM for that thread on import.
"""
from __future__ import annotations

from collections.abc import Sequence
from ctypes import POINTER, cast

import comtypes
from pycaw.pycaw import DEVICE_STATE, EDataFlow, IAudioMeterInformation
from pycaw.utils import AudioUtilities

from gpcable.devices import name_matches


class RenderMeter:
    def __init__(self, name: str, iface) -> None:
        self.name = name
        self._iface = iface

    def peak(self) -> float:
        """Current linear peak 0..1; raises on COM error (caller treats as not found)."""
        return float(self._iface.GetPeakValue())


def _activate_meter(device) -> RenderMeter | None:
    try:
        raw = device._dev.Activate(IAudioMeterInformation._iid_, comtypes.CLSCTX_ALL, None)
        iface = cast(raw, POINTER(IAudioMeterInformation))
        iface.GetPeakValue()  # probe: fail now rather than mid-run
        return RenderMeter(device.FriendlyName, iface)
    except Exception:
        return None


def open_render_meters(names: Sequence[str]) -> dict[str, RenderMeter | None]:
    """Map each wanted name to a meter on the first active render endpoint matching it."""
    result: dict[str, RenderMeter | None] = {n: None for n in names}
    try:
        devices = AudioUtilities.GetAllDevices(
            data_flow=EDataFlow.eRender.value,
            device_state=DEVICE_STATE.ACTIVE.value)
    except Exception:
        return result
    for wanted in names:
        for dev in devices:
            try:
                friendly = dev.FriendlyName or ""
            except Exception:
                continue
            if name_matches(friendly, wanted):
                result[wanted] = _activate_meter(dev)
                break
    return result
