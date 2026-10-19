"""Summarise a Tracy trace as per-zone timing tables (count, mean, p50, p95,
p99, max), CPU zones per thread and GPU zones, optionally against a baseline
trace. Reads the trace through Tracy's own tracy-csvexport (the version must
match the client the trace was recorded with: 0.14.1, ports/tracy).

    python -I scripts/tracy_summary.py <trace.tracy> [--baseline <old.tracy>]
                                       [--tools <dir with tracy-csvexport.exe>]

The tools directory defaults to $TRACY_DIR, then PATH. Times are printed in
milliseconds. Used by scripts/run_game.ps1 -Tracy.
"""
import argparse
import csv
import io
import math
import os
import shutil
import subprocess
import sys

EXPECTED_VERSION = "0.14.1"


def find_csvexport(tools):
    names = ["tracy-csvexport.exe", "tracy-csvexport"]
    for d in [tools, os.environ.get("TRACY_DIR")]:
        if d:
            for n in names:
                p = os.path.join(d, n)
                if os.path.isfile(p):
                    return p
    for n in names:
        p = shutil.which(n)
        if p:
            return p
    sys.exit("tracy-csvexport not found: pass --tools or set TRACY_DIR")


def check_version(exe):
    out = subprocess.run([exe, "--version"], capture_output=True, text=True)
    text = (out.stdout + out.stderr).strip()
    if EXPECTED_VERSION not in text:
        sys.exit(f"{exe} reports '{text}', expected Tracy {EXPECTED_VERSION} (ports/tracy)")


def export(exe, trace, flag):
    out = subprocess.run([exe, flag, trace], capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"tracy-csvexport {flag} {trace} failed: {out.stderr.strip()}")
    return list(csv.DictReader(io.StringIO(out.stdout)))


def percentile(sorted_values, p):
    if not sorted_values:
        return math.nan
    k = (len(sorted_values) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (k - lo)


def stats(samples_ns):
    v = sorted(samples_ns)
    ms = 1e-6
    return {
        "count": len(v),
        "mean": sum(v) / len(v) * ms,
        "p50": percentile(v, 50) * ms,
        "p95": percentile(v, 95) * ms,
        "p99": percentile(v, 99) * ms,
        "max": v[-1] * ms,
    }


def summarise(exe, trace):
    """{("cpu", thread, zone) | ("gpu", "", zone): stats}"""
    groups = {}
    for row in export(exe, trace, "-u"):
        key = ("cpu", row["thread"], row["name"])
        groups.setdefault(key, []).append(float(row["exec_time_ns"]))
    for row in export(exe, trace, "-g"):
        key = ("gpu", "", row["name"])
        groups.setdefault(key, []).append(float(row["GPU execution time"]))
    return {k: stats(v) for k, v in groups.items() if v}


def print_table(current, baseline):
    cols = ["count", "mean", "p50", "p95", "p99", "max"]
    head = "| kind | thread | zone | " + " | ".join(cols)
    if baseline is not None:
        head += " | d mean | d p95"
    print(head + " |")
    print("|" + "---|" * (3 + len(cols) + (2 if baseline is not None else 0)))
    for key in sorted(current, key=lambda k: (k[0], k[1], -current[k]["mean"] * current[k]["count"])):
        s = current[key]
        cells = [key[0], key[1], key[2], str(s["count"])] + [f"{s[c]:.3f}" for c in cols[1:]]
        if baseline is not None:
            b = baseline.get(key)
            cells += ["new", "new"] if b is None else [f"{s['mean'] - b['mean']:+.3f}", f"{s['p95'] - b['p95']:+.3f}"]
        print("| " + " | ".join(cells) + " |")
    if baseline is not None:
        for key in sorted(set(baseline) - set(current)):
            print(f"| {key[0]} | {key[1]} | {key[2]} | gone |" + " |" * (len(cols) + 1))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("trace")
    ap.add_argument("--baseline")
    ap.add_argument("--tools")
    a = ap.parse_args()
    exe = find_csvexport(a.tools)
    check_version(exe)
    current = summarise(exe, a.trace)
    if not current:
        sys.exit(f"{a.trace} holds no zones")
    baseline = summarise(exe, a.baseline) if a.baseline else None
    print(f"Times in ms. Trace: {a.trace}" + (f", baseline: {a.baseline}" if a.baseline else ""))
    print()
    print_table(current, baseline)


if __name__ == "__main__":
    main()
