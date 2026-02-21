#!/usr/bin/env python3

import json
import sys
import argparse
from pathlib import Path
import numpy as np

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
except ImportError:
    print("Error: matplotlib not found. Install with: pip install matplotlib")
    sys.exit(1)


def load_results(filename):
    """Load JSON results file."""
    try:
        with open(filename, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {filename}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}")
        sys.exit(1)


def plot_latency_histograms(results, output_dir=None):
    """Plot latency histograms for each benchmark."""
    benchmarks = results.get('benchmarks', [])
    if not benchmarks:
        print("No benchmarks found in results")
        return

    fig, axes = plt.subplots(len(benchmarks), 1, figsize=(12, 4 * len(benchmarks)))
    if len(benchmarks) == 1:
        axes = [axes]

    for idx, bench in enumerate(benchmarks):
        ax = axes[idx]
        name = bench['name']

        percentiles = [
            ('p50', bench.get('p50_ns', 0)),
            ('p90', bench.get('p90_ns', 0)),
            ('p99', bench.get('p99_ns', 0)),
            ('p999', bench.get('p999_ns', 0))
        ]

        labels, values = zip(*percentiles)
        colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
        bars = ax.bar(labels, values, color=colors, alpha=0.7, edgecolor='black')

        ax.set_ylabel('Latency (ns)', fontsize=11)
        ax.set_title(f'{name.upper()} Latency Distribution', fontsize=12, fontweight='bold')
        ax.grid(axis='y', alpha=0.3)

        # Add value labels on bars
        for bar in bars:
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{height:.0f}',
                   ha='center', va='bottom', fontsize=9)

    plt.tight_layout()

    if output_dir:
        output_file = Path(output_dir) / 'latency_histograms.png'
    else:
        output_file = Path('latency_histograms.png')

    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_file}")


def plot_percentile_comparison(results, output_dir=None):
    """Compare percentiles across benchmarks."""
    benchmarks = results.get('benchmarks', [])
    if not benchmarks:
        print("No benchmarks found in results")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    percentiles = ['p50', 'p90', 'p99', 'p999']
    x = np.arange(len(percentiles))
    width = 0.8 / len(benchmarks)

    for idx, bench in enumerate(benchmarks):
        name = bench['name']
        values = [bench.get(f'{p}_ns', 0) for p in percentiles]
        offset = width * (idx - len(benchmarks) / 2 + 0.5)
        ax.bar(x + offset, values, width, label=name.upper(), alpha=0.8)

    ax.set_xlabel('Percentile', fontsize=11)
    ax.set_ylabel('Latency (ns)', fontsize=11)
    ax.set_title('Latency Percentile Comparison', fontsize=12, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(percentiles)
    ax.legend()
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()

    if output_dir:
        output_file = Path(output_dir) / 'percentile_comparison.png'
    else:
        output_file = Path('percentile_comparison.png')

    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_file}")


def print_summary_table(results):
    """Print a text table of results."""
    benchmarks = results.get('benchmarks', [])
    if not benchmarks:
        print("No benchmarks found in results")
        return

    print("\n" + "=" * 100)
    print("SYSCALL LATENCY PROFILER RESULTS")
    print("=" * 100)

    for bench in benchmarks:
        print(f"\n{bench['name'].upper()}")
        print("-" * 100)
        print(f"  Iterations:  {bench.get('iterations', 0):,}")
        print(f"  Duration:    {bench.get('duration_sec', 0):.3f}s")
        print(f"  Min:         {bench.get('min_ns', 0):,} ns")
        print(f"  Max:         {bench.get('max_ns', 0):,} ns")
        print(f"  P50:         {bench.get('p50_ns', 0):.1f} ns")
        print(f"  P90:         {bench.get('p90_ns', 0):.1f} ns")
        print(f"  P99:         {bench.get('p99_ns', 0):.1f} ns")
        print(f"  P999:        {bench.get('p999_ns', 0):.1f} ns")

    print("\n" + "=" * 100 + "\n")


def main():
    parser = argparse.ArgumentParser(description='Plot syscall latency benchmark results')
    parser.add_argument('input', nargs='?', default='results.json',
                       help='Input JSON results file (default: results.json)')
    parser.add_argument('-o', '--output-dir', default=None,
                       help='Output directory for plots (default: current directory)')
    parser.add_argument('-s', '--summary', action='store_true',
                       help='Print summary table to stdout')
    parser.add_argument('-p', '--plots', action='store_true',
                       help='Generate plots (default: both summary and plots)')

    args = parser.parse_args()

    if not args.summary and not args.plots:
        args.summary = True
        args.plots = True

    # Load results
    results = load_results(args.input)

    # Print summary
    if args.summary:
        print_summary_table(results)

    # Generate plots
    if args.plots:
        if args.output_dir:
            Path(args.output_dir).mkdir(parents=True, exist_ok=True)

        plot_latency_histograms(results, args.output_dir)
        plot_percentile_comparison(results, args.output_dir)

    print("Done!")


if __name__ == '__main__':
    main()
