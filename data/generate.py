from argparse import ArgumentParser
import numpy as np
import struct


def hash_u32(x: np.ndarray, seed: int) -> np.ndarray:
    """Simple, fast hash for 32-bit integers using Murmur-inspired mixing."""
    x = np.asarray(x, dtype=np.uint32)
    x ^= np.uint32(seed)
    x ^= x >> 16
    x *= 0x85ebca6b
    x ^= x >> 13
    x *= 0xc2b2ae35
    x ^= x >> 16
    return x

def uniform_distribution(determined_values):
    p = rng.uniform(low=0, high=0.25)
    minimum = (1-p) * min(determined_values)
    maximum = np.clip((1+p) * max(determined_values), 0, np.iinfo(np.uint32).max)
    block_values = rng.uniform(low=minimum, high=maximum, size=args.chunk_size).astype(np.uint32)
    return block_values

def zipf_distribution():
    skew = max(1.01, rng.normal(loc=args.base_skew, scale=0.5))

    # Generate Zipf-distributed data
    z = rng.zipf(skew, size=args.chunk_size).astype(np.uint32)
    return z

def zipf_distribution_offset(determined_values):
    return zipf_distribution() + np.random.choice(determined_values, size=1)

def zipf_distribution_hashed(seed):
    return hash_u32(zipf_distribution(), seed)

def ensure_determined_values(arr, determined_values):
    chosen = np.random.choice(np.unique(arr), size=len(determined_values), replace=True)
    for src, dest in zip(chosen, determined_values):
        arr[arr == src] = dest
    return arr

# Argument parsing
parser = ArgumentParser()
parser.add_argument("-f", "--file", dest="filename", required=True,
                    help="Output data file")
parser.add_argument("-q", "--queries", dest="num_queries", type=int, required=True,
                    help="Number of queries")
parser.add_argument("-n", "--chunks", dest="num_chunks", type=int, required=True,
                    help="Number of data chunks")
parser.add_argument("-m", "--chunkSize", dest="chunk_size", type=int, required=True,
                    help="Number of rows per chunk")
parser.add_argument("-s", "--skew", dest="base_skew", type=float, required=True,
                    help="Base skew (Zipf distribution)")
parser.add_argument("-i", "--seed", dest="seed", type=int, required=True,
                    help="Seed for the random number generator")

args = parser.parse_args()
rng = np.random.default_rng(args.seed)

query_values = rng.integers(0, 1 << 32, size=args.num_queries, dtype=np.uint32)

pruning_likelihoods = rng.uniform(low=0, high=1, size=(int(args.num_queries/2)))
pruning_likelihoods = np.clip(pruning_likelihoods, 0.0, 1.0)

zeros = np.zeros(int(args.num_queries/4))
ones = np.ones(int(args.num_queries/4))

pruning_likelihoods = np.concatenate([zeros, pruning_likelihoods, ones])

block_likelihoods = rng.uniform(low=0, high=1, size=args.num_chunks)
determined_values = [[] for _ in range(0, args.num_chunks)]

for i in range(0, args.num_queries):
    distr = rng.uniform(low=0, high=1, size=args.num_chunks)
    for j in range(0, args.num_chunks):
        if distr[j] >= block_likelihoods[j]:
            determined_values[j].append(query_values[i])

with open(args.filename + ".query", "wb") as f:
    f.write(struct.pack("Q", args.num_queries))
    query_values.tofile(f)

with open(args.filename + ".data", "wb") as f:
    f.write(struct.pack("Q", args.num_chunks))
    f.write(struct.pack("Q", args.chunk_size))

    for i in range(args.num_chunks):
        if len(determined_values[i]) > 0 and len(determined_values[i]) < 20:
            block_values = uniform_distribution(determined_values[i])
        elif len(determined_values[i]) > 0 and len(determined_values[i]) < 256:
            block_values = zipf_distribution_offset(determined_values[i])
        else:
            block_values = zipf_distribution_hashed(args.seed)
        block_values = ensure_determined_values(block_values, determined_values[i])
        block_values.tofile(f)
