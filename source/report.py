#!/usr/bin/env python3
"""
Generate a report from simulation log files.

Each log line must contain four integers:
    blkerr  biterrs  enctime  dectime

Summary files may be extension-less or have a .out extension.

Example
-------
python report.py --bits-per-block 1024 run1.log run2.log
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages          # NEW

import math  # NEW


def parse_logs(*paths: str | Path) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Return blkerr, biterrs, enc_t, dec_t as NumPy arrays."""
    blkerr_lst, biterrs_lst, enc_lst, dec_lst = [], [], [], []

    # First resolve every argument into an explicit list of log files
    files: list[Path] = []
    for raw in paths:
        p = Path(raw).expanduser()
        if p.is_dir():
            for child in p.iterdir():
                if not child.is_file():
                    continue
                suf = child.suffix.lower()

                if suf == ".txt":                 # ignore *.txt
                    continue

                if suf == ".out":                 # always keep *.out (they ARE the stub)
                    files.append(child)
                    continue

                if suf in (".log", ""):           # .log or extension-less → keep only
                    stub  = child.stem            # when companion stub.out exists
                    if not child.with_name(stub + ".out").is_file():
                        continue
                    files.append(child)
            continue        # directory handled – move to next argument
        else:
            if not p.is_file():
                raise FileNotFoundError(f"Log file not found: {p}")
            files.append(p)

    if not files:
        raise ValueError("No log files found after filtering.")

    # Now parse every discovered file
    for p in files:
        # tolerate odd encodings → silently drop undecodable bytes
        print(f"Parsing log file: {p}")
        with p.open(encoding="utf-8", errors="ignore") as fh:
            n_from_file = 0
            for line in fh:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()

                # ---- recognise line formats ---------------------------------
                if len(parts) == 4:                       # blkerr biterrs enc dec
                    try:
                        be, bi        = map(int,   parts[:2])
                        et, dt        = map(float, parts[2:4])
                    except ValueError:
                        continue

                elif len(parts) >= 8:                     # k n esno nblk blk bit enc dec
                    try:
                        be, bi        = map(int,   parts[4:6])
                        et, dt        = map(float, parts[6:8])
                    except ValueError:
                        continue
                else:
                    continue
                # ----------------------------------------------------------------

                blkerr_lst.append(be)
                biterrs_lst.append(bi)
                enc_lst.append(et)
                dec_lst.append(dt)
                n_from_file += 1

            if n_from_file == 0:
                print(f"  ⚠️  no valid log lines found in {p}")

    if not blkerr_lst:
        raise ValueError("No valid log entries parsed – nothing to plot.")
    return (
        np.asarray(blkerr_lst, dtype=np.int32),
        np.asarray(biterrs_lst, dtype=np.int32),
        np.asarray(enc_lst, dtype=np.float32),
        np.asarray(dec_lst, dtype=np.float32),
    )


# --- single–file front-end -----------------------------------------
def parse_one_log(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Wrapper that calls parse_logs() for exactly one file-path."""
    return parse_logs(path)                       # existing routine already accepts

def calc_rates(blkerr: np.ndarray, biterrs: np.ndarray, bits_per_block: int | None = None) -> tuple[float, float]:
    """Compute BLER and BER."""
    if len(blkerr) == 0:
        raise ValueError("Empty input – cannot compute rates.")
    bler = float(blkerr.mean())
    if bits_per_block and bits_per_block > 0:
        ber = biterrs.sum() / (bits_per_block * len(blkerr))
    else:
        ber = float(biterrs.mean())  # fallback
    return bler, ber


def plot_hist(
    enc_t: np.ndarray,
    dec_t: np.ndarray,
    bler: float,
    ber: float,
    *,
    bins: int = 50,
    stub: str | None = None,
    show: bool = True,
) -> plt.Figure:
    """
    Create histograms for encoding / decoding times and return the Figure.

    If `show` is True (default) the plot window is displayed.
    When `stub` is provided it is used as a figure title – handy for PDF pages.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))

    ax1.hist(enc_t, bins=bins, color="steelblue", edgecolor="black", alpha=0.7)
    ax1.set_title("Encoding time distribution")
    ax1.set_xlabel("Time (µs)")
    ax1.set_ylabel("Count")

    ax2.hist(dec_t, bins=bins, color="salmon", edgecolor="black", alpha=0.7)
    ax2.set_title("Decoding time distribution")
    ax2.set_xlabel("Time (µs)")
    ax2.set_ylabel("Count")

    txt = f"BLER = {bler:.4f}\nBER  = {ber:.4e}"
    fig.text(0.92, 0.5, txt, transform=fig.transFigure, fontsize=12, va="center", ha="left")

    if stub:
        fig.suptitle(stub)
    plt.tight_layout(rect=[0, 0, 0.9, 1])
    if show:
        plt.show()
    return fig

def _grid_hists(
    stub_data: dict[str, tuple[np.ndarray, np.ndarray, float, float]],
    *,
    bins: int = 50,
) -> tuple[plt.Figure, plt.Figure]:
    """
    Return two figures:

    • fig_enc  – grid of encoding-time histograms
    • fig_dec  – grid of decoding-time histograms
    Every subplot is titled with its stub name and annotated with BLER/BER.
    """
    n = len(stub_data)
    cols = 3 if n > 4 else 2
    rows = math.ceil(n / cols)

    fig_enc, axes_enc = plt.subplots(rows, cols, figsize=(4 * cols, 3 * rows))
    fig_dec, axes_dec = plt.subplots(rows, cols, figsize=(4 * cols, 3 * rows))

    # always get 2-D arrays for uniform indexing
    axes_enc = np.atleast_2d(axes_enc)
    axes_dec = np.atleast_2d(axes_dec)

    for idx, (stub, (enc, dec, bler, ber)) in enumerate(sorted(stub_data.items())):
        r, c = divmod(idx, cols)

        ax_e = axes_enc[r, c]
        ax_e.hist(enc, bins=bins, color="steelblue", edgecolor="black", alpha=0.7)
        ax_e.set_title(stub)
        ax_e.set_xlabel("Enc time (µs)")
        ax_e.set_ylabel("Cnt")
        ax_e.text(
            0.98,
            0.95,
            f"BLER={bler:.4f}\nBER={ber:.3e}",
            transform=ax_e.transAxes,
            ha="right",
            va="top",
            fontsize=8,
            bbox=dict(boxstyle="round", fc="white", ec="gray", alpha=0.8),
        )

        ax_d = axes_dec[r, c]
        ax_d.hist(dec, bins=bins, color="salmon", edgecolor="black", alpha=0.7)
        ax_d.set_title(stub)
        ax_d.set_xlabel("Dec time (µs)")
        ax_d.set_ylabel("Cnt")
        ax_d.text(
            0.98,
            0.95,
            f"BLER={bler:.4f}\nBER={ber:.3e}",
            transform=ax_d.transAxes,
            ha="right",
            va="top",
            fontsize=8,
            bbox=dict(boxstyle="round", fc="white", ec="gray", alpha=0.8),
        )

    # hide unused axes
    for fig, axes in ((fig_enc, axes_enc), (fig_dec, axes_dec)):
        for ax in axes.flat[n:]:
            ax.set_visible(False)
        fig.tight_layout()

    return fig_enc, fig_dec

# ---------- summary-file support -------------------------------------------
def parse_summary_files(*paths: str | Path,
                        k: int | None = None,
                        n: int | None = None
                        ) -> dict[str, tuple[float, float]]:
    """
    Return {stub : (esno, avg_dec_time)} taken from “summary” files.

    • Every *paths* item may be either
        – a file               → parsed directly
        – a directory          → all files inside whose extension is “” or
                                 “.out” are parsed
    • If k and/or n are given, only lines that match those values are used.
    """

    def consume_file(p: Path, store: dict[str, tuple[float, float]]) -> None:
        """Read one summary file and update *store* (at most one entry per file)."""
        stub = p.stem                       # filename without extension
        with p.open(encoding="utf-8", errors="ignore") as fh:
            for line in fh:
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                parts = line.split()
                if len(parts) < 8:          # need k n esno n_blk blkerr biterr avg_enc avg_dec
                    continue
                try:
                    k_i, n_i = map(int, parts[:2])
                except ValueError:
                    continue          # skip non-numeric line
                if (k is not None and k_i != k) or (n is not None and n_i != n):
                    continue
                esno      = float(parts[2])
                avg_dec_t = float(parts[7])
                store[stub] = (esno, avg_dec_t)
                break                       # only first matching line per file

    result: dict[str, tuple[float, float]] = {}

    for raw in paths:
        p = Path(raw).expanduser()
        if p.is_dir():
            # scan directory for candidate summary files
            for child in p.iterdir():
                if child.is_file() and child.suffix == ".out":
                    consume_file(child, result)
        elif p.is_file():
            consume_file(p, result)
        # non-existent paths are silently ignored (consistent with previous behaviour)

    return result


def plot_esno_vs_dec(data: dict[str, tuple[float, float]]) -> plt.Figure:
    """Scatter plot of Es/No vs avg decoding time – one point per stub."""
    fig, ax = plt.subplots(figsize=(6, 4))
    for stub, (esno, dec_t) in data.items():
        ax.scatter(esno, dec_t, label=stub)
        ax.annotate(stub, (esno, dec_t), textcoords="offset points",
                    xytext=(5, -5), ha="left", fontsize=9)
    ax.set_xlabel("Es/No (dB)")
    ax.set_ylabel("Average decoding time (µs)")
    ax.set_title("Decoder comparison")
    ax.grid(True, ls="--", alpha=0.4)
    return fig
# ---------------------------------------------------------------------------

def plot_dec_hist(dec_t: np.ndarray, bler: float, ber: float,
                  stub: str, bins: int = 50) -> plt.Figure:
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.hist(dec_t, bins=bins, color="salmon", edgecolor="black", alpha=0.7)
    ax.set_title(f"{stub} – Decoding time")
    ax.set_xlabel("Time (µs)")
    ax.set_ylabel("Count")
    txt = f"BLER = {bler:.4f}\nBER  = {ber:.4e}"
    ax.text(0.95, 0.95, txt, transform=ax.transAxes, ha="right", va="top",
            fontsize=10, bbox=dict(boxstyle="round", fc="white", ec="gray", alpha=0.8))
    return fig


def find_logs_by_stub(k: int, n: int,
                      exts: tuple[str, ...] = ("", ".log", ".txt"),
                      directory: Path | str = ".") -> dict[str, list[Path]]:
    """
    Return {stubname: [Path, …]} for every file that matches '*_<k>_<n>*.<ext>'.
    The stubname is the part before '_<k>_<n>'.
    """
    dirpath = Path(directory).expanduser()
    stub_map: dict[str, list[Path]] = {}
    for ext in exts:
        pattern = f"*_{k}_{n}*{ext}" if ext else f"*_{k}_{n}*"
        for p in dirpath.glob(pattern):
            if not p.is_file():
                continue
            stem = p.stem
            stub = stem.split(f"_{k}_{n}", 1)[0] or stem      # derive stub
            stub_map.setdefault(stub, []).append(p)
    return stub_map

def main() -> None:
    parser = argparse.ArgumentParser(description="Parse simulation logs and plot statistics.")
    parser.add_argument("logs", nargs="*", help="Path(s) to log files")   # CHANGED ‘+’ → ‘*’
    parser.add_argument("--bits-per-block", type=int, default=None, help="Bits per block (optional)")
    parser.add_argument("--bins", type=int, default=50, help="Histogram bin count")
    parser.add_argument("--k",  type=int, help="Number of information bits")      # NEW
    parser.add_argument("--n",  type=int, help="Number of coded bits")            # NEW
    parser.add_argument("--out", type=str, help="Output PDF path")                # NEW
    parser.add_argument("--summary", action="store_true",
                        help="Plot Es/No vs avg-decoding-time from summary files")
    args = parser.parse_args()

    # SUMMARY scatter mode -------------------------------------------------
    if args.summary:
        # if no files supplied, take every .out file in cwd
        summary_files = (
            args.logs or
            [p for p in Path(".").iterdir() if p.is_file() and p.suffix == ".out"]
        )
        if not summary_files:
            parser.error("No summary files provided or found.")
        summary = parse_summary_files(*summary_files, k=args.k, n=args.n)
        if not summary:
            parser.error("No matching entries found in summary files.")
        fig = plot_esno_vs_dec(summary)
        if args.out:
            out_path = Path(args.out)
            if out_path.suffix.lower() == ".pdf":
                with PdfPages(out_path) as pdf:
                    pdf.savefig(fig)
            else:
                fig.savefig(out_path)
            plt.close(fig)
            print(f"Wrote scatter plot to {out_path}")
        else:
            plt.show()
        return
    # -------------------------------------------------------------------

    # NEW block – automatic search mode ---------------------------------
    if args.k is not None and args.n is not None and not args.logs:
        stub_logs = find_logs_by_stub(args.k, args.n)
        if not stub_logs:
            parser.error(f"No log files matching '*_{args.k}_{args.n}*' found.")
        out_path = Path(args.out or f"histograms_{args.k}_{args.n}.pdf")
        with PdfPages(out_path) as pdf:
            stub_data: dict[str, tuple[np.ndarray, np.ndarray, float, float]] = {}
            for stub, files in sorted(stub_logs.items()):
                blk, bit, enc, dec = parse_logs(*files)
                bler, ber = calc_rates(blk, bit, args.bits_per_block)
                stub_data[stub] = (enc, dec, bler, ber)

            fig_enc, fig_dec = _grid_hists(stub_data, bins=args.bins)
            pdf.savefig(fig_dec)   # page 1 – decoding hists (often the key metric)
            pdf.savefig(fig_enc)   # page 2 – encoding  hists
            plt.close(fig_dec)
            plt.close(fig_enc)
        print(f"Wrote {len(stub_logs)} histograms to {out_path}")
        return
    # -------------------------------------------------------------------

    # ------------- collect data per-stub ----------------------------
    files = args.logs or ["."]
    stub_data: dict[str, tuple[np.ndarray, np.ndarray, float, float]] = {}

    for p_raw in files:
        p = Path(p_raw)
        # if directory was supplied pick eligible log files inside
        if p.is_dir():
            for child in p.iterdir():
                if not child.is_file() or child.suffix != ".out":
                    continue
                stub = child.stem
                blk, bit, enc, dec = parse_one_log(child)
                bler, ber          = calc_rates(blk, bit, args.bits_per_block)
                stub_data[stub]    = (enc, dec, bler, ber)
        else:                                   # explicit file given
            stub = p.stem
            blk, bit, enc, dec = parse_one_log(p)
            bler, ber          = calc_rates(blk, bit, args.bits_per_block)
            stub_data[stub]    = (enc, dec, bler, ber)

    if not stub_data:
        parser.error("No valid log files to plot.")

    # ------------- plotting / saving --------------------------------
    if len(stub_data) == 1:
        stub, (enc, dec, bler, ber) = next(iter(stub_data.items()))
        fig = plot_hist(enc, dec, bler, ber, bins=args.bins, stub=stub, show=not args.out)
        if args.out:
            fig.savefig(args.out)
            plt.close(fig)
            print(f"Wrote histogram to {args.out}")
    else:
        fig_enc, fig_dec = _grid_hists(stub_data, bins=args.bins)
        if args.out:
            with PdfPages(args.out) as pdf:
                pdf.savefig(fig_dec)    # decoding histograms first
                pdf.savefig(fig_enc)    # encoding histograms second
            plt.close(fig_dec)
            plt.close(fig_enc)
            print(f"Wrote {len(stub_data)} histograms to {args.out}")
        else:
            plt.show()


if __name__ == "__main__":
    main()
