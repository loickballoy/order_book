"""Génère docs/bench-dark.svg et docs/bench-light.svg pour le README de order_book.

Usage : python3 bench_chart.py  (relancer après chaque nouvelle mesure, puis commit)
Les chiffres viennent du tableau « Benchmarks » du README : à mettre à jour ensemble.
"""
from pathlib import Path

TITLE = "Mean latency per operation vs. book depth"
SUBTITLE = "200,000 orders · single thread · g++ 15.2 -O2 · Intel i5-4670 · v0.2-price-levels"
GROUPS = ["100", "1,000", "10,000", "100,000"]          # distinct price levels (L)
SERIES = [                                            # (label, values in µs)
    ("insert + match, per order", [0.211, 0.278, 0.401, 0.567]),
    ("cancel by ID", [0.128, 0.179, 0.282, 0.601]),
]
Y_MAX, Y_STEP, UNIT = 0.7, 0.2, "µs"

THEMES = {
    "dark":  dict(text="#e6edf3", muted="#9198a1", grid="#3d444d", bars=["#4493f8", "#9198a1"]),
    "light": dict(text="#1f2328", muted="#59636e", grid="#d1d9e0", bars=["#0969da", "#818b98"]),
}
W, H = 760, 320
X0, X1, Y0, Y1 = 64, 736, 92, 262          # plot area (Y0 top, Y1 baseline)
FONT = "-apple-system,BlinkMacSystemFont,'Segoe UI','Noto Sans',Helvetica,Arial,sans-serif"


def y(v):
    return Y1 - (Y1 - Y0) * v / Y_MAX


def svg(theme):
    c = THEMES[theme]
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
           f'font-family="{FONT}" role="img" aria-label="{TITLE}">',
           f'<title>{TITLE}</title>',
           f'<text x="{X0 - 40}" y="30" font-size="17" font-weight="600" fill="{c["text"]}">{TITLE}</text>',
           f'<text x="{X0 - 40}" y="52" font-size="12" fill="{c["muted"]}">{SUBTITLE}</text>']
    # grid + y labels
    v = 0.0
    while v <= Y_MAX + 1e-9:
        out.append(f'<line x1="{X0}" x2="{X1}" y1="{y(v):.1f}" y2="{y(v):.1f}" stroke="{c["grid"]}" '
                   f'stroke-width="1"{"" if v == 0 else " stroke-dasharray=\"3 4\""}/>')
        out.append(f'<text x="{X0 - 8}" y="{y(v) + 4:.1f}" font-size="11" text-anchor="end" '
                   f'fill="{c["muted"]}">{v:.1f}</text>')
        v += Y_STEP
    out.append(f'<text x="{X0 - 8}" y="{Y0 - 14}" font-size="11" text-anchor="end" fill="{c["muted"]}">{UNIT}</text>')
    # bars
    n, k = len(GROUPS), len(SERIES)
    slot = (X1 - X0) / n
    bw, gap = 46, 6
    for i, g in enumerate(GROUPS):
        cx = X0 + slot * (i + 0.5)
        left = cx - (k * bw + (k - 1) * gap) / 2
        for j, (_, vals) in enumerate(SERIES):
            x = left + j * (bw + gap)
            val = vals[i]
            out.append(f'<rect x="{x:.1f}" y="{y(val):.1f}" width="{bw}" height="{Y1 - y(val):.1f}" '
                       f'rx="3" fill="{c["bars"][j]}"/>')
            out.append(f'<text x="{x + bw / 2:.1f}" y="{y(val) - 6:.1f}" font-size="11" text-anchor="middle" '
                       f'fill="{c["text"]}">{val:.2f}</text>')
        out.append(f'<text x="{cx:.1f}" y="{Y1 + 20}" font-size="12" text-anchor="middle" fill="{c["text"]}">{g}</text>')
    out.append(f'<text x="{(X0 + X1) / 2:.1f}" y="{Y1 + 42}" font-size="12" text-anchor="middle" '
               f'fill="{c["muted"]}">distinct price levels in the book</text>')
    # legend (top right, under subtitle)
    lx = X1
    for j, (label, _) in reversed(list(enumerate(SERIES))):
        tw = 6.6 * len(label)
        lx -= tw
        out.append(f'<text x="{lx:.1f}" y="{Y0 - 14}" font-size="12" fill="{c["text"]}">{label}</text>')
        out.append(f'<rect x="{lx - 18:.1f}" y="{Y0 - 24}" width="12" height="12" rx="2" fill="{c["bars"][j]}"/>')
        lx -= 34
    out.append('</svg>')
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    d = Path("docs")
    d.mkdir(exist_ok=True)
    for t in THEMES:
        (d / f"bench-{t}.svg").write_text(svg(t), encoding="utf-8")
        print(f"docs/bench-{t}.svg")
