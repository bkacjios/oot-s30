#!/usr/bin/env python3
"""
The S30's button glyphs from Modern Controller Overhaul's Nintendo texture pack
(t0mtee, CC0, https://github.com/t0mtee/MMModernControllerOverhaul; textures
based on Nerrel's and GhostlyDark's), scaled down to the original textures'
sizes and formats and laid out for the port's buttons (src/s30/buttons.c):
X is C-left, Y is C-down, R1 is C-right (the pack's are Y, X, R). The pack is
Majora's Mask's, but the glyphs are intensity only: the game tints them (OoT's
C buttons in its own yellow). As ports/mm's pc_tools/mco_textures.py.

Written to assets/ (committed), which pc_tools/extract_rom.py prefers to
extracted/ when it makes the textures' .inc.c files.

  pc_tools/mco_textures.py <MMModernControllerOverhaul>/textures/Nintendo/png
"""
import os
import sys

from PIL import Image

PACK = sys.argv[1]


def pack(crc, fmt):
    return Image.open(os.path.join(PACK, f"ZELDA MAJORA'S MASK#{crc}#{fmt}_all.png")).convert("RGBA")


def to_ia8(im, size):
    # Intensity and alpha (resized premultiplied, as RGBA), saved as gray RGBA like the extracted ones (ZAPD reads
    # RGB(A) PNGs only)
    return im.resize(size, Image.LANCZOS).convert("LA").convert("RGBA")


def to_i4(im, size):
    # Intensity only: the glyph's coverage, its letter cut out (luminance x alpha)
    im = im.resize(size, Image.LANCZOS)
    return Image.composite(im.convert("L"), Image.new("L", size, 0), im.getchannel("A")).convert("RGB")


def swap(im, a, b):
    # Two same-sized boxes' contents swapped
    out = im.copy()
    out.paste(im.crop(a), b[:2])
    out.paste(im.crop(b), a[:2])
    return out


def save(im, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.info.pop("icc_profile", None)  # the pack's RGB profile
    im.save(path)
    print(path)


letter_x = pack("ACF93890", "3#1")
letter_y = pack("E973D922", "3#1")
letter_r = pack("218CFC10", "3#1")
circle_a = pack("986CC390", "4#0")
circle_x = pack("9C51DC08", "4#0")
circle_y = pack("C8D66332", "4#0")
circle_r = pack("8D30266D", "4#0")
# The three C buttons as one glyph: the pack's Y on top and X on the left, swapped
circle_c = swap(pack("48D41F13", "4#0"), (58, 5, 162, 109), (0, 110, 104, 214))
# The pause menu's C buttons (left, down, right): the pack's Y X R, as X Y R. The circles' outlines touch, so only
# the letters inside them are swapped (on the plain white fill)
strip = swap(pack("C090A2AC", "3#1"), (78, 58, 218, 198), (310, 58, 450, 198))

PARAM = "assets/textures/parameter_static"
FONT = "assets/textures/nes_font_static"
save(to_ia8(letter_x, (32, 32)), f"{PARAM}/gEmptyCLeftArrowTex.ia8.png")
save(to_ia8(letter_y, (32, 32)), f"{PARAM}/gEmptyCDownArrowTex.ia8.png")
save(to_ia8(letter_r, (32, 32)), f"{PARAM}/gEmptyCRightArrowTex.ia8.png")
save(to_i4(circle_a, (16, 16)), f"{FONT}/gMsgChar9FButtonATex.i4.png")
save(to_i4(circle_c, (16, 16)), f"{FONT}/gMsgCharA1ButtonCTex.i4.png")
save(to_i4(circle_y, (16, 16)), f"{FONT}/gMsgCharA6ButtonCDownTex.i4.png")
save(to_i4(circle_x, (16, 16)), f"{FONT}/gMsgCharA7ButtonCLeftTex.i4.png")
save(to_i4(circle_r, (16, 16)), f"{FONT}/gMsgCharA8ButtonCRightTex.i4.png")
save(to_ia8(strip, (48, 16)), "assets/textures/icon_item_static/gCBtnSymbolsTex.ia8.png")
