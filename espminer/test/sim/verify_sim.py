#!/usr/bin/env python3
"""Independent check of what the firmware submitted.

Reads sim_result.json (written by the simulation) and rebuilds every share
from scratch with Python's own hashlib: coinbase -> merkle root -> block
header -> double SHA-256, then compares against the target the pool set.

Nothing here shares code with the firmware, so agreement means the byte
order, the merkle folding and the nonce placement in miner.cpp are all
genuinely right - not merely self-consistent.
"""
import hashlib
import json
import sys


def sha256d(b):
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def main():
    with open("sim_result.json") as f:
        r = json.load(f)

    jobs = {j["id"]: j for j in r["jobs"]}
    e1 = r["extranonce1"]
    target = int((0xFFFF << 208) / r["difficulty"])

    subs = r["submissions"]
    if not subs:
        print("FAIL: the firmware submitted nothing")
        return 1

    print(f"verifying {len(subs)} submission(s) against target {target:064x}\n")

    bad = 0
    for i, s in enumerate(subs, 1):
        job = jobs.get(s["job_id"])
        if job is None:
            print(f"  {i}. FAIL: submitted for unknown job {s['job_id']!r}")
            bad += 1
            continue

        # 1. coinbase -> merkle root
        coinbase = bytes.fromhex(job["coinb1"] + e1 + s["extranonce2"] + job["coinb2"])
        root = sha256d(coinbase)
        for branch in job["merkle"]:
            root = sha256d(root + bytes.fromhex(branch))

        # 2. block header (stratum prevhash has each 4-byte word reversed)
        raw = bytes.fromhex(job["prevhash"])
        prev = b"".join(raw[w * 4:w * 4 + 4][::-1] for w in range(8))
        header = (
            bytes.fromhex(job["version"])[::-1]
            + prev
            + root
            + bytes.fromhex(s["ntime"])[::-1]
            + bytes.fromhex(job["nbits"])[::-1]
            + bytes.fromhex(s["nonce"])[::-1]
        )
        if len(header) != 80:
            print(f"  {i}. FAIL: header is {len(header)} bytes, not 80")
            bad += 1
            continue

        # 3. does it actually beat the target?
        h = sha256d(header)
        value = int.from_bytes(h[::-1], "big")
        ok = value <= target
        share_diff = (0xFFFF << 208) / value if value else float("inf")

        print(f"  {i}. job {s['job_id']}  nonce {s['nonce']}  extranonce2 {s['extranonce2']}")
        print(f"     hash {h[::-1].hex()}")
        print(f"     difficulty {share_diff:.4f}  -> {'VALID' if ok else 'DOES NOT MEET TARGET'}")

        if s["ntime"] != job["ntime"]:
            print(f"     note: ntime rolled from {job['ntime']} to {s['ntime']}")
        if not ok:
            bad += 1

    jobs_used = {s["job_id"] for s in subs}
    print()
    if len(jobs_used) < 2:
        print(f"note: only job(s) {sorted(jobs_used)} were mined")

    if bad:
        print(f"FAILED: {bad} of {len(subs)} submissions are not valid shares")
        return 1
    print(f"PASSED: all {len(subs)} submissions are cryptographically valid shares")
    return 0


if __name__ == "__main__":
    sys.exit(main())
