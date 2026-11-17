# About AESulator

AESulator is a clean-room Beebium coprocessor plugin emulating the Acorn ARM Evaluation System (AES): an ARM1 processor with a 20 MHz crystal, attached to a BBC Micro as a second processor through the Tube. It models the ARM1 itself, the board's memory map, the board's timing cycle by cycle, and the ARM's side of the Tube. Beebium supplies the BBC Micro and its Tube chip.

Each claim about the real hardware carries a label:

- **DOC**: stated in the documents (listed under References).
- **INF**: worked out from the documents, such as the circuit's equations or the ROM's code, but not stated anywhere.
- **UNK**: not known.

## The board in brief

- An ARM1 driven from a 20 MHz crystal (DOC). 2 MB of RAM on the main board, plus 2 MB on a card, for 4 MB (DOC); the plugin offers 2 or 4 MB.
- RAM sits below `&1000000` and repeats every 4 MB (INF). The Tube's eight registers are at `&1000000` and repeat every 32 bytes up to `&17FFFFF` (INF). Nothing answers at `&2000000` (INF). The 16 KB Executive ROM is at `&3000000` and repeats every 16 KB (INF).
- The ROM is one byte wide, so a word takes four accesses (DOC). At reset the ROM also appears at the bottom of memory, until the first write of any kind (DOC).
- The Tube chip is taken to be the same part as on Acorn's other second processors: the host's own Tube code serves them all (INF). The Tube's interrupt lines reach the ARM's IRQ and FIQ (INF from the ROM's code; the circuit diagram agrees, DOC), and the host's reset line holds the ARM in reset (wiring DOC; that it holds, INF).
- What a read of a missing RAM bank returns is UNK. AESulator returns `&FFFFFFFF`.

## The ARM1

Acorn's documents call the processor simply "the ARM"; we write ARM1 and ARM2 to tell the two apart. The first chip was made on a 3 micron process and the redesigned chip on 2 micron (DOC, Wilson 1988).

The ARM1 was the first ARM. It has 25 registers, with a bank of its own for FIQ mode, R10 to R14 (DOC). R15 holds the program counter together with the flags, the interrupt masks and the mode (DOC), so code addresses are 26 bits (INF). Every instruction is conditional, and the PC reads 8 bytes ahead (DOC). There is no multiply instruction and no coprocessor interface: coprocessor instructions trap as undefined (DOC).

## Bugs and differences from later ARMs

The later ARMs include the ARM2 in the Archimedes. The ARM1's manuals and the ARM2's datasheet show where the ARM1 differs (DOC unless marked):

- RRX with the S bit: the carry out is bit 0 OR bit 31, where the ARM2 takes bit 0 only.
- ROR by a register holding more than 31: the carry out is 0. The 1985 and 1986 manuals agree on this; the ARM2 datasheet reads it differently. AESulator follows the manuals.
- LDR and STR can shift their offset by an amount held in another register. The ARM2 doesn't allow this and treats those instructions as undefined. (The ARM2 datasheet says the ARM1 allowed these (DOC); that the shift amount comes from a register is INF.)
- An undefined instruction traps even when its condition fails. The ARM2 skips it.
- LDM of the user registers from a privileged mode was added on the ARM2, so what the ARM1 does with it is UNK. AESulator loads the user registers.
- After a mode change, reads in the next instruction's first cycle still see the old register bank; its writes use the new one (INF).
- The ARM2 datasheet records two bugs in the ARM1 itself. On the AES the abort line is tied inactive (DOC), so neither can arise there (INF). In AESulator an abort comes only from a memory access: with the `abort` setting off, the default, nothing aborts; set to `strict`, an access aborts by its own address only.
  - An LDM of one register in the last three words of a page, with the next page inaccessible, could abort falsely. AESulator can't trigger it: that LDM makes one access, to its own address, which is never in the next page.
  - An abort signalled during an internal cycle, when no memory is requested, still took effect. AESulator can't trigger it: an internal cycle makes no memory access, so it never aborts.

## The board's timing

- Time is counted in ticks of the 20 MHz crystal, 50 ns each. A sequential memory cycle takes 3 ticks (150 ns), a non-sequential one 6 (300 ns) (DOC).
- Each instruction's cost in cycles comes from the 1986 manual (DOC). For LDM of n registers the 1986 manual gives (n+1)S + R15time and the 1985 manual (n+1)S + 1N. The default is the 1986 figure, chosen because it is the AES's own manual, supplied with the machine; the 1985 figure is the alternative of the switch `time.ldm_cycles`. A reading of the hardware manual and the ARM2 datasheet points towards the 1985 figure (INF); the default is kept.
- RAM is refreshed once every 192 ticks (9.6 µs), and each refresh costs 9 ticks (INF). The processor's clock stops while it happens (DOC).
- Only RAM has fast sequential cycles. Every ROM and Tube cycle costs a non-sequential cycle plus a wait: 8 PAL clocks (1.2 µs) for the ROM, 2 (0.3 µs) for the Tube (INF). Whether the base cost under the wait is really a non-sequential cycle is UNK.
- A sequential cycle that crosses into a new 32-word block costs as a non-sequential cycle (INF). Whether the fetch after an internal cycle at a block start pays it as well is UNK.
- The ARM does not wait for the host's 2 MHz clock: no host clock reaches its side of the Tube (INF).
- A data address above `&3FFFFFF`, the 64 MB limit, makes the ARM itself raise an address exception (DOC). The abort line is tied inactive, so memory accesses within the 64 MB always complete (DOC).

## Behaviours no document settles

Where the documents conflict or say nothing, the behaviour is a named switch with a default, not a fixed choice. Three are settings in Beebium (`rom`, `ram` and `abort`; see [the user guide](user-guide.md)). The rest are in `aes-coprocessor/AesOptions.hpp` and `ArmCore.hpp`, and change by editing a default and rebuilding. Some examples:

- A word load or store at an unaligned address: UNK. AESulator ignores the low two bits.
- TST, TEQ, CMP and CMN without the S bit: whether they write a register is UNK. AESulator writes nothing.
- A word access to the Tube: UNK. AESulator passes the low byte.
- An instruction with a 1 in bit 7 of its register shift: the two manuals print the bit differently. AESulator ignores it.
- The value of the PC stored by STR and STM: 12 ahead, adapted from the ARM2 datasheet (INF).

A few smaller guesses have no switch, because no real alternative is known yet.

## What the AESulator tests do

The tests check that AESulator behaves as the documents say, and that real ROM code runs on it: the AES's Executive and the host's own ROM code. They do not check it against a real AES: see "What is not checked". They come in layers:

- **The ARM1 core alone:** data processing, shifts and conditions; loads and stores; branches, register banks, mode changes and reset; exceptions; cycle counts, including the manuals' example programs.
- **The whole board:** the memory map and boot latch, Tube addresses, board time, one small program on the whole board, exceptions with the program's own handlers, the ARM's side of the Tube, and the settings against the manifest.
- **The real Executive ROM:** the banner, the ROM's facts files, and exceptions through the Executive.
- **The plugin in Beebium:** the plugin in the built server, the Tube conversation on the wire, and each kind of Tube call.

The C++ tests are headless; the Python tests run in the built server. The expected results for the Tube conversations, the SWI index and the exception table were written ahead from the documents and the Executive's source, never from an emulator or from Beebium, and a result that disagrees is reported. How skips work, and the check that breaks the code in small deliberate ways to see whether the tests notice, are in `aes-coprocessor/README.md`.

## What is not checked

This release is built from the documents alone. No result from a real AES is used in it yet, so every default above is unconfirmed, and so is every timing figure. Building and running: [BUILDING.md](../BUILDING.md).

## References

| Short name | Title | Part number | Issue or date | What this page takes from it |
|---|---|---|---|---|
| SW | Acorn, ARM Software Reference Manual | 0448,007 | 1986 | Cycle costs, the 1986 LDM figure, the instruction set, shifter carries, address exceptions |
| SW85 | Acorn, ARM CPU Software Manual Version 1.00 | not recorded | Issue 1.00, October 1985 | The 1985 LDM figure; where it agrees or conflicts with SW |
| HW | Acorn, ARM Hardware Reference Manual, with the circuit diagram | 0448,006 | Issue 1.0, 4 August 1986 | The memory map, crystal, refresh, wait states, block crossing, boot latch, Tube wiring, the tied abort line |
| DS87 | Acorn, ARM datasheet (the ARM2), chapter 13 "Compatibility with Prototype ARMs" | 1 85250 0360 0 | Issue 1.0, 17 March 1987 | How the ARM1 differs from the ARM2; the two ARM1 bugs; the adapted defaults |
| Wilson 1988 | Sophie Wilson, "Some facts about the Acorn RISC Machine", comp.arch, <https://groups.google.com/g/comp.arch/c/hPsDLEPf2eo/m/nvJR_d7nnyYJ> | none | 2 November 1988 | The process sizes of the first and the redesigned chip |
| SRC | The Executive's source (Kernel, Middle, Super1) and the ROM's own bytes | none | Executive 1.00 | The interrupt wiring as the ROM uses it; the mode-change delay |
