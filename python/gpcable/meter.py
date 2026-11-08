"""Tkinter level meter: one row per GP Playback endpoint, level read from Windows
(IAudioMeterInformation). No audio stream is opened, so exclusive-mode clients are unaffected."""
from __future__ import annotations

import argparse
import time
import tkinter as tk

from gpcable.config import Cable, CableConfig, default_config_path, load_config
from gpcable.levels import FLOOR_DB, PeakHold, classify, linear_to_dbfs
from gpcable.meters import RenderMeter, open_render_meters

BAR_LO, BAR_HI = -60.0, 0.0
BAR_W, BAR_H = 360, 16
REFRESH_MS = 33
NOT_FOUND = "tidak ditemukan"


def bar_fraction(db: float, lo: float = BAR_LO, hi: float = BAR_HI) -> float:
    """Map a dB value onto 0..1 for the bar, clamped."""
    return min(1.0, max(0.0, (db - lo) / (hi - lo)))


def channel_label(cable: Cable) -> str:
    """Destination channel(s) on the 8-channel Recording, e.g. "ch 3" or "ch 3+4"."""
    first = cable.first_channel
    if cable.channels == 1:
        return f"ch {first}"
    return f"ch {first}+{first + cable.channels - 1}"


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
        self.meters: list[RenderMeter | None] = [None] * n
        self.holds = [PeakHold() for _ in range(n)]

        root.title("GP Virtual Cable - Meter")
        root.protocol("WM_DELETE_WINDOW", self.close)
        frame = tk.Frame(root, padx=8, pady=8)
        frame.pack()
        self.canvases, self.bars, self.num_labels, self.status_labels = [], [], [], []
        for r, cable in enumerate(cfg.cables):
            tk.Label(frame, text=cable.name, width=14, anchor="w").grid(row=r, column=0)
            tk.Label(frame, text=channel_label(cable), width=7, anchor="w").grid(row=r, column=1)
            cv = tk.Canvas(frame, width=BAR_W, height=BAR_H, bg="#202020",
                           highlightthickness=0)
            cv.grid(row=r, column=2, padx=6, pady=2)
            self.bars.append(cv.create_rectangle(0, 0, 0, BAR_H, fill="#3c3", width=0))
            self.canvases.append(cv)
            num = tk.Label(frame, text="", width=7, anchor="e")
            num.grid(row=r, column=3)
            st = tk.Label(frame, text=NOT_FOUND, width=16, anchor="w")
            st.grid(row=r, column=4)
            self.num_labels.append(num)
            self.status_labels.append(st)
        self.error_var = tk.StringVar()
        tk.Label(frame, textvariable=self.error_var, fg="#c33", wraplength=520,
                 justify="left").grid(row=len(cfg.cables) + 1, column=0, columnspan=5)
        tk.Button(frame, text="Scan ulang", command=self.rescan).grid(
            row=len(cfg.cables), column=0, columnspan=5, pady=(8, 0))
        self.rescan()
        self._job = self.root.after(REFRESH_MS, self.refresh)

    def rescan(self) -> None:
        n = len(self.cfg.cables)
        self.holds = [PeakHold() for _ in range(n)]
        self.meters = [None] * n
        try:
            found = open_render_meters([c.name for c in self.cfg.cables])
            self.meters = [found.get(c.name) for c in self.cfg.cables]
            self.error_var.set("")
        except Exception as e:
            self.error_var.set(f"Scan gagal: {e}")

    def refresh(self) -> None:
        now = time.monotonic()
        for i, meter in enumerate(self.meters):
            peak_db = FLOOR_DB
            if meter is not None:
                try:
                    peak_db = linear_to_dbfs(meter.peak())
                except Exception:
                    self.meters[i] = None  # endpoint vanished mid-run
                    meter = None
            if meter is None:
                self.canvases[i].coords(self.bars[i], 0, 0, 0, BAR_H)
                self.num_labels[i].config(text="")
                self.status_labels[i].config(text=row_status(False, FLOOR_DB))
                continue
            held = self.holds[i].update(peak_db, now)
            self.canvases[i].coords(self.bars[i], 0, 0, bar_fraction(peak_db) * BAR_W, BAR_H)
            self.num_labels[i].config(text=_fmt_db(held))
            self.status_labels[i].config(text=row_status(True, peak_db))
        self._job = self.root.after(REFRESH_MS, self.refresh)

    def close(self) -> None:
        self.root.after_cancel(self._job)
        self.meters = [None] * len(self.cfg.cables)
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
