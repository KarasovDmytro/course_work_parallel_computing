"""E3, E4, E6, E7, E8: index build variants measured with index_bench.

index_bench is built into the search-server image, so every configuration runs as

    docker run --rm -v <dataset>:/bench:ro search-app-search-server ./build/index_bench --data /bench ...

and prints CSV rows that are collected into results/<experiment>.csv. Build the image first
(docker compose build) and prepare the dataset (prepare_dataset.py).

Experiments (dataset, what changes):
    e3  25 %   shards 1, 4, 16, 64 at 1 and 8 threads
    e4  10/25/50/100 %   corpus size at 1 and 8 threads
    e6  10 %   local table vs every token under the shard lock, 64 and 1 shards, threads 1..8
    e7  25 %   largest files first vs path order, threads 2..16
    e8  25 %   with and without stemming, 8 threads, matches on test queries
E6 builds an index with one posting per token, so it uses the smallest subset to fit in memory.
"""

import argparse
import csv
import io
import subprocess
import time
from pathlib import Path

RESULTS_DIR = Path(__file__).resolve().parent / "results"

TEST_QUERIES = [
    "whales hunting",
    "storage battery",
    "running horses",
    "sailing ships",
    "ancient kingdoms",
    "beautiful flowers",
    "bloody battles",
    "loving mothers",
]


def experiment_runs(data):
    """Returns {experiment: [(dataset label, dataset folder, index_bench options), ...]}."""
    subsets = {
        "p10": data / "subsets" / "p10",
        "p25": data / "subsets" / "p25",
        "p50": data / "subsets" / "p50",
        "p100": data / "books",
    }
    runs = {"e3": [], "e4": [], "e6": [], "e7": [], "e8": []}

    for threads in [1, 8]:
        for shards in [1, 4, 16, 64]:
            runs["e3"].append(("p25", subsets["p25"], ["--threads", threads, "--shards", shards]))

    for threads in [1, 8]:
        for label in ["p10", "p25", "p50", "p100"]:
            runs["e4"].append((label, subsets[label], ["--threads", threads]))

    for shards in [64, 1]:
        for threads in [1, 2, 4, 8]:
            runs["e6"].append(("p10", subsets["p10"], ["--threads", threads, "--shards", shards]))
            runs["e6"].append(("p10", subsets["p10"], ["--threads", threads, "--shards", shards, "--per-token-lock"]))

    for threads in [2, 4, 8, 16]:
        runs["e7"].append(("p25", subsets["p25"], ["--threads", threads]))
        runs["e7"].append(("p25", subsets["p25"], ["--threads", threads, "--as-is-order"]))

    queries = []
    for query in TEST_QUERIES:
        queries += ["--query", query]
    runs["e8"].append(("p25", subsets["p25"], ["--threads", 8] + queries))
    runs["e8"].append(("p25", subsets["p25"], ["--threads", 8, "--no-stemming"] + queries))

    return runs


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("experiments", nargs="*", default=["e3", "e4", "e6", "e7", "e8"],
                        help="which experiments to run (default: all)")
    parser.add_argument("--data", default=Path.home() / "bench-data", type=Path,
                        help="folder made by prepare_dataset.py")
    parser.add_argument("--image", default="search-app-search-server", help="docker image with index_bench")
    parser.add_argument("--repeat", default=5, type=int, help="measured builds per configuration")
    parser.add_argument("--warmup", default=1, type=int, help="builds before measuring, not saved")
    parser.add_argument("--results", default=RESULTS_DIR, type=Path, help="folder for the CSV files")
    return parser.parse_args()


def run_index_bench(image, dataset, options, repeat, warmup):
    command = ["docker", "run", "--rm", "-v", f"{dataset}:/bench:ro", image,
               "./build/index_bench", "--data", "/bench",
               "--repeat", str(repeat), "--warmup", str(warmup)]
    command += [str(option) for option in options]
    output = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    return list(csv.DictReader(io.StringIO(output)))


def main():
    args = parse_args()
    runs = experiment_runs(args.data)
    args.results.mkdir(parents=True, exist_ok=True)

    for experiment in args.experiments:
        out_path = args.results / f"{experiment}.csv"
        writer = None
        started = time.time()
        with open(out_path, "w", newline="") as file:
            for label, dataset, options in runs[experiment]:
                rows = run_index_bench(args.image, dataset, options, args.repeat, args.warmup)
                for row in rows:
                    row = {"experiment": experiment, "dataset": label, **row}
                    if writer is None:
                        writer = csv.DictWriter(file, fieldnames=list(row.keys()))
                        writer.writeheader()
                    writer.writerow(row)
                file.flush()
                times = sorted(int(row["time_ms"]) for row in rows)
                shown = [str(option) for option in options if option not in TEST_QUERIES and option != "--query"]
                print(f"{experiment} {label} {' '.join(shown)}: median {times[len(times) // 2]} ms")
        print(f"{experiment} done in {time.time() - started:.0f} s, saved to {out_path}\n")


if __name__ == "__main__":
    main()
