# The AES plugin for Beebium: user guide

Labels: DOC, a document says so; SRC, source code or the ROM's own bytes say so; INF, inferred; USER, the author's own statement or decision; UNK, unknown.

## What it is

The plugin adds the Acorn ARM Evaluation System (AES) to the Beebium BBC Micro emulator. The AES was Acorn's 1986 second processor for the BBC Micro, built around the ARM1, the first ARM chip. It connects through the Tube, and here it attaches to Beebium's own Tube. The AES's own control program, the Executive, comes from a ROM image you supply. Links to the ROM, the manuals and the system discs are at the foot of this guide, under "Where to find things". The known images, and which two the tests know, are in the table under "Executive ROM images".

## How accurate it aims to be

The aim is cycle accuracy: the ARM1 should run at the AES's speed, cycle by cycle, and not just give the right answers (USER).

- **Clock.** Sequential memory cycles run at 6.67 MHz, 150 ns each: the board's 20 MHz crystal divided by 3 (DOC). Non-sequential cycles take twice as long, 300 ns, the crystal divided by 6 (DOC).
- **Instruction timing.** The cost of each instruction in sequential and non-sequential memory cycles comes from Acorn's manuals (DOC).
- **Board delays.** The extra delays from the board itself come from reading its circuit equations (INF):
  - when memory is refreshed;
  - the slowdown when code runs across a 32-word block;
  - the ROM and Tube wait states.
- **Rare cases.** A few cases no normal program should meet rest on the later ARM2's datasheet, assumed to apply to the ARM1 (INF).
- **ClockSp.** The BBC BASIC benchmark ClockSp scores 37.16 on the plugin. Two real AESes scored 35.68 and 35.73 (DOC, second-hand: two forum posts). So the plugin's score is about 4% higher (INF, arithmetic). In running time it is about 2.4% fast: each of ClockSp's eight sub-test times can be worked back from its printed score, and the score adds rates, so a short sub-test counts for more than a long one (INF, arithmetic from the printed scores). Nothing has been adjusted to close that gap (USER).
- **Not yet checked against a real AES.** Every timing above is a reading of the documents, not a measurement (USER). The model has not yet been compared with results from a real AES, and those results will be used to check it.
- **Not yet known (UNK):**
  - some timing details, such as whether a block break is paid straight after a load;
  - how some unusual instructions behave on a real ARM1.

## How it was built

It was written from Acorn's documents, the circuit diagram and the ROM's own code, without reference to any other emulator's source (USER). Beebium supplies the BBC Micro and its Tube chip; the plugin adds the ARM1 core, the AES memory map, the ROM and the board's timing.

## Running it

Tested with Beebium 0.15.0. How to build it first: `BUILDING.md`, at the top of the package.

Add one option to Beebium's command line:

```
--tube-aes rom=<file>[:ram=2|4][:abort=off|strict]
```

The wording for each setting is taken from the plugin's manifest:

- **`rom`**, "Executive ROM": "The AES Executive ROM: a 16 KB image, as a path. Beebium does not supply one." Required: always give `rom=<file>`. Without it the server stops with "missing required parameter 'rom'". The tested ROM has SHA-1 `f86bbc4894e62725b8ef22d44e7f44d37c98ac14`; inside, it reads "Executive version 1.00 (14th August 1986)" (SRC).

  Where the ROM file can be: `rom=` takes a full path, or a path relative to the folder the server was started in. With the commands in `BUILDING.md`, that is Beebium's folder. Tried with Beebium 0.15.0 on 17 November 2026:
  - a full path: the server started, from any folder;
  - a bare file name, with the file in the folder the server was started in: the server started;
  - the same bare name, started from another folder: the server stopped with `Error: AES: no file at myrom.rom`;
  - no `rom=` at all: the server stopped with `Error: --tube-aes: missing required parameter 'rom'`, followed by the setting's description.
- **`ram`**, "Memory": "How much memory the AES has." Default `4`.
  - `2`, "2 MB": "The AES with its memory card removed."
  - `4`, "4 MB": "The standard AES."
- **`abort`**, "Memory aborts": "An emulator aid: whether the ARM is told when it accesses memory that isn't emulated." Default `off`.
  - `off`, "Off (as hardware)": "No aborts are raised."
  - `strict`, "Strict": "An abort is raised for any access outside the emulated memory."

Example, with a DFS ROM in slot 14:

```
beebium-model-b --fdc acorn-1770 --sideways slot=14:type=rom:image=acorn-dfs_2_26.rom --tube-aes "rom=/path/to/your/rom"
```

`beebium-model-b-plus` also works without the DFS options, because it loads the DFS itself (seen working, 17 November 2026; not covered by the tests).

On boot, the screen should show the AES banner, `ARM Second Processor 4096K`, and the `A*` prompt.

## Executive ROM images

Eight images of the Executive are known. "Facts file": the tests tied to a ROM run for the two with a file in `aes-coprocessor/tests/rom-facts/` and skip for any other. "Boots" is what this plugin does with the image: emulator output, not evidence about a real board.

| Version and date | SHA-1 begins | Where to find it | Boots | Facts file |
|---|---|---|---|---|
| 1.00, 14th August 1986 (SRC); the tested ROM | `f86bbc48` | the Stardot thread | Yes | Yes |
| 1.00, 14th August 1986 (SRC) | `9f451f53` | the Stardot thread | Yes | Yes |
| 1.00, 6th June 1986 (SRC) | `15e15c78` | the Stardot thread | Yes | No |
| 1.00, 6th June 1986 (SRC) | `74af1e9f` | the Stardot thread | Yes | No |
| 1.00, 6th June 1986 (SRC) | `ba33eaf1` | the Stardot thread | Yes | No |
| Brazil -.005, 8th August 1986 (SRC) | `abcc71cb` | the Stardot thread | Yes | No |
| Brazil -.006, 11th July 1986 (SRC) | `bfa252de` | the MAME bbc_tube_arm.zip ROM bundle | Yes | No |
| Brazil -.005, 8th August 1986 (SRC) | `5b051d0a` | the Stardot thread | No: the screen stays blank, here and in MAME (USER); probably a damaged copy (INF) | No |

The Stardot thread is the first link under "Where to find things".

## Where to find things

- **The ROM:** the Stardot thread "Arm Eval System: Executive ROM Variants", <https://stardot.org.uk/forums/viewtopic.php?t=31805>, lists the known versions. The plugin takes any 16 KB image you supply. Other places that hold the images:
  - <https://acorn.huininga.nl/pub/roms/Acorn/ARM%20Evaluation%20Kit%20Second%20Processor/>
  - <https://tobylobster.github.io/rom_library/?md5=ed14f6139c641b995867def2a5f2d5d3>
- **Copies of the original manuals:**
  - ARM Hardware Reference Manual, ARM Software Reference Manual: <https://chrisacorns.computinghistory.org.uk/docs/Acorn/OEM/OEM.html>
  - Tube Application Note 004: <https://chrisacorns.computinghistory.org.uk/docs/Acorn/AN/ApplicationNotes.html>
  - ARM2 datasheet, ARM Architecture Reference Manual chapter on 26-bit architectures: <https://home.marutan.net/arcemdocs/>
  - ARM CPU Software Manual, Kernel source: <https://gtoal.com/acorn/arm/>
- **Copies of the original system discs:** <https://mdfs.net/Mirror/Archive/Acorn/ARMEval/>
- **Brazil Manual remake:** <https://stardot.org.uk/forums/viewtopic.php?p=457364#p457364>
- **Tak on ARM, benchmarks across the AES's languages:** <https://stardot.org.uk/forums/viewtopic.php?t=33256>; repository: <https://github.com/acheton1984/ReTestingTheTak/tree/main/aes>
