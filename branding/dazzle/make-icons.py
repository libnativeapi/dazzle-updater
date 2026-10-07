#!/usr/bin/env python3
"""Builds the demo's application icons from the brand mark, one per platform.

The mark (dazzle-updater-mark.svg) is the master. The tile it sits on is the brand page's
"App icon" surface — the dark #07111f tile with the soft gradient — but each platform
gets the tile drawn to its own icon grid, and every small size (24 px and below) gets the
mark in one solid colour, as the brand's optical-size test asks:

  macOS    1024 canvas, 824 rounded square centred (Apple's icon grid: 100 px margins,
           continuous ~22.4 % corners); the system adds the shadow and, on macOS 26,
           its own mask and glass. -> DazzleDemo.icns
  Windows  square, full bleed, 20 % corners (no system mask, no margins); the sizes
           Windows and the WinUI backend ask for, 16 to 256. -> DazzleDemo.ico
  Linux    the GNOME grid: 128 canvas, 112 tile with 8 px margins, 20 % corners;
           hicolor PNGs 16..512, a scalable SVG, and a -symbolic SVG (one colour,
           currentColor) for the places GNOME recolours icons for light and dark.

Needs rsvg-convert, iconutil (macOS) and magick. Run from the repository root:
  python3 branding/dazzle/make-icons.py
"""
import os
import pathlib
import re
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent
MARK = (HERE / "dazzle-updater-mark.svg").read_text()
PATHS = re.search(r'<g fill="url\(#g\)">(.*?)</g>', MARK, re.S).group(1).strip()

DARK_TILE = "#07111f"
SOLID_SMALL = "#67E8F9"  # the soft gradient's middle stop: one colour at 24 px and below
SOFT_GRADIENT = '''<linearGradient id="g" gradientUnits="userSpaceOnUse" x1="20" y1="108" x2="104" y2="12">
      <stop offset="0" stop-color="#5599F5"/><stop offset=".55" stop-color="#67E8F9"/><stop offset="1" stop-color="#A5F3FC"/>
    </linearGradient>'''


def tile_svg(canvas, tile, radius, mark_ratio=0.65, solid=False, label=""):
    """An icon: `tile` px rounded square centred in `canvas`, the mark at `mark_ratio`."""
    off = (canvas - tile) / 2
    mark = tile * mark_ratio
    moff = (canvas - mark) / 2
    fill = SOLID_SMALL if solid else "url(#g)"
    defs = "" if solid else f"<defs>\n    {SOFT_GRADIENT}\n  </defs>\n  "
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {canvas} {canvas}" role="img" aria-label="dazzle updater {label}">
  <title>dazzle updater</title>
  {defs}<rect x="{off:g}" y="{off:g}" width="{tile:g}" height="{tile:g}" rx="{radius:g}" ry="{radius:g}" fill="{DARK_TILE}"/>
  <g transform="translate({moff:.3f} {moff:.3f}) scale({mark / 128:.6f})" fill="{fill}">
    {PATHS}
  </g>
</svg>
'''


def symbolic_svg():
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16">
  <title>dazzle updater (symbolic)</title>
  <g transform="scale(0.125)" fill="#2e3436" class="warning success error">
    {PATHS}
  </g>
</svg>
'''


def render(svg_text, size, out, work):
    src = work / f"{out.stem}-{size}.svg"
    src.write_text(svg_text)
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), str(src), "-o", str(out)], check=True)


def main():
    work = pathlib.Path(tempfile.mkdtemp())
    # Platform master SVGs, kept beside the mark for the record.
    macos = tile_svg(1024, 824, 824 * 0.2237, label="app icon, macOS")
    windows = tile_svg(256, 256, 256 * 0.20, label="app icon, Windows")
    linux = tile_svg(128, 112, 112 * 0.20, label="app icon, Linux")
    (HERE / "app-icon-macos.svg").write_text(macos)
    (HERE / "app-icon-windows.svg").write_text(windows)
    (HERE / "app-icon-linux.svg").write_text(linux)
    (HERE / "app-icon-symbolic.svg").write_text(symbolic_svg())

    # macOS: the iconset, 16 to 1024; 16 and 32@1x in one colour.
    iconset = work / "DazzleDemo.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            px = size * scale
            solid = px <= 32
            svg = tile_svg(1024, 824, 824 * 0.2237, solid=solid) if not solid else tile_svg(1024, 824, 824 * 0.2237, mark_ratio=0.72, solid=True)
            name = f"icon_{size}x{size}{'@2x' if scale == 2 else ''}.png"
            render(svg, px, iconset / name, work)
    icns = ROOT / "core/examples/updater_ui_example/DazzleDemo.icns"
    subprocess.run(["iconutil", "--convert", "icns", "--output", str(icns), str(iconset)], check=True)

    # Windows: every size the shell and the WinUI backend (64 px at the DPI) pull.
    ico_pngs = []
    for px in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256):
        solid = px <= 24
        svg = tile_svg(256, 256, 256 * 0.20, mark_ratio=0.72 if solid else 0.65, solid=solid)
        out = work / f"ico-{px}.png"
        render(svg, px, out, work)
        ico_pngs.append(str(out))
    ico = ROOT / "examples/winui3_updater_ui_example/DazzleDemo.ico"
    subprocess.run(["magick", *ico_pngs, str(ico)], check=True)

    # Linux: hicolor, plus the scalable and symbolic SVGs.
    hicolor = ROOT / "core/examples/updater_ui_example/icons/hicolor"
    if hicolor.exists():
        shutil.rmtree(hicolor)
    for px in (16, 22, 24, 32, 48, 64, 128, 256, 512):
        solid = px <= 24
        svg = tile_svg(128, 112, 112 * 0.20, mark_ratio=0.72 if solid else 0.65, solid=solid)
        render(svg, px, hicolor / f"{px}x{px}/apps/com.example.dazzle-demo.png", work)
    scalable = hicolor / "scalable/apps/com.example.dazzle-demo.svg"
    scalable.parent.mkdir(parents=True)
    scalable.write_text(linux)
    symbolic = hicolor / "symbolic/apps/com.example.dazzle-demo-symbolic.svg"
    symbolic.parent.mkdir(parents=True)
    symbolic.write_text(symbolic_svg())

    # Previews of the three masters, for looking at.
    for name, svg, px in (("macos", macos, 1024), ("windows", windows, 256), ("linux", linux, 128)):
        render(svg, px, HERE / f"preview-app-icon-{name}.png", work)
    print("icons written:", icns.relative_to(ROOT), ico.relative_to(ROOT), hicolor.relative_to(ROOT))


if __name__ == "__main__":
    os.chdir(ROOT)
    main()
