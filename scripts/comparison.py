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
    "Array": "std::array",
}

CONTAINER_ORDER = ["PackedSet", "StdSet", "UnorderedSet", "Vector", "Array"]
COLORS = ["#2196F3", "#FF9800", "#4CAF50", "#E91E63", "#9C27B0"]


def parse_name(name: str):
    """Extract (operation, container) from e.g. 'BM_Insert_PackedSet'."""
    m = re.match(r"BM_(\w+?)_(PackedSet|StdSet|UnorderedSet|Vector|Array)$", name)
    if m:
        return m.group(1), m.group(2)
    return None, None


def plot_operation(ax, data: dict, title: str):
    """Bar chart: one bar per container, y=time(ns)."""
    x = np.arange(len(CONTAINER_ORDER))
    times = [data.get(c, 0) for c in CONTAINER_ORDER]
    labels = [CONTAINER_LABELS[c] for c in CONTAINER_ORDER]

    bars = ax.bar(x, times, color=COLORS, edgecolor="black", alpha=0.85)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=25, ha="right", fontsize=8)
    ax.set_ylabel("Time (ns)")
    ax.set_title(title)

    for bar, t in zip(bars, times):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            f"{t:.1f}",
            ha="center",
            va="bottom",
            fontsize=7,
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
    for bm in raw.get("benchmarks", []):
        op, container = parse_name(bm["name"])
        if op is None:
            continue
        ops.setdefault(op, {})[container] = bm["real_time"]

    titles = {
        "Insert": "Insert 10 Elements (N=11)",
        "Contains": "Contains — Hit (N=11, size=10)",
        "ContainsMiss": "Contains — Miss (N=11, size=10)",
    }

    fig, axes = plt.subplots(1, len(ops), figsize=(6 * len(ops), 5))
    if len(ops) == 1:
        axes = [axes]

    for ax, (op, data) in zip(axes, sorted(ops.items())):
        plot_operation(ax, data, titles.get(op, op))

    fig.suptitle("PackedSet<11> vs Standard Containers (size=10)", fontsize=14, y=1.02)
    plt.tight_layout()

    out = RESULTS_DIR / "comparison.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved {out}")

    subprocess.Popen(["google-chrome", str(out)])


if __name__ == "__main__":
    main()
