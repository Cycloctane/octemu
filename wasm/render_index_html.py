#!/usr/bin/env python3

import json
from os import environ, makedirs, path
import shutil
import sys

import jinja2

TEMPL = sys.argv[1]
OUTPUT_DIR = sys.argv[2]
CHIP8_ARCHIVE = environ.get("CHIP8_ARCHIVE_PATH", "../chip8Archive/")

def load_chip8archive() -> dict[str, dict[str, str | int]]:
    programs_json = path.join(CHIP8_ARCHIVE, "programs.json")
    if not path.exists(programs_json):
        return {}
    with open(programs_json, "r") as f:
        roms_json = json.load(f)
    roms = {}
    for name, info in roms_json.items():
        if info.get("platform") not in {"chip8", "schip"} or "options" not in info:
            continue
        if info["options"].get("logicQuirks"):
            mode = "chip8"
        elif info["options"].get("jumpQuirks"):
            mode = "schip"
        else:
            mode = "octo"
        roms[name] = {
            "title": info["title"], "mode": mode, "tickrate": int(info["options"]["tickrate"]),
            "fillColor": info["options"].get("fillColor"),
            "backgroundColor": info["options"].get("backgroundColor"),
            "desc": info.get("desc"), "howto": info.get("howto"),
        }
    return roms

def copy_roms(roms: dict[str, dict]) -> None:
    makedirs(path.join(OUTPUT_DIR, "roms/"), exist_ok=True)
    for name in roms.keys():
        shutil.copy(
            path.join(CHIP8_ARCHIVE, "roms/", f"{name}.ch8"),
            path.join(OUTPUT_DIR, "roms/"),
        )

if __name__ == "__main__":
    roms = load_chip8archive()
    copy_roms(roms)

    with open(TEMPL, "r") as f:
        tmpl = jinja2.Environment().from_string(f.read())

    with open(path.join(OUTPUT_DIR, "index.html"), "w") as f:
        f.write(tmpl.render(roms=roms))
