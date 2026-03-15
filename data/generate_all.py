import json
import subprocess
import argparse
import sys

def main():
    parser = argparse.ArgumentParser(description="Generate all test data from JSON config")
    parser.add_argument("json_file", help="Path to JSON configuration file")
    args = parser.parse_args()

    with open(args.json_file, "r") as f:
        params_list = json.load(f)

    for params in params_list:
        name = params["name"]
        seed = params["seed"]
        num_chunks = params["num_chunks"]
        chunk_size = params["chunk_size"]
        num_queries = params["num_queries"]
        skew = 2

        cmd = [
            sys.executable, "generate.py",
            "-f", f"{name}",
            "-n", str(num_chunks),
            "-m", str(chunk_size),
            "-q", str(num_queries),
            "-s", str(skew),
            "-i", str(seed)
        ]
        print(f"Generating '{name}' (f_a={params['f_a']}, f_s={params['f_s']})...")
        print(f"  Running: {' '.join(cmd)}")
        subprocess.run(cmd, check=True)
        print(f"  Done. Generated {name}.data and {name}.query\n")

if __name__ == "__main__":
    main()
