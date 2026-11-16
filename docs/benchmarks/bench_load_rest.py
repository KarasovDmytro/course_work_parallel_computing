"""E2a / E2b (REST): throughput and latency of POST /api/search vs number of clients.

For every client count runs Locust headless for a fixed time and takes the aggregated
statistics: requests per second, p50/p95/p99 latency, errors and (in expanded mode) answers
without expanded terms. The server must run with a built index or an index is built first.

    python bench_load_rest.py --mode keyword                 # E2a, no LLM
    python bench_load_rest.py --mode expanded --users 1,2,4,8,16 --duration 60   # E2b

The load generator runs on the same machine as the server and takes CPU from it. Its CPU use
is printed for every step; --client-cpus pins Locust to chosen cores (taskset) to keep it apart
from the server.
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
RESULTS_DIR = HERE / "results"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default="http://localhost:8080", help="server address")
    parser.add_argument("--mode", default="keyword", choices=["keyword", "expanded"], help="search mode")
    parser.add_argument("--users", default="1,2,4,8,16,32,64", help="comma-separated client counts")
    parser.add_argument("--duration", default=30, type=int, help="seconds per client count")
    parser.add_argument("--processes", default=4, type=int, help="Locust worker processes (at most one per client)")
    parser.add_argument("--client-cpus", default="", help="pin Locust to these cores, e.g. 6,7")
    parser.add_argument("--results", default=RESULTS_DIR, type=Path, help="folder for the CSV file")
    return parser.parse_args()


def call(url, method, body=None):
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request) as response:
        return json.loads(response.read())


def prepare_server(base_url, mode):
    if call(base_url + "/api/stats", "GET")["documents"] == 0:
        result = call(base_url + "/api/index", "POST", {})
        print(f"index built: {result['documents_indexed']} documents in {result['time_ms']} ms")
    # A few requests first, so the first step does not pay for cold caches (and for loading the model).
    for query in ["whales hunting", "storage battery", "sailing ships"]:
        call(base_url + "/api/search", "POST", {"query": query, "mode": mode})


def run_locust(args, users, prefix):
    command = ["locust", "-f", str(HERE / "locustfile.py"), "--headless", "--only-summary",
               "-u", str(users), "-r", str(users), "-t", f"{args.duration}s",
               "-H", args.url, "--csv", str(prefix)]
    processes = min(args.processes, users)
    if processes > 1:
        command += ["--processes", str(processes)]
    if args.client_cpus:
        command = ["taskset", "-c", args.client_cpus] + command

    environment = dict(os.environ, SEARCH_MODE=args.mode)
    locust_bin = Path(sys.executable).parent
    environment["PATH"] = f"{locust_bin}{os.pathsep}{environment['PATH']}"

    before = os.times()
    subprocess.run(command, env=environment, check=False, capture_output=True, text=True)
    after = os.times()
    client_cpu = (after.children_user - before.children_user) + (after.children_system - before.children_system)
    return client_cpu / args.duration


def read_summary(prefix):
    with open(f"{prefix}_stats.csv") as file:
        aggregated = [row for row in csv.DictReader(file) if row["Name"] == "Aggregated"][0]
    fallback = 0
    with open(f"{prefix}_failures.csv") as file:
        for row in csv.DictReader(file):
            if "no expanded terms" in row["Error"]:
                fallback += int(row["Occurrences"])
    requests = int(aggregated["Request Count"])
    return {
        "requests": requests,
        "errors": int(aggregated["Failure Count"]) - fallback,
        "no_expansion": fallback,
        "rps": float(aggregated["Requests/s"]),
        "avg_ms": float(aggregated["Average Response Time"]),
        "p50_ms": float(aggregated["50%"]),
        "p95_ms": float(aggregated["95%"]),
        "p99_ms": float(aggregated["99%"]),
    }


def main():
    args = parse_args()
    user_counts = [int(value) for value in args.users.split(",")]
    prepare_server(args.url, args.mode)

    args.results.mkdir(parents=True, exist_ok=True)
    out_path = args.results / f"e2_rest_{args.mode}.csv"
    fields = ["mode", "users", "duration_s", "requests", "errors", "no_expansion", "rps",
              "avg_ms", "p50_ms", "p95_ms", "p99_ms", "client_cpu_cores"]

    with open(out_path, "w", newline="") as file, tempfile.TemporaryDirectory() as temp:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        for users in user_counts:
            prefix = Path(temp) / f"u{users}"
            client_cpu = run_locust(args, users, prefix)
            row = {"mode": args.mode, "users": users, "duration_s": args.duration,
                   **read_summary(prefix), "client_cpu_cores": round(client_cpu, 2)}
            writer.writerow(row)
            file.flush()
            print(f"{args.mode} users {users:3}: {row['rps']:8.1f} req/s, p50 {row['p50_ms']:.0f} ms, "
                  f"p95 {row['p95_ms']:.0f} ms, errors {row['errors']}, no expansion {row['no_expansion']}, "
                  f"client CPU {row['client_cpu_cores']} cores")
            time.sleep(2)
    print(f"saved to {out_path}")


if __name__ == "__main__":
    main()
