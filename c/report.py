import sys, os, re, statistics

RUNS = sys.argv[1] if len(sys.argv) > 1 else "10"
FILES = ["15kb-word.txt", "30kb-word.txt"]
CONFIG_MAP = {
    "no-opt-debug": "No-Opt (Debug)",
    "no-opt-release": "No-Opt (Release)",
    "full-opt-debug": "Full-Opt (Debug)",
    "full-opt-release": "Full-Opt (Release)"
}

def get_stats(filepath):
    if not os.path.exists(filepath): return None
    with open(filepath, 'r') as f:
        content = f.read()
    ins = [float(x) for x in re.findall(r"Insert: ([\d.]+) ms", content)]
    get = [float(x) for x in re.findall(r"Get:\s+([\d.]+) ms", content)]
    if not ins: return None

    def calc(data):
        try:
            mode_val = statistics.mode([round(x, 2) for x in data])
        except statistics.StatisticsError:
            mode_val = data[0]
        return {
            "mean": statistics.mean(data),
            "std": statistics.stdev(data) if len(data) > 1 else 0,
            "mode": mode_val
        }

    return {"ins": calc(ins), "get": calc(get)}

with open("plot_data.dat", "w") as f:
    for fname in FILES:
        f.write(f'# {fname}\n')
        for cfg_id, display_name in CONFIG_MAP.items():
            s = get_stats(f"log/{cfg_id}-{fname}.log")
            if s:
                f.write(f'"{display_name}" {s["ins"]["mean"]} {s["ins"]["std"]} {s["get"]["mean"]} {s["get"]["std"]}\n')
        f.write("\n\n")

with open("plot.gp", "w") as f:
    f.write(f"""
set terminal pngcairo size 1200,600 background "#121212"
set output "summary_plot.png"
set style data histograms
set style histogram errorbars gap 2 lw 2.5
set style fill solid 1.0 border -1
set boxwidth 0.9

set title "Trie Performance Matrix (N={RUNS})" tc rgb "#ffffff" font ",18"
set ylabel "Latency (ms)" tc rgb "#ffffff" font ",12"
set border lc rgb "#666666"
set xtics tc rgb "#ffffff" rotate by -15 font ",10"
set ytics tc rgb "#ffffff"
set grid ytics lc rgb "#333333"
set key tc rgb "#ffffff" top left box lt -1 lw 1

# High Saturation Neon Palette
# 15kb: Neon Cyan & Royal Blue
# 30kb: Neon Red & Vivid Gold
plot "plot_data.dat" i 0 using 2:3:xtic(1) title "15kb: Insert" lt rgb "#00FFFF", \\
     "" i 0 using 4:5 title "15kb: Search" lt rgb "#3D5AFE", \\
     "" i 1 using 2:3:xtic(1) title "30kb: Insert" lt rgb "#FF1744", \\
     "" i 1 using 4:5 title "30kb: Search" lt rgb "#FFEA00"
""")

rows = ""
for fname in FILES:
    rows += f"<tr><th colspan='4' style='background:#222; color:#00FFFF;'>Dataset: {fname}</th></tr>"
    for cfg_id, display_name in CONFIG_MAP.items():
        s = get_stats(f"log/{cfg_id}-{fname}.log")
        if s:
            rows += f"""
            <tr style="border-top: 2px solid #444;">
                <td rowspan="2"><b>{display_name}</b></td>
                <td style="color:#00FFFF"><b>Insert</b></td>
                <td>{s['ins']['mean']:.3f} ms ± {s['ins']['std']:.3f}</td>
                <td>Mode: {s['ins']['mode']:.2f} ms</td>
            </tr>
            <tr>
                <td style="color:#3D5AFE"><b>Search</b></td>
                <td>{s['get']['mean']:.3f} ms ± {s['get']['std']:.3f}</td>
                <td>Mode: {s['get']['mode']:.2f} ms</td>
            </tr>"""

html_content = f"""
<html><head><style>
    body {{ font-family: 'Consolas', monospace; background: #0a0a0a; color: #f0f0f0; padding: 20px; }}
    .card {{ background: #141414; padding: 30px; border-radius: 8px; border: 1px solid #333; max-width: 1100px; margin: auto; }}
    table {{ width: 100%; border-collapse: collapse; margin-top: 20px; background: #1a1a1a; }}
    th, td {{ padding: 12px; border: 1px solid #333; text-align: left; }}
    img {{ width: 100%; border: 2px solid #333; margin-bottom: 20px; }}
    h1 {{ color: #ffffff; text-align: center; letter-spacing: 2px; }}
</style></head><body><div class="card">
    <h1>TRIE PERFORMANCE ANALYSIS</h1>
    <p style="text-align:center; color:#888;">N = {RUNS} Iterations | Stats: Mean, StdDev, Mode</p>
    <img src="summary_plot.png">
    <table>
        <tr><th>Configuration</th><th>Op</th><th>Mean ± StdDev</th><th>Mode (rounded)</th></tr>
        {rows}
    </table>
</div></body></html>
"""
with open("report.html", "w") as f: f.write(html_content)
