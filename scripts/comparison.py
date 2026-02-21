#!/usr/bin/env python3
"""Visualize comparison benchmarks: PackedSet vs std containers."""

import json
import re
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = Path(__file__).parent.parent / "results"
BENCH_FILE = RESULTS_DIR / "comparison_results.json"


def parse_name(name: str):
    """Extract (operation, container, size) from e.g. 'BM_Insert_PackedSet/50'."""
    m = re.match(r"BM_(\w+?)_(PackedSet|StdSet|UnorderedSet|Vector|Array)/(\d+)", name)
    if m:
        return m.group(1), m.group(2), int(m.group(3))
    return None, None, None


CONTAINER_LABELS = {
    "PackedSet": "PackedSet<11>",
    "StdSet": "std::set",
    "UnorderedSet": "std::unordered_set",
    "Vector": "std::vector",
    "Array": "std::array",
}

CONTAINER_ORDER = ["PackedSet", "StdSet", "UnorderedSet", "Vector", "Array"]


def plot_operation(ax, op_name: str, data: dict, title: str):
    """Plot one operation: lines per container, x=size, y=time."""
    markers = ["o", "s", "^", "D", "v"]
    for i, cname in enumerate(CONTAINER_ORDER):
        if cname not in data:
            continue
        sizes = sorted(data[cname].keys())
        times = [data[cname][s] for s in sizes]
        ax.plot(
            sizes,
            times,
            marker=markers[i],
            label=CONTAINER_LABELS[cname],
            linewidth=1.5,
            markersize=5,
        )
    ax.set_xlabel("Set size")
    ax.set_ylabel("Time (ns)")
    ax.set_title(title)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3, which="both")


def run_benchmark():
    """Build and run comparison_bench, writing JSON to BENCH_FILE."""
    project_root = Path(__file__).parent.parent
    bench_bin = project_root / "build" / "comparison_bench"

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
            str(bench_bin),
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

    # Group: ops[operation][container][size] = time_ns
    ops: dict[str, dict[str, dict[int, float]]] = {}
    for bm in raw.get("benchmarks", []):
        op, container, size = parse_name(bm["name"])
        if op is None:
            continue
        ops.setdefault(op, {}).setdefault(container, {})[size] = bm["real_time"]

    titles = {
        "Insert": "Insert Throughput (N=11)",
        "Contains": "Contains (hit) Latency (N=11)",
        "ContainsMiss": "Contains (miss) Latency (N=11)",
    }

    fig, axes = plt.subplots(1, len(ops), figsize=(6 * len(ops), 5))
    if len(ops) == 1:
        axes = [axes]

    for ax, (op, data) in zip(axes, sorted(ops.items())):
        plot_operation(ax, op, data, titles.get(op, op))

    fig.suptitle("PackedSet<11> vs Standard Containers", fontsize=14, y=1.02)
    plt.tight_layout()

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out = RESULTS_DIR / "comparison.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved {out}")

    subprocess.Popen(["google-chrome", str(out)])


if __name__ == "__main__":
    main()
