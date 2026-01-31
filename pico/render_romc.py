#!/usr/bin/env python3

import jinja2

def bin2carray(path: str) -> str:
    with open(path, "rb") as f:
        data = f.read()
    lines = [", ".join(f"0x{b:02X}" for b in data[i:i+16]) for i in range(0, len(data), 16)]
    carray = ",\n\t".join(lines)
    return carray

_escape = lambda s: s.translate(str.maketrans({'"': '', '\\': ''}))

def load_yml(file: str) -> list:
    import yaml
    with open(file, "r") as f:
        return [
            {
                "title": _escape(rom["title"]),
                "mode": rom.get("mode", "octo"),
                "tickrate": int(rom.get("tickrate", 100)),
                "data": bin2carray(rom["file"]),
            } for rom in yaml.safe_load(f).values()
        ]

def load_chip8achive() -> list:
    import json
    with open("../chip8Archive/programs.json", "r") as f:
        roms_json = json.load(f)
    roms = []
    for name, info in roms_json.items():
        if info.get("platform") not in ("chip8", "schip"):
            continue
        if info["options"].get("logicQuirks"):
            mode = "chip8"
        elif info["options"].get("jumpQuirks"):
            mode = "schip"
        else:
            mode = "octo"
        roms.append({
            "title": _escape(info["title"]), "mode": mode,
            "tickrate": int(info["options"]["tickrate"]),
            "data": bin2carray(f"../chip8Archive/roms/{name}.ch8"),
        })
    return roms

if __name__ == "__main__":
    import sys

    with open("rom.c.jinja", "r") as f:
        tmpl = jinja2.Environment(autoescape=False).from_string(f.read())

    roms = load_yml(sys.argv[2]) if len(sys.argv) > 2 else load_chip8achive()
    assert len(roms) > 0, "No ROMs"

    with open(sys.argv[1].removesuffix(".c") + ".c", "w") as f:
        f.write(tmpl.render(roms=roms))
