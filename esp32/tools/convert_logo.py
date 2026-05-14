#!/usr/bin/env python3
"""
Convertit un PNG en tableau C RGB565 pour l'affichage ILI9488.
Usage : python3 convert_logo.py <input.png> <output.hpp> [taille]

Exemple :
    python3 esp32/tools/convert_logo.py logo.png esp32/main/spider_logo.hpp 32

Les pixels transparents (alpha < 128) et les pixels blancs purs (R,G,B > 240)
sont encodés 0xFFFF — la fonction drawRotatedLogo() les rend transparents
(fond noir de l'en-tête).

Dépendance : pip install Pillow
"""
import sys
import math
from PIL import Image


def rgb_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert(input_path: str, output_path: str, size: int = 32) -> None:
    img = Image.open(input_path).convert("RGBA")
    img = img.resize((size, size), Image.LANCZOS)

    pixels = []
    for y in range(size):
        for x in range(size):
            r, g, b, a = img.getpixel((x, y))
            if a < 128 or (r > 240 and g > 240 and b > 240):
                pixels.append(0xFFFF)  # transparent
            else:
                pixels.append(rgb_to_rgb565(r, g, b))

    with open(output_path, "w") as f:
        f.write("#pragma once\n")
        f.write(f"// Généré par convert_logo.py depuis {input_path} ({size}x{size})\n")
        f.write("// 0xFFFF = pixel transparent (fond noir sur l'écran).\n")
        f.write("#include <cstdint>\n\n")
        f.write("namespace spiderbot {\n\n")
        f.write(f"static constexpr int kLogoSize = {size};\n\n")
        f.write("static constexpr uint16_t kSpiderLogoData[kLogoSize * kLogoSize] = {\n")
        for i in range(0, len(pixels), 16):
            row = pixels[i : i + 16]
            f.write("    " + ", ".join(f"0x{p:04X}" for p in row) + ",\n")
        f.write("};\n\n")
        f.write("}  // namespace spiderbot\n")

    print(f"OK : {input_path} -> {output_path}  ({size}x{size}, {len(pixels)} pixels)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} input.png output.hpp [taille=32]")
        sys.exit(1)
    size = int(sys.argv[3]) if len(sys.argv) >= 4 else 32
    convert(sys.argv[1], sys.argv[2], size)
