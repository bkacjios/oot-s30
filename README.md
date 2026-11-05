# Ocarina of Time for the PocketGo S30

A native Linux port of *The Legend of Zelda: Ocarina of Time* for the PocketGo
S30 (Allwinner A33, ARMv7 hard-float, Alpine musl), built from the
[ZeldaRET decompilation](https://github.com/zeldaret/oot). The N64's RSP/RDP
graphics are replaced by a display-list interpreter that draws with GLES2 on
KMS/EGL, and the RSP audio microcode is emulated in software and played
through ALSA.

There is no N64 build step and no MIPS toolchain: the layout metadata the port
needs is derived from the ROM on the host.

**No game data is included.** You need your own copy of the game. The default
version is the first N64 release, NTSC 1.0 (`ntsc-1.0`: its blue A and green B
buttons and its original music). The GameCube US ROM (`gc-us`) builds too.

## Requirements

* The pocketgo-s30 build tree (`WORK`, default `../../s30-build`): the Alpine
  sysroot and the `armv7-clang` wrapper
* `arm-none-eabi-gcc` and binutils (the game needs `-fno-toplevel-reorder`)
* a host C compiler, libxml2 headers, python3
* GNU make 3.82 or newer

## Building

```bash
# once: python deps
python3 -m venv .venv && .venv/bin/pip install -U pip -r requirements.txt

# your ROM (.z64, .n64 or .v64 all work)
cp /path/to/your/rom.z64 baseroms/ntsc-1.0/baserom.z64

make -f Makefile.s30 extract          # decompress the ROM, extract assets, generate build inputs
make -f Makefile.s30 assets           # compile the asset segments for ARM (into build_assets/ and data/)
make -f Makefile.s30 -j$(nproc)       # build zelda
```

`extract` builds the host tools it needs the first time it runs.

## Installing

Copy `zelda` and `data/` to `/mnt/sd/home/ports/oot` on the card. The launcher entry
(`apps/Ocarina of Time.sh`) starts it from there and reads optional debug
variables from `/mnt/sd/home/ports/oot/oot.env`, one `VAR=value` per line.

## Runtime options

| variable | effect |
|---|---|
| `OOT_DATA` | asset segment directory (default: `data` next to the binary) |
| `OOT_SAVE` | save file (default: `$XDG_DATA_HOME/oot/oot.sav`, else `~/.local/share/oot/oot.sav`) |
| `OOT_NOAUDIO` | run without sound |
| `OOT_GFXSTATS` | print renderer timings every 100 frames |
| `OOT_HUDTRACE` | log the C-button area's rectangles to `hudtrace.log` when they change |

## Build options

| variable | default | effect |
|---|---|---|
| `VERSION` | `ntsc-1.0` | ROM version directory under `baseroms/` (`ntsc-1.0` or `gc-us`; switching versions needs `extract`, `assets` and a clean build) |
| `WORK` | `../../s30-build` | pocketgo-s30 build tree (sysroot, compiler wrapper) |

## Layout

* `src/s30/`: platform layer (renderer `gfx.c`, display, audio, input, main)
* `src/linux/`: file loading and N64 OS reimplementations
* `src/audio/internal/mixer.c`: the software RSP audio microcode
* `pc_tools/`: the host-only build pipeline (`extract_rom.py`, `rom_meta.py`, `make_assets.py`)
* `Makefile.s30`: the S30 build
* everything else (`src/`, `include/`, `assets/`, `spec/`, `tools/`) is the decompilation

## Credits

Based on [jnmartin84/oot-dc](https://github.com/jnmartin84/oot-dc), whose
authors used LLMs to bootstrap that port. This port would not exist without the
work of the ZeldaRET team and contributors on the
[Ocarina of Time decompilation](https://github.com/zeldaret/oot).
