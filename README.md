# AESulator

AESulator is a [Beebium](https://github.com/rob-smallshire/beebium) coprocessor plugin emulating the Acorn ARM Evaluation System (ARM1, 20 MHz crystal, Tube second processor). It was written from Acorn's documents and the ROM's own code, without reference to any other emulator's source. It was developed with the help of Claude, an AI assistant from Anthropic.

## What you need

- Beebium 0.20.1, with what it needs to build (see `BUILDING.md`). AESulator is built and tested with that version.
- `uv`, for the server tests.
- An Executive ROM image for the ARM Evaluation System. The ROM is your own and is not included; `docs/user-guide.md` says where to look.

## Where to start

Read `docs/user-guide.md`: what the plugin is, how accurate it aims to be and how to run it. `BUILDING.md` says how to build, test and run it. The design document, `docs/arm1-and-aes.md`, says what the ARM1 and the board are, what is not known and what AESulator does there, and the evidence for each.

## What is here

- `acorn-aes-coprocessor/`: the plugin's source, its tests and its manifest.
- `tools/`: the scripts that build, test and check it.
- `aes-paths.template.sh`: copy it to `aes-paths.sh` and fill it in.
- `BUILDING.md`: how to build, test and run it.
- `docs/user-guide.md`: the user guide.
- `docs/arm1-and-aes.md`: the design document: what the ARM1 and the AES board are, what is not known and what AESulator does there, the evidence and the sources.

## Licence

AESulator is free software, released under the GNU General Public License, version 3 or (at your option) any later version. The full text is in `LICENSE`. Each source file carries the licence notice in its header.
