import json
import numpy as np
from matplotlib import pyplot as plt


def plot_timing_logs(log_file):
    data = []
    with open(log_file, "r") as f:
        for line in f:
            data.append(json.loads(line))
    
    jitters = np.array([float(d["jitter_ns"]) / 1e6 for d in data])

    print("Jitter (ms) stats:")
    print(f"mean: {np.mean(jitters):.2f}")
    print(f"std: {np.std(jitters):.2f}")
    print(f"min: {np.min(jitters):.2f}")
    print(f"max: {np.max(jitters):.2f}")
    print(f"count: {len(jitters)}")

    plt.hist(jitters, bins=20)
    plt.xlabel("Jitter (ms)")
    plt.ylabel("Count")
    plt.title("Jitter Distribution")
    plt.savefig("jitter_hist.png")
    plt.show()

if __name__ == "__main__":
    plot_timing_logs("test.jsonl")
