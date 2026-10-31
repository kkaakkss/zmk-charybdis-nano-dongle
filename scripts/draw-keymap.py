"""Generate diagrams from the firmware keymap and its physical-layout DTS.

Run from any directory: python scripts/draw-keymap.py
Requires: python -m pip install -r requirements-draw.txt
"""
from pathlib import Path
import shutil
import subprocess
import sys
import yaml

ROOT = Path(__file__).resolve().parents[1]

def main():
    # Prefer the CLI belonging to this Python environment.
    cli = Path(sys.executable).parent / ("keymap.exe" if sys.platform == "win32" else "keymap")
    command = str(cli) if cli.is_file() else shutil.which("keymap")
    if not command:
        raise SystemExit("Install requirements-draw.txt in this Python environment first")
    out = ROOT / "keymap-drawer"
    out.mkdir(exist_ok=True)
    def run(*args):
        subprocess.run([command, "-c", "keymap_drawer.config.yaml", *args], cwd=ROOT, check=True)
    parsed = "keymap-drawer/nano35.yaml"
    run("parse", "-z", "config/nano35.keymap", "-o", parsed)
    data = yaml.safe_load((ROOT / parsed).read_text(encoding="utf-8"))
    # Keymap Editor may change display-name/label. Names and layer count are user data.
    layers = data.get("layers", {})
    if not layers:
        raise SystemExit("No keymap layers were parsed")
    for name, keys in layers.items():
        if len(keys) != 35:
            raise SystemExit(f"Layer {name!r}: expected 35 positions, got {len(keys)}")
    if any(not 0 <= position < 35 for combo in data.get("combos", []) for position in combo["p"]):
        raise SystemExit("Combo position outside the Nano35 range 0..34")
    # Reuse the actual firmware layout; no independent hand-maintained key order.
    data["layout"] = {"dts_layout": "boards/shields/nano35/nano35.dtsi", "layout_name": "nano35_layout"}
    # Show applicability on each separate combo diagram (0..6 match firmware layers).
    names = list(data["layers"])
    for combo in data.get("combos", []):
        key = combo["k"]
        key = {"t": key} if isinstance(key, str) else dict(key)
        scopes = combo.get("l", names)
        scope_label = "L" + ",".join(str(names.index(name)) for name in scopes)
        key["s"] = scope_label + (" / " + str(key["s"]) if key.get("s") else "")
        combo.update(k=key, w=100, h=46)
    (ROOT / parsed).write_text(yaml.safe_dump(data, sort_keys=False, allow_unicode=True), encoding="utf-8", newline="\n")
    run("draw", parsed, "--keys-only", "-o", "keymap-drawer/nano35.svg")
    run("draw", parsed, "--combos-only", "-o", "keymap-drawer/nano35-combos.svg")
    run("draw", parsed, "--keys-only", "-s", names[0], "-o", "keymap-drawer/nano35-base.svg")
    print(f"Generated {len(data['layers'])} x 35 keys and {len(data.get('combos', []))} combos")

if __name__ == "__main__":
    main()

