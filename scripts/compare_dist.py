#!/usr/bin/env python3

import sys
import argparse
import math


def load_dist(path):
    dist = {}
    with open(path) as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) != 2:
                sys.exit(f"error: {path}:{lineno}: expected 'vertex dist', got: {line!r}")
            v, d = parts
            dist[int(v)] = float(d)
    return dist


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file_a")
    ap.add_argument("file_b")
    ap.add_argument("--tol", type=float, default=1e-3,
                     help="absolute tolerance for float comparison (default: 1e-3)")
    ap.add_argument("--quiet", action="store_true", help="only print PASS/FAIL, no per-vertex diffs")
    args = ap.parse_args()

    a = load_dist(args.file_a)
    b = load_dist(args.file_b)

    ok = True

    verts_a, verts_b = set(a), set(b)
    if verts_a != verts_b:
        ok = False
        only_a = verts_a - verts_b
        only_b = verts_b - verts_a
        print(f"MISMATCH: vertex sets differ ({len(a)} vs {len(b)} vertices)")
        if only_a:
            print(f"  only in {args.file_a}: {sorted(only_a)[:10]}{' ...' if len(only_a) > 10 else ''}")
        if only_b:
            print(f"  only in {args.file_b}: {sorted(only_b)[:10]}{' ...' if len(only_b) > 10 else ''}")

    mismatches = []
    for v in sorted(verts_a & verts_b):
        da, db = a[v], b[v]
        a_inf, b_inf = math.isinf(da), math.isinf(db)
        if a_inf or b_inf:
            if a_inf != b_inf:
                mismatches.append((v, da, db, "one is inf, other is not"))
            continue
        if abs(da - db) > args.tol:
            mismatches.append((v, da, db, f"diff={abs(da - db):.6f} > tol={args.tol}"))

    if mismatches:
        ok = False
        print(f"MISMATCH: {len(mismatches)} vertex distance(s) differ (tol={args.tol})")
        if not args.quiet:
            for v, da, db, reason in mismatches[:20]:
                print(f"  vertex {v}: {args.file_a}={da:.6f}  {args.file_b}={db:.6f}  ({reason})")
            if len(mismatches) > 20:
                print(f"  ... and {len(mismatches) - 20} more")

    if ok:
        print(f"PASS: {len(verts_a)} vertices match within tol={args.tol}")
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()