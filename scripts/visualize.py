#!/usr/bin/env python3
"""Visualize SWAR benchmark results and packing density."""

import json
import re
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = Path(__file__).parent.parent / "results"
OUTPUT_DIR = RESULTS_DIR


def plot_density():
    """Bar chart: packing efficiency for each bit-width N = 5..14."""
    ns = list(range(5, 15))
    lanes = [64 // n for n in ns]
    bits_used = [l * n for l, n in zip(lanes, ns)]
    efficiency = [bu / 64.0 * 100 for bu in bits_used]
    wasted = [64 - bu for bu in bits_used]

    fig, ax1 = plt.subplots(figsize=(10, 5))

    x = np.arange(len(ns))
    bars = ax1.bar(x, efficiency, color="steelblue", edgecolor="black", alpha=0.85)

    ax1.set_xlabel("Bit-width (N)")
    ax1.set_ylabel("Packing efficiency (%)")
    ax1.set_title("SWAR Packing Density: N-bit integers in uint64_t")
    ax1.set_xticks(x)
    ax1.set_xticklabels([str(n) for n in ns])
    ax1.set_ylim(0, 105)

    # Annotate bars with lanes and wasted bits
    for i, (bar, l, w) in enumerate(zip(bars, lanes, wasted)):
        ax1.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 1,
            f"{l} lanes\n{w}b wasted",
            ha="center",
            va="bottom",
            fontsize=8,
        )

    ax1.axhline(y=100, color="gray", linestyle="--", alpha=0.5)
    plt.tight_layout()
    out = OUTPUT_DIR / "density.png"
    plt.savefig(out, dpi=150)
    print(f"Saved {out}")


def parse_bench_name(name: str):
    """Extract operation and N from benchmark name like 'BM_Broadcast<5>'."""
    m = re.match(r"BM_(\w+)<(\d+)>", name)
    if m:
        return m.group(1), int(m.group(2))
    return None, None


def plot_throughput(bench_file: Path):
    """Line chart: ns/op for each operation across N values."""
    with open(bench_file) as f:
        data = json.load(f)

    benchmarks = data.get("benchmarks", [])
    if not benchmarks:
        print("No benchmark data found!")
        return

    # Group by operation
    ops: dict[str, dict[int, float]] = {}
    for bm in benchmarks:
        op, n = parse_bench_name(bm["name"])
        if op is None:
            continue
        ops.setdefault(op, {})[n] = bm["real_time"]

    fig, ax = plt.subplots(figsize=(10, 6))

    markers = ["o", "s", "^", "D", "v", "P", "X"]
    for i, (op, values) in enumerate(sorted(ops.items())):
        ns_sorted = sorted(values.keys())
        times = [values[n] for n in ns_sorted]
        ax.plot(
            ns_sorted,
            times,
            marker=markers[i % len(markers)],
            label=op,
            linewidth=1.5,
            markersize=6,
        )

    ax.set_xlabel("Bit-width (N)")
    ax.set_ylabel("Time (ns)")
    ax.set_title("SWAR Operation Throughput by Bit-width")
    ax.set_xticks(list(range(5, 15)))
    ax.legend(loc="best", fontsize=8)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    out = OUTPUT_DIR / "throughput.png"
    plt.savefig(out, dpi=150)
    print(f"Saved {out}")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Always generate density chart (no benchmark data needed)
    plot_density()

    # Generate throughput chart if benchmark results exist
    bench_file = RESULTS_DIR / "bench_results.json"
    if bench_file.exists():
        plot_throughput(bench_file)
    else:
        print(
            f"No benchmark results at {bench_file}. "
            "Run: ./build/swar_bench --benchmark_format=json "
            "> results/bench_results.json"
        )


if __name__ == "__main__":
    main()
    plt.show()
