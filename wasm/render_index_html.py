#!/usr/bin/env python3

import json
from os import makedirs, path
import shutil
import sys
import jinja2

CURRENT_DIR = path.dirname(__file__)
DEST_DIR = sys.argv[1]

def load_chip8archive() -> dict[str, dict[str, str | int]]:
    programs_json = path.join(CURRENT_DIR, "../chip8Archive/programs.json")
    if not path.exists(programs_json):
        return {}
    with open(programs_json, "r") as f:
        roms_json = json.load(f)
    roms = {}
    for name, info in roms_json.items():
        if info.get("platform") not in {"chip8", "schip"}:
            continue
        if info["options"].get("logicQuirks"):
            mode = "chip8"
        elif info["options"].get("jumpQuirks"):
            mode = "schip"
        else:
            mode = "octo"
        roms[name] = {
            "title": info["title"], "mode": mode, "tickrate": int(info["options"]["tickrate"]),
            "fillColor": info.get("options", {}).get("fillColor"),
            "backgroundColor": info.get("options", {}).get("backgroundColor"),
        }
    return roms

def copy_roms(roms: dict[str, dict], dest_dir: str) -> None:
    makedirs(path.join(dest_dir, "roms"), exist_ok=True)
    for name in roms.keys():
        shutil.copy(
            path.join(CURRENT_DIR, f"../chip8Archive/roms/{name}.ch8"),
            path.join(dest_dir, "roms"),
        )

if __name__ == "__main__":
    roms = load_chip8archive()
    copy_roms(roms, DEST_DIR)

    with open(path.join(CURRENT_DIR, "index.html.jinja"), "r") as f:
        tmpl = jinja2.Environment().from_string(f.read())

    with open(path.join(DEST_DIR, "index.html"), "w") as f:
        f.write(tmpl.render(roms=roms))
