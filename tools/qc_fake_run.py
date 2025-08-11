import os, json, time, pathlib
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime

BASE = pathlib.Path(__file__).resolve().parent.parent
REPORTS = BASE / "reports"
REPORTS.mkdir(exist_ok=True)

def generate_fake_jsonl(path, fps=30, seconds=60, drop_rate=0.01):
    rng = np.random.default_rng(0)
    N = fps*seconds
    t0 = time.time_ns()
    seq = 0
    brightness = 100.0
    with open(path, "w") as f:
        for i in range(N):
            if rng.random() < drop_rate:
                brightness += rng.normal(0, 0.1)
                continue
            ts_mono_ns = t0 + int((i / fps) * 1e9) + int(rng.normal(0, 2e6))
            ts_wall_ns = ts_mono_ns
            brightness += rng.normal(0, 0.1) + 0.001
            rec = {"seq": int(seq), "ts_mono_ns": int(ts_mono_ns),
                   "ts_wall_ns": int(ts_wall_ns), "device_id": "cam_front",
                   "brightness": float(brightness)}
            f.write(json.dumps(rec) + "\n")
            seq += 1

def parse(jsonl_path):
    seqs, ts, br = [], [], []
    with open(jsonl_path) as f:
        for line in f:
            j = json.loads(line)
            seqs.append(j["seq"]); ts.append(j["ts_mono_ns"]); br.append(j.get("brightness", 0))
    seqs = np.array(seqs); ts = np.array(ts); br = np.array(br)
    if len(ts) < 2: return None
    dts_ms = np.diff(ts) / 1e6
    drops = int(seqs[-1] - seqs[0] + 1 - len(seqs))
    return dts_ms, br, drops

def plot_hist(dts_ms, out_png):
    import matplotlib.pyplot as plt
    plt.figure()
    plt.hist(dts_ms, bins=50)
    plt.xlabel("Inter-frame interval (ms)"); plt.ylabel("Count")
    plt.title("Frame Interval Histogram"); plt.tight_layout(); plt.savefig(out_png); plt.close()

def plot_brightness(br, out_png):
    import matplotlib.pyplot as plt
    plt.figure()
    plt.plot(br)
    plt.xlabel("Frame index"); plt.ylabel("Brightness (a.u.)")
    plt.title("Brightness Trend"); plt.tight_layout(); plt.savefig(out_png); plt.close()

def write_html(report_path, interval_png, bright_png, drops, fps):
    html = f\"\"\"<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>QC Report</title></head>
<body>
<h2>Fake QC Report</h2>
<p>Generated: {datetime.now().isoformat(timespec='seconds')}</p>
<p>Drops (approx): {drops} — Target FPS: {fps}</p>
<h3>Frame Interval Histogram</h3>
<img src="{interval_png.name}" width="640">
<h3>Brightness Trend</h3>
<img src="{bright_png.name}" width="640">
</body></html>
\"\"\"
    with open(report_path, "w") as f:
        f.write(html)

def main():
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = REPORTS / f"fake_run_{stamp}"
    run_dir.mkdir(parents=True, exist_ok=True)
    jsonl = run_dir / "cam_front.jsonl"
    generate_fake_jsonl(jsonl)
    parsed = parse(jsonl)
    if not parsed: print("Not enough data"); return
    dts_ms, br, drops = parsed
    png1 = run_dir / "interval_hist.png"
    png2 = run_dir / "brightness.png"
    plot_hist(dts_ms, png1)
    plot_brightness(br, png2)
    html = run_dir / "qc_report.html"
    write_html(html, png1, png2, drops, 30)
    print(f"QC report: {html}")

if __name__ == "__main__":
    main()
