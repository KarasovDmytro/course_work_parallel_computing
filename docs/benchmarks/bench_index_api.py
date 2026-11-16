"""E1: index build time vs number of threads, measured through the server API.

Calls POST /api/index {"threads": N} and takes time_ms from the response, the same way the
automatic check does. The server must already run with the benchmark dataset:

    DATASET_DIR=~/bench-data/books docker compose up -d

Runs go round-robin (1, 2, 4, ... then again 1, 2, 4, ...), so a slow drift of the machine
(heating, background programs) spreads over all thread counts instead of hitting one of them.
"""

import argparse
import csv
import json
import statistics
import urllib.request
from pathlib import Path

RESULTS_DIR = Path(__file__).resolve().parent / "results"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default="http://localhost:8080", help="server address")
    parser.add_argument("--threads", default="1,2,4,6,8,12,16", help="comma-separated thread counts")
    parser.add_argument("--repeat", default=5, type=int, help="measured builds per thread count")
    parser.add_argument("--warmup", default=1, type=int, help="builds before measuring, not saved")
    parser.add_argument("--out", default=RESULTS_DIR / "e1_threads.csv", type=Path, help="CSV file")
    return parser.parse_args()


def call(url, method, body=None):
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request) as response:
        return json.loads(response.read())


def build_index(base_url, threads):
    return call(base_url + "/api/index", "POST", {"threads": threads})


def main():
    args = parse_args()
    thread_counts = [int(value) for value in args.threads.split(",")]

    health = call(args.url + "/api/health", "GET")
    print(f"server: {health}")

    for i in range(args.warmup):
        result = build_index(args.url, max(thread_counts))
        print(f"warm-up {i + 1}: {result['documents_indexed']} documents, {result['time_ms']} ms")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    times = {threads: [] for threads in thread_counts}
    with open(args.out, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["run", "threads", "documents", "time_ms"])
        for run in range(1, args.repeat + 1):
            for threads in thread_counts:
                result = build_index(args.url, threads)
                writer.writerow([run, threads, result["documents_indexed"], result["time_ms"]])
                file.flush()
                times[threads].append(result["time_ms"])
                print(f"run {run}, threads {threads}: {result['time_ms']} ms")

    # Speedup is measured against the first thread count in the list (normally 1 thread).
    base_threads = thread_counts[0]
    base = statistics.median(times[base_threads])
    print(f"\nthreads  median_ms  speedup  efficiency   (relative to {base_threads} thread(s))")
    for threads in thread_counts:
        median = statistics.median(times[threads])
        speedup = base / median
        efficiency = speedup * base_threads / threads
        print(f"{threads:7}  {median:9.0f}  {speedup:7.2f}  {efficiency:10.2f}")
    print(f"\nsaved to {args.out}")


if __name__ == "__main__":
    main()
