"""Tkinter level meter: one row per GP cable, shared-mode WASAPI capture."""
from __future__ import annotations

import argparse
import time
import tkinter as tk
from collections.abc import Mapping

from gpcable.config import Cable, CableConfig, default_config_path, load_config
from gpcable.devices import CableDevices, find_cable_devices
from gpcable.levels import FLOOR_DB, PeakHold, classify, peak_dbfs, rms_dbfs

SAMPLE_RATE = 48000
BAR_LO, BAR_HI = -60.0, 0.0
BAR_W, BAR_H = 360, 16
REFRESH_MS = 33
NOT_FOUND = "tidak ditemukan"


def bar_fraction(db: float, lo: float = BAR_LO, hi: float = BAR_HI) -> float:
    """Map a dB value onto 0..1 for the bar, clamped."""
    return min(1.0, max(0.0, (db - lo) / (hi - lo)))


def plan_streams(
    cfg: CableConfig, found: Mapping[str, CableDevices]
) -> list[tuple[Cable, int | None]]:
    """Pair every configured cable with its capture device index (or None)."""
    plan = []
    for cable in cfg.cables:
        dev = found.get(cable.id)
        plan.append((cable, dev.input_index if dev else None))
    return plan


def row_status(found: bool, peak_db: float) -> str:
    """Status text from the instantaneous block peak (not the held value)."""
    return classify(peak_db) if found else NOT_FOUND


def _fmt_db(db: float) -> str:
    return "-inf" if db <= FLOOR_DB else f"{db:.1f}"


class MeterApp:
    def __init__(self, root: tk.Tk, cfg: CableConfig):
        self.root = root
        self.cfg = cfg
        n = len(cfg.cables)
        self.streams: list = [None] * n
        self.peak = [FLOOR_DB] * n  # written by audio callbacks, read by UI
        self.rms = [FLOOR_DB] * n
        self.holds = [PeakHold() for _ in range(n)]
        self.found = [False] * n

        root.title("GP Virtual Cable - Meter")
        root.protocol("WM_DELETE_WINDOW", self.close)
        frame = tk.Frame(root, padx=8, pady=8)
        frame.pack()
        self.canvases, self.bars, self.num_labels, self.status_labels = [], [], [], []
        for r, cable in enumerate(cfg.cables):
            tk.Label(frame, text=cable.name, width=14, anchor="w").grid(row=r, column=0)
            cv = tk.Canvas(frame, width=BAR_W, height=BAR_H, bg="#202020",
                           highlightthickness=0)
            cv.grid(row=r, column=1, padx=6, pady=2)
            self.bars.append(cv.create_rectangle(0, 0, 0, BAR_H, fill="#3c3", width=0))
            self.canvases.append(cv)
            num = tk.Label(frame, text="", width=7, anchor="e")
            num.grid(row=r, column=2)
            st = tk.Label(frame, text=NOT_FOUND, width=16, anchor="w")
            st.grid(row=r, column=3)
            self.num_labels.append(num)
            self.status_labels.append(st)
        self.error_var = tk.StringVar()
        tk.Label(frame, textvariable=self.error_var, fg="#c33", wraplength=520,
                 justify="left").grid(row=len(cfg.cables) + 1, column=0, columnspan=4)
        tk.Button(frame, text="Scan ulang", command=self.rescan).grid(
            row=len(cfg.cables), column=0, columnspan=4, pady=(8, 0))
        self.rescan()
        self._job = root.after(REFRESH_MS, self.refresh)

    def _make_callback(self, i: int):
        def callback(indata, frames, time_info, status):
            self.peak[i] = peak_dbfs(indata)
            self.rms[i] = rms_dbfs(indata)
        return callback

    def _close_streams(self) -> None:
        for i, s in enumerate(self.streams):
            if s is not None:
                try:
                    s.stop()
                    s.close()
                except Exception:
                    pass
            self.streams[i] = None
            self.found[i] = False
            self.peak[i] = self.rms[i] = FLOOR_DB
            self.holds[i] = PeakHold()

    def rescan(self) -> None:
        import sounddevice as sd

        self._close_streams()
        found: dict[str, CableDevices] = {}
        try:
            # PortAudio caches the device list; re-init so new devices appear.
            sd._terminate()
            sd._initialize()
            found = find_cable_devices(sd.query_devices(), sd.query_hostapis(), self.cfg)
            self.error_var.set("")
        except Exception as e:
            self.error_var.set(f"Scan gagal: {e}")
        for i, (cable, idx) in enumerate(plan_streams(self.cfg, found)):
            if idx is None:
                continue
            try:
                s = sd.InputStream(
                    device=idx, channels=cable.channels, samplerate=SAMPLE_RATE,
                    dtype="float32", extra_settings=sd.WasapiSettings(exclusive=False),
                    callback=self._make_callback(i))
                s.start()
            except Exception:
                continue
            self.streams[i] = s
            self.found[i] = True

    def refresh(self) -> None:
        now = time.monotonic()
        for i in range(len(self.cfg.cables)):
            if not self.found[i]:
                self.canvases[i].coords(self.bars[i], 0, 0, 0, BAR_H)
                self.num_labels[i].config(text="")
                self.status_labels[i].config(text=NOT_FOUND)
                continue
            peak = self.peak[i]
            held = self.holds[i].update(peak, now)
            self.canvases[i].coords(self.bars[i], 0, 0, bar_fraction(peak) * BAR_W, BAR_H)
            self.num_labels[i].config(text=_fmt_db(held))
            self.status_labels[i].config(text=row_status(True, peak))
        self._job = self.root.after(REFRESH_MS, self.refresh)

    def close(self) -> None:
        self.root.after_cancel(self._job)
        self._close_streams()
        self.root.destroy()


def main(argv: list[str] | None = None) -> None:
    ap = argparse.ArgumentParser(description="GP Virtual Cable level meter")
    ap.add_argument("--exit-after", type=float, default=None, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)
    cfg = load_config(default_config_path())
    root = tk.Tk()
    app = MeterApp(root, cfg)
    if args.exit_after is not None:
        root.after(int(args.exit_after * 500), app.rescan)  # exercise rescan path
        root.after(int(args.exit_after * 1000), app.close)
    root.mainloop()


if __name__ == "__main__":
    main()
