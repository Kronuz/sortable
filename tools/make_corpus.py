"""Writes fuzz seeds into DIR: the reject witnesses and the encodings of a sample of the vectors.

Usage: python3 -I tools/make_corpus.py DIR
"""

import os
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "test")
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
seeds = set()
for line in open(os.path.join(root, "reject.txt")):
    hexed = line.split()[-1]
    seeds.add(b"" if hexed == "-" else bytes.fromhex(hexed))
for n, line in enumerate(open(os.path.join(root, "vectors.txt"))):
    if n % 97 == 0:
        seeds.add(bytes.fromhex(line.split()[-1]))
for n, s in enumerate(sorted(seeds)):
    with open(os.path.join(out, f"seed{n:04d}"), "wb") as f:
        f.write(bytes([n % 7]) + s)  # a leading split byte for the pair check
print(len(seeds), "seeds")
