#!/usr/bin/env python3

import json
import jinja2

def bin2carray(path: str) -> str:
    with open(path, "rb") as f:
        data = f.read()
    lines = [", ".join(f"0x{b:02X}" for b in data[i:i+16]) for i in range(0, len(data), 16)]
    carray = ",\n\t".join(lines)
    return carray

if __name__ == "__main__":
    import sys

    with open("rom.c.jinja", "r") as f:
        tmpl = jinja2.Environment(autoescape=False).from_string(f.read())

    input_file = sys.argv[1]
    with open(input_file, "r") as f:
        if input_file.endswith(".yaml") or input_file.endswith(".yml"):
            import yaml
            rom_config = yaml.safe_load(f)
        else:
            import json
            rom_config = json.load(f)

    rom_c = tmpl.render(
        roms=[
            {
                "title": rom["title"].translate(str.maketrans({'"': '_', '\\': ''})),
                "mode": rom.get("mode", "octo"),
                "tickrate": int(rom.get("tickrate", 100)),
                "data": bin2carray(rom["file"]),
            } for rom in rom_config.values()
        ]
    )

    with open(sys.argv[2].removesuffix(".c") + ".c", "w") as f:
        f.write(rom_c)
