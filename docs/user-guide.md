# The AES plugin for Beebium: user guide

## What it is

The plugin adds the Acorn ARM Evaluation System (AES) to the Beebium BBC Micro emulator. The AES was Acorn's 1986 second processor for the BBC Micro, built around the ARM1, the first ARM chip. It connects through the Tube, and here it attaches to Beebium's own Tube. The AES's own control program, the Executive, comes from a ROM image you supply. Links to the ROM, the manuals and the system discs are at the foot of this guide, under "Where to find things". The known images, and which ones the tests know, are in the table under "Executive ROM images".

## How accurate it aims to be

The aim is cycle accuracy: the ARM1 should run at the AES's speed, cycle by cycle, and not just give the right answers. Its timing comes from Acorn's documents, the board's circuit and tests on a real AES; nothing was adjusted to fit a benchmark score.

The BBC BASIC benchmark ClockSp 2.01 scores 35.83 on the plugin, against 35.71 on a real AES and 35.68 (<https://stardot.org.uk/forums/viewtopic.php?p=128579#p128579>) and 35.73 (<https://stardot.org.uk/forums/viewtopic.php?p=147162#p147162>) reported for two others.

Where the real hardware's behaviour is not known, AESulator has switches, set when it is built, so the code can be changed as more is learned. The design document, [The ARM1 and the AES](arm1-and-aes.md), lists them, with what the ARM1 and the AES are and the evidence.

## How it was built

It was written from Acorn's documents, the circuit diagram and the ROM's own code, without reference to any other emulator's source. Beebium supplies the BBC Micro and its Tube chip; the plugin adds the ARM1 core, the AES memory map, the ROM and the board's timing.

## What the tests do

The tests check that the plugin behaves as Acorn's documents say, and that real ROM code runs on it: the AES's Executive and the host's own ROM code. They do not compare it with a real AES. Their expected results were written from the documents and the Executive's source, never from an emulator. How to run them, how skips work, and the check that breaks the code on purpose to see whether the tests notice: `acorn-aes-coprocessor/README.md`.

## Running it

Tested with Beebium 0.20.1. How to build it first: `BUILDING.md`, at the top of the package.

Add one option to Beebium's command line:

```
--tube-aes rom=<file>[:ram=2|4][:abort=off|strict]
```

The wording for each setting is taken from the plugin's manifest:

- **`rom`**, "Executive ROM": "The AES Executive ROM: a 16 KB image, as a path. Beebium does not supply one." Required: always give `rom=<file>`. Without it the server stops with "missing required parameter 'rom'". The tests are built on the ROM with SHA-1 `f86bbc4894e62725b8ef22d44e7f44d37c98ac14`; inside, it reads "Executive version 1.00 (14th August 1986)".

  Where the ROM file can be: `rom=` takes a full path, or a path relative to the folder the server was started in. With the commands in `BUILDING.md`, that is Beebium's folder.
  - A full path works from any folder.
  - A bare file name works when the file is in the folder the server was started in. Started from another folder, the server stops with `Error: AES: no file at myrom.rom`.
  - With no `rom=` at all, the server stops with `Error: --tube-aes: missing required parameter 'rom'`, followed by the setting's description.
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

`beebium-model-b-plus` also works without the DFS options, because it loads the DFS itself; the tests don't cover it.

On boot, the screen should show the AES banner, `ARM Second Processor 4096K`, and the `A*` prompt.

The version is in the server's own output when it starts, on the AES's line after `Initialising extensions...`, for example `ARM1 (6.67 MHz) coprocessor, AESulator 2.0.0, built with Beebium 0.20.1`.

## The debugger

The AES has its own debugger, reached through Beebium's Python client. It has no window and no text console: you drive it from a short Python script.

Start the server as above. Then, from this package's folder, save the lines below in a file and run it with `sh tools/run-py.sh <file>`. That uses the tests' Python environment, which already has Beebium's client. The examples follow on from each other in one script.

```python
from beebium.client import Beebium

bbc = Beebium.connect()              # the server on this machine
with bbc.coprocessor() as aes:       # the AES's side
    ...                              # the examples below go here
```

### Stop, look and step

```python
aes.debugger.stop()
regs = aes.cpu.registers
print(hex(regs["R15"]), hex(regs["R13_svc"]), regs.status)
print(aes.debugger.step())
```

Stopping the AES leaves the BBC running. `step()` runs one instruction, or one exception entry, and reports what it cost; the debugger counts time in ticks of the AES's 20 MHz crystal (50 ns each).

### Registers

- `R0` to `R14` are the registers of the mode the ARM is in.
- `R15` is the address of the instruction about to run. A program reading R15 sees 8 more, as the ARM's pipeline gives it.
- `PSR` is the flags (N, Z, C, V), the interrupt masks (I, F) and the mode (M1, M0). `regs.status` shows them by name, capital for a 1. The PSR is read-only: setting it changes nothing and gives no error.
- Every banked register can be read and set by name, whatever the mode: `R10_usr` to `R14_usr`, `R10_fiq` to `R14_fiq`, `R13_irq`, `R14_irq`, `R13_svc` and `R14_svc`.

```python
aes.cpu.update(r0=0x1234)
print(hex(aes.cpu.registers["R0"]))
```

Setting `R15` moves the program counter: the instruction at that address runs next. Moving it takes no time and makes no access to memory.

`aes.cpu.signals` shows the IRQ and FIQ lines from the Tube: whether each is up, waiting (up and not masked), and whether the ARM is in its handler.

### Memory

The debugger shows three regions:

| Region | Address | Size | Can the debugger write it? |
|---|---|---|---|
| `ram` | `&0` | the RAM fitted, 2 or 4 MB | Yes |
| `tube` | `&1000000` | 32 bytes, the Tube's eight registers | No |
| `rom` | `&3000000` | 16 KB, the Executive ROM | No |

```python
print(aes.memory.region("rom").peek.read(0x3000000, 16).hex())
aes.memory.region("ram").bus[0x8000] = 0x42
print(aes.memory.region("ram").peek[0x8000])
```

Each region is shown once, without the repeats the ARM sees. Looking at memory changes nothing: reading a Tube register here does not take its byte. The `ram` region is RAM itself, even straight after a reset, when the ARM's own reads at low addresses still reach the ROM. A write to `tube` or `rom` is refused with an error.

### Breakpoints and watchpoints

The AES runs only while the BBC runs. With the BBC running, `aes.debugger.run()` starts the AES; `bbc.debugger.run(run_counterpart=True)` starts both together. `run_and_wait_for_stop()` starts the AES and waits until something stops it.

```python
bp = aes.debugger.add_breakpoint(aes.cpu.registers["R15"])
print(aes.debugger.run_and_wait_for_stop())
print(hex(aes.cpu.registers["R15"]))
bp.remove()

wp = aes.debugger.add_watchpoint(0x1000000, 0x1000020, "read")
print(aes.debugger.run_and_wait_for_stop())
wp.remove()
```

- A breakpoint stops the AES before the instruction at its address runs. Running on from a breakpoint steps over it once, so the AES stops there again only on its next pass.
- A watchpoint stops the AES after a load or store that reads or writes an address in its range (`"read"`, `"write"` or `"both"`), with `R15` at the next instruction. Each byte the access moved is checked, so a watchpoint on part of a word still fires. Instruction fetches never trigger a watchpoint.
- Addresses are matched in full, not cut to 16 bits.
- With nothing set, the debugger costs the AES almost no speed.

### Reset

```python
aes.debugger.reset()
print(hex(aes.cpu.registers["R15"]), aes.cpu.registers.status)
aes.debugger.run()
```

Reset restarts the ARM at address 0 in supervisor mode with both interrupts masked, and puts the ROM back at the bottom of memory. RAM keeps its contents.

### What the debugger can't do

- Conditions on breakpoints and watchpoints: Beebium's condition language knows only a 6502's registers, so leave the condition empty.
- Writes to the `tube` or `rom` regions, or to the PSR.
- A window or a text console.

## Executive ROM images

Nine images of the Executive are known. "Facts file": the tests tied to a ROM run for the ones with a file in `acorn-aes-coprocessor/tests/rom-facts/` (see "ROM facts files" below) and skip for any other. "Boots" is what this plugin does with the image: emulator output, not evidence about a real board.

| Version and date | SHA-1 begins | Where to find it | Boots | Facts file |
|---|---|---|---|---|
| 1.00, 14th August 1986; the tests are built on it | `f86bbc48` | acorn.huininga.nl (`ARM Evaluation System 1.01.rom`) | Yes | Yes |
| 1.00, 14th August 1986 | `9f451f53` | mdfs.net (`Eval101.rom`) | Yes | Yes |
| 1.00, 6th June 1986 | `15e15c78` | acorn.huininga.nl (`ARM Evaluation System 1.00.rom`) | Yes | Yes |
| 1.00, 6th June 1986 | `74af1e9f` | mdfs.net (`Eval100.rom`) | Yes | Yes |
| 1.00, 6th June 1986 | `ba33eaf1` | acorn.huininga.nl (`ARMeval_100.rom`) | Yes | Yes |
| Brazil -.005, 8th August 1986 | `df785d8a` | read from the real AES used for the 15 November 2026 results | Yes | Yes |
| Brazil -.005, 8th August 1986 | `abcc71cb` | 8bs.com (`ARMeval_100.rom`) | Yes | Yes, not included |
| Brazil -.006, 11th July 1986 | `bfa252de` | the MAME bbc_tube_arm.zip ROM bundle | Yes | Yes |
| Brazil -.005, 8th August 1986 | `5b051d0a` | acorn.huininga.nl, unsorted ROMs (`ARM_Eval_ROM_August1986`) | No: the screen stays blank, here and in MAME | No |

The sites' pages are linked under "Where to find things".

## ROM facts files

A ROM facts file is a small text file of known facts about one Executive ROM: where its banner and prompt are, the messages it prints, and a few ways it behaves. The tests use it to know what to expect from that ROM. It holds no ROM code. The files are in `acorn-aes-coprocessor/tests/rom-facts/`, each named by its ROM's SHA-1. "Included" means the file comes with this package; a ROM without one still runs, but the tests tied to a ROM skip.

| SHA-1 begins | Version and date, as the ROM reports them | Facts file | Included | Why |
|---|---|---|---|---|
| `f86bbc48` | Executive version 1.00 (14th August 1986) | Yes | Yes | Passes every test; the tests are built on it |
| `9f451f53` | Executive version 1.00 (14th August 1986) | Yes | Yes | Passes every test |
| `15e15c78` | Executive version 1.00 (6th June 1986) | Yes | Yes | An earlier Executive without the cache and soft start, so some tests don't apply |
| `74af1e9f` | Executive version 1.00 (6th June 1986) | Yes | Yes | An earlier Executive without the cache and soft start, so some tests don't apply |
| `ba33eaf1` | Executive version 1.00 (6th June 1986) | Yes | Yes | An earlier Executive without the cache and soft start, so some tests don't apply |
| `df785d8a` | Brazil version -.005 (8th August 1986) | Yes | Yes | Read from a real board; passes every test |
| `abcc71cb` | Brazil version -.005 (8th August 1986) | Yes | No | Buffered file access behaved unexpectedly in our tests |
| `bfa252de` | Brazil version -.006 (11th July 1986) | Yes | Yes | Passes every test |
| `5b051d0a` | Brazil version -.005 (8th August 1986) | No | No | Doesn't start |

## Where to find things

- **The ROM:** the Stardot thread "Arm Eval System: Executive ROM Variants", <https://stardot.org.uk/forums/viewtopic.php?t=31805>, lists the known versions. The plugin takes any 16 KB image you supply. Other places that hold the images:
  - <https://acorn.huininga.nl/pub/roms/Acorn/ARM%20Evaluation%20Kit%20Second%20Processor/>
  - <https://acorn.huininga.nl/pub/unsorted/roms/>
  - <https://mdfs.net/Software/Tube/ARM/>
  - <https://8bs.com/emum5000.htm>
  - <https://tobylobster.github.io/rom_library/?md5=ed14f6139c641b995867def2a5f2d5d3>
- **Copies of the original manuals:**
  - ARM Hardware Reference Manual, ARM Software Reference Manual: <https://chrisacorns.computinghistory.org.uk/docs/Acorn/OEM/OEM.html>
  - Tube Application Note 004: <https://chrisacorns.computinghistory.org.uk/docs/Acorn/AN/ApplicationNotes.html>
  - ARM2 datasheet, ARM Architecture Reference Manual chapter on 26-bit architectures: <https://home.marutan.net/arcemdocs/>
  - ARM CPU Software Manual, Kernel source: <https://gtoal.com/acorn/arm/>
- **Copies of the original system discs:** <https://mdfs.net/Mirror/Archive/Acorn/ARMEval/>
- **Brazil Manual remake:** <https://stardot.org.uk/forums/viewtopic.php?p=457364#p457364>
- **Tak on ARM, benchmarks across the AES's languages:** <https://stardot.org.uk/forums/viewtopic.php?t=33256>; repository: <https://github.com/acheton1984/ReTestingTheTak/tree/main/aes>
