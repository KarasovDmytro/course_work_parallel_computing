"""Copies a random sample of PG-19 books to a fast disk and makes nested subsets for E4.

Result:
    <dest>/books/            all sampled books (the benchmark dataset)
    <dest>/subsets/p10/      10 % of them, hard links to the files in books/
    <dest>/subsets/p25/      25 %
    <dest>/subsets/p50/      50 %
The 100 % point of E4 is <dest>/books itself. Running the script again skips files
that are already copied.
"""

import argparse
import os
import random
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SUBSET_PERCENTS = [10, 25, 50]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", default=REPO_ROOT / "deepmind-gutenberg" / "train", type=Path,
                        help="folder with PG-19 .txt files")
    parser.add_argument("--dest", default=Path.home() / "bench-data", type=Path,
                        help="where to put the dataset (preferably ext4 on an SSD)")
    parser.add_argument("--count", default=11111, type=int, help="number of books")
    parser.add_argument("--seed", default=2026, type=int, help="random seed, the same seed gives the same books")
    return parser.parse_args()


def main():
    args = parse_args()
    all_books = sorted(args.source.rglob("*.txt"))
    if len(all_books) < args.count:
        raise SystemExit(f"{args.source} has only {len(all_books)} books, {args.count} requested")

    # The shuffled order is kept: the first 10 % of it is subset p10, the first 25 % is p25 and so on,
    # so every smaller subset is a part of the bigger one.
    sample = random.Random(args.seed).sample(all_books, args.count)

    books_dir = args.dest / "books"
    books_dir.mkdir(parents=True, exist_ok=True)
    total_bytes = 0
    for i, source in enumerate(sample, start=1):
        target = books_dir / source.name
        if not target.exists() or target.stat().st_size != source.stat().st_size:
            shutil.copyfile(source, target)
        total_bytes += target.stat().st_size
        if i % 500 == 0 or i == len(sample):
            print(f"copied {i}/{len(sample)} books, {total_bytes / 2**30:.2f} GiB")

    for percent in SUBSET_PERCENTS:
        subset_dir = args.dest / "subsets" / f"p{percent}"
        subset_dir.mkdir(parents=True, exist_ok=True)
        size = len(sample) * percent // 100
        for source in sample[:size]:
            link = subset_dir / source.name
            if not link.exists():
                os.link(books_dir / source.name, link)
        print(f"subset p{percent}: {size} books")

    print(f"dataset: {books_dir} ({len(sample)} books, {total_bytes / 2**30:.2f} GiB)")


if __name__ == "__main__":
    main()
