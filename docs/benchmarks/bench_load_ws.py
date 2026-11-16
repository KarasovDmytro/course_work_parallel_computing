"""E2b (WebSocket): throughput and latency of /ws/search vs number of clients.

Every client keeps one connection open and sends {"query": ...} again and again, each time
waiting for "done" (or "error") before sending the next query, as the protocol requires.
WebSocket search always asks the LLM, so this shows the ceiling set by Ollama, which handles
one request at a time. For every query two times are recorded:
    expansion_ms  from sending the query to the query_expansion message (LLM queue + call)
    total_ms      from sending the query to done

    python bench_load_ws.py --users 1,2,4,8,16 --duration 60
"""

import argparse
import asyncio
import csv
import json
import random
import statistics
import time
from pathlib import Path

import websockets

RESULTS_DIR = Path(__file__).resolve().parent / "results"

QUERIES = [
    "whales hunting", "storage battery", "running horses", "sailing ships", "ancient kingdoms",
    "beautiful flowers", "bloody battles", "loving mothers", "old castle ghost", "steam engine",
    "gold mine", "winter snow", "king and queen", "french revolution", "electric light",
    "pirate treasure", "railway station", "desert island", "village church", "doctor medicine",
]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default="ws://localhost:8080/ws/search", help="WebSocket address")
    parser.add_argument("--users", default="1,2,4,8,16", help="comma-separated client counts")
    parser.add_argument("--duration", default=60, type=int, help="seconds per client count")
    parser.add_argument("--results", default=RESULTS_DIR, type=Path, help="folder for the CSV file")
    return parser.parse_args()


async def client(url, stop_at, samples):
    async with websockets.connect(url, max_size=None) as connection:
        while time.monotonic() < stop_at:
            sent = time.monotonic()
            await connection.send(json.dumps({"query": random.choice(QUERIES)}))
            sample = {"expansion_ms": None, "total_ms": None, "error": None, "expanded": False}
            while True:
                message = json.loads(await connection.recv())
                elapsed = (time.monotonic() - sent) * 1000
                if message["stage"] == "query_expansion":
                    sample["expansion_ms"] = elapsed
                    sample["expanded"] = len(message["data"]["expanded_terms"]) > 0
                elif message["stage"] == "done":
                    sample["total_ms"] = elapsed
                    break
                elif message["stage"] == "error":
                    sample["error"] = message["data"]["message"]
                    break
            samples.append(sample)


def percentile(values, percent):
    if not values:
        return 0.0
    values = sorted(values)
    return values[min(len(values) - 1, int(len(values) * percent / 100))]


async def run_step(url, users, duration):
    samples = []
    started = time.monotonic()
    stop_at = started + duration
    await asyncio.gather(*[client(url, stop_at, samples) for _ in range(users)])
    # The last queries may finish after stop_at, so the real time is used for throughput.
    elapsed = time.monotonic() - started

    done = [sample for sample in samples if sample["total_ms"] is not None]
    totals = [sample["total_ms"] for sample in done]
    expansions = [sample["expansion_ms"] for sample in done]
    return {
        "users": users,
        "duration_s": round(elapsed, 1),
        "queries": len(done),
        "errors": len(samples) - len(done),
        "no_expansion": sum(1 for sample in done if not sample["expanded"]),
        "qps": len(done) / elapsed,
        "p50_ms": percentile(totals, 50),
        "p95_ms": percentile(totals, 95),
        "expansion_p50_ms": percentile(expansions, 50),
        "expansion_p95_ms": percentile(expansions, 95),
        "search_avg_ms": statistics.mean(t - e for t, e in zip(totals, expansions)) if done else 0.0,
    }


async def main():
    args = parse_args()
    user_counts = [int(value) for value in args.users.split(",")]

    # One query first: the first call to Ollama loads the model into memory.
    await run_step(args.url, 1, 1)

    args.results.mkdir(parents=True, exist_ok=True)
    out_path = args.results / "e2_ws.csv"
    with open(out_path, "w", newline="") as file:
        writer = None
        for users in user_counts:
            row = await run_step(args.url, users, args.duration)
            if writer is None:
                writer = csv.DictWriter(file, fieldnames=list(row.keys()))
                writer.writeheader()
            writer.writerow(row)
            file.flush()
            print(f"ws users {users:3}: {row['qps']:6.2f} queries/s, p50 {row['p50_ms']:.0f} ms, "
                  f"p95 {row['p95_ms']:.0f} ms, LLM p50 {row['expansion_p50_ms']:.0f} ms, "
                  f"errors {row['errors']}, no expansion {row['no_expansion']}")
    print(f"saved to {out_path}")


if __name__ == "__main__":
    asyncio.run(main())
