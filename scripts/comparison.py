#!/usr/bin/env python3
"""Visualize comparison benchmarks: PackedSet vs std containers (fixed size=10)."""

import json
import re
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = Path(__file__).parent.parent / "results"
BENCH_FILE = RESULTS_DIR / "comparison_results.json"

CONTAINER_LABELS = {
    "PackedSet": "PackedSet<11>",
    "StdSet": "std::set",
    "UnorderedSet": "std::unordered_set",
    "Vector": "std::vector",
    "SortedVector": "sorted vector",
    "Array": "std::array",
}

CONTAINER_ORDER = ["PackedSet", "StdSet", "UnorderedSet", "Vector", "SortedVector", "Array"]
COLORS = ["#2196F3", "#FF9800", "#4CAF50", "#E91E63", "#673AB7", "#9C27B0"]


def parse_name(name: str):
    """Extract (operation, container) from e.g. 'BM_Insert_PackedSet'."""
    m = re.match(r"BM_(\w+?)_(PackedSet|StdSet|UnorderedSet|SortedVector|Vector|Array)$", name)
    if m:
        return m.group(1), m.group(2)
    return None, None


def plot_operation(ax, data: dict, title: str):
    """Horizontal bar chart: one bar per container, x=time(ns)."""
    y = np.arange(len(CONTAINER_ORDER))
    times = [data.get(c, 0) for c in CONTAINER_ORDER]
    labels = [CONTAINER_LABELS[c] for c in CONTAINER_ORDER]

    bars = ax.barh(y, times, color=COLORS, edgecolor="black", alpha=0.85)
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=9)
    ax.set_xlabel("Time (ns)")
    ax.set_title(title, fontsize=11)
    ax.invert_yaxis()

    for bar, t in zip(bars, times):
        ax.text(
            bar.get_width(),
            bar.get_y() + bar.get_height() / 2,
            f"  {t:.1f}",
            ha="left",
            va="center",
            fontsize=8,
        )


def run_benchmark():
    """Build and run comparison_bench, writing JSON to BENCH_FILE."""
    project_root = Path(__file__).parent.parent

    print("Building...")
    subprocess.run(
        ["cmake", "--build", "build", "-j", str(len(__import__("os").sched_getaffinity(0)))],
        cwd=project_root,
        check=True,
        capture_output=True,
    )

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    print("Running comparison_bench...\n", flush=True)
    subprocess.run(
        [
            str(project_root / "build" / "comparison_bench"),
            "--benchmark_format=console",
            f"--benchmark_out={BENCH_FILE}",
            "--benchmark_out_format=json",
        ],
        check=True,
    )
    print(f"\nWrote {BENCH_FILE}")


def main():
    run_benchmark()

    with open(BENCH_FILE) as f:
        raw = json.load(f)

    # Group: ops[operation][container] = time_ns
    ops: dict[str, dict[str, float]] = {}
    bench_n = None
    bench_size = None
    for bm in raw.get("benchmarks", []):
        op, container = parse_name(bm["name"])
        if op is None:
            continue
        ops.setdefault(op, {})[container] = bm["real_time"]
        if bench_n is None and "N" in bm:
            bench_n = int(bm["N"])
        if bench_size is None and "size" in bm:
            bench_size = int(bm["size"])

    n_str = str(bench_n) if bench_n is not None else "?"
    sz_str = str(bench_size) if bench_size is not None else "?"

    titles = {
        "Insert": f"Insert {sz_str} Elements (N={n_str})",
        "Contains": f"Contains — Hit (N={n_str}, size={sz_str})",
        "ContainsMiss": f"Contains — Miss (N={n_str}, size={sz_str})",
        "Erase": f"Erase (N={n_str}, size={sz_str})",
    }

    n_ops = len(ops)
    fig, axes = plt.subplots(n_ops, 1, figsize=(8, 3.5 * n_ops))
    if n_ops == 1:
        axes = [axes]

    for ax, (op, data) in zip(axes, sorted(ops.items())):
        plot_operation(ax, data, titles.get(op, op))

    fig.suptitle(f"PackedSet<{n_str}> vs Standard Containers (size={sz_str})", fontsize=14)
    plt.tight_layout(rect=[0, 0, 1, 0.97])

    out = RESULTS_DIR / "comparison.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved {out}")

    subprocess.Popen(["google-chrome", str(out)])


if __name__ == "__main__":
    main()
