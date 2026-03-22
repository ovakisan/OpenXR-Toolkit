import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import numpy as np
import sys
import os

# --- Load CSV ---
csv_path = sys.argv[1] if len(sys.argv) > 1 else input("Path to _frames.csv: ").strip().strip('"')
df = pd.read_csv(csv_path)

# --- Derive time and frametimes ---
df["time_s"] = (df["timestamp_ns"] - df["timestamp_ns"].iloc[0]) / 1e9
df["frametime_ms"] = df["time_s"].diff() * 1000
df = df.dropna(subset=["frametime_ms"])
df = df[df["frametime_ms"] > 0]

df["appCPU_ms"]    = df["appCPU_us"]    / 1000
df["renderCPU_ms"] = df["renderCPU_us"] / 1000
df["appGPU_ms"]    = df["appGPU_us"]    / 1000
df["fps"]          = 1000 / df["frametime_ms"]

# --- Jitter (VR equivalent of animation error) ---
ideal_ms = 1000 / df["fps"].mean()
df["jitter_ms"] = df["frametime_ms"] - ideal_ms

# --- Percentile stats ---
def percentile_stats(series):
    return {
        "Avg":   series.mean(),
        "1%":    np.percentile(series, 1),
        "0.1%":  np.percentile(series, 0.1),
        "Min":   series.min(),
        "Max":   series.max(),
    }

fps_stats = percentile_stats(df["fps"])

# --- Build figure with 5 subplots ---
fig = make_subplots(
    rows=5, cols=1,
    subplot_titles=[
        "Frametime (ms)",
        "FPS over time",
        "CPU & GPU times (ms)",
        "Frame Delivery Jitter (ms) — VR Animation Error equivalent",
        "FPS Percentiles"
    ],
    vertical_spacing=0.08,
    row_heights=[0.20, 0.20, 0.20, 0.20, 0.20]
)

colors = ["#6366f1", "#22d3ee", "#f59e0b", "#10b981"]

# Row 1 — Frametime
fig.add_trace(go.Scatter(
    x=df["time_s"], y=df["frametime_ms"],
    mode="lines", name="Frametime",
    line=dict(color=colors[0], width=1),
    fill="tozeroy", fillcolor="rgba(99,102,241,0.15)"
), row=1, col=1)

# Row 2 — FPS
fig.add_trace(go.Scatter(
    x=df["time_s"], y=df["fps"],
    mode="lines", name="FPS",
    line=dict(color=colors[1], width=1),
    fill="tozeroy", fillcolor="rgba(34,211,238,0.15)"
), row=2, col=1)

# Row 3 — CPU/GPU times
fig.add_trace(go.Scatter(x=df["time_s"], y=df["appCPU_ms"],
    mode="lines", name="App CPU", line=dict(color=colors[2], width=1)), row=3, col=1)
fig.add_trace(go.Scatter(x=df["time_s"], y=df["renderCPU_ms"],
    mode="lines", name="Render CPU", line=dict(color=colors[3], width=1)), row=3, col=1)
fig.add_trace(go.Scatter(x=df["time_s"], y=df["appGPU_ms"],
    mode="lines", name="App GPU", line=dict(color="#f43f5e", width=1)), row=3, col=1)

# Row 4 — Jitter
fig.add_hline(y=0, line=dict(color="rgba(255,255,255,0.3)", width=1, dash="dash"), row=4, col=1)
fig.add_trace(go.Scatter(
    x=df["time_s"], y=df["jitter_ms"],
    mode="lines", name="Jitter",
    line=dict(color="#a78bfa", width=1),
    fill="tozeroy", fillcolor="rgba(167,139,250,0.15)"
), row=4, col=1)
fig.add_hrect(y0=-1, y1=1, fillcolor="rgba(34,197,94,0.08)", line_width=0, row=4, col=1)

# Row 5 — FPS percentile bars
perc_labels = list(fps_stats.keys())
perc_values = list(fps_stats.values())
fig.add_trace(go.Bar(
    x=perc_labels, y=perc_values,
    name="FPS Percentiles",
    marker_color=[colors[1] if l == "Avg" else colors[0] for l in perc_labels],
    text=[f"{v:.1f}" for v in perc_values],
    textposition="outside"
), row=5, col=1)

# --- Layout ---
fig.update_layout(
    title=dict(
        text=f"Frame Analysis — {os.path.basename(csv_path)}<br>"
             f"<span style='font-size:14px;font-weight:normal'>"
             f"Avg FPS: {fps_stats['Avg']:.1f} | 1% Low: {fps_stats['1%']:.1f} | "
             f"0.1% Low: {fps_stats['0.1%']:.1f} | "
             f"Jitter RMS: {np.sqrt((df['jitter_ms']**2).mean()):.2f}ms"
             f"</span>",
        x=0.5
    ),
    height=1600, showlegend=True,
    legend=dict(orientation="h", yanchor="bottom", y=1.01, xanchor="center", x=0.5),
    paper_bgcolor="#0f172a", plot_bgcolor="#1e293b",
    font=dict(color="#e2e8f0"),
    margin=dict(l=60, r=40, t=130, b=60)
)

for i in range(1, 6):
    fig.update_xaxes(gridcolor="#334155", row=i, col=1)
    fig.update_yaxes(gridcolor="#334155", row=i, col=1)

fig.update_xaxes(title_text="Time (s)", row=4, col=1)
fig.update_yaxes(title_text="ms",  row=1, col=1)
fig.update_yaxes(title_text="FPS", row=2, col=1)
fig.update_yaxes(title_text="ms",  row=3, col=1)
fig.update_yaxes(title_text="ms",  row=4, col=1)
fig.update_yaxes(title_text="FPS", row=5, col=1)

# --- Save ---
out_html = csv_path.replace(".csv", "_analysis.html")
fig.write_html(out_html)
print(f"Saved: {out_html}")

out_png = csv_path.replace(".csv", "_analysis.png")
fig.write_image(out_png, width=1400, height=1600)
print(f"Saved: {out_png}")
