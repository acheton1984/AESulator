# The ARM1 and the AES

This is our understanding of the ARM1 and of Acorn's ARM Evaluation System (AES), from Acorn's documents and from tests on a real AES; the evidence is in Part 3. Part 2 says what is not known and what AESulator does there, and Part 4 lists the sources.

## Part 1: what an ARM1 and an AES are

### The ARM1

The ARM1 was the first Acorn RISC Machine, made on a 3 micron chip process; the first chips arrived in April 1985. Most of what is known about it comes from the manuals Acorn supplied with the AES, an earlier Acorn manual from 1985, and the datasheet of its successor, the ARM2. The rest comes from tests on a real AES, or is worked out from the board and its software; Part 2 says where.

#### Registers and modes

There are four modes: user, FIQ, IRQ and supervisor (SVC). There are 25 registers: R0 to R15 as user mode sees them, plus FIQ's own R10 to R14, and IRQ's and SVC's own R13 and R14. R15 holds the program counter together with the flags N, Z, C and V, the interrupt masks I and F, and the mode, so code addresses are 26 bits.

#### The instructions

There are five classes: data processing, single loads and stores, block loads and stores, branches, and SWI. Every instruction carries a condition. There is no MUL or MLA and no coprocessor interface: coprocessor instructions trap as undefined.

#### Shifts and carries

- RRX with the S bit: the carry out is bit 0 OR bit 31.
- A shift by a register holding 0 changes nothing and keeps the carry.
- ROR by a register holding more than 31: the carry out is bit 31 when the amount is a multiple of 32, and 0 for every other amount.

#### Loads, stores and block transfers

- LDR and STR can shift their offset by an amount held in another register.
- STR and STM store the PC 12 ahead of the instruction.
- LDM and STM check only their first address against the 64 MB limit; later words wrap within 26 bits. An LDR or STR that faults does not write its base back.
- LDM with R15 in its list loads the PC only, and with the `^` bit also the parts of the PSR the mode may change. STM with R15 stores the PSR with it; STM with `^` stores the user registers.

#### The pipeline

The ARM1 fetches two words ahead, so a program reading R15 sees its own address plus 8, or plus 12 when it reads R15 in its second cycle: after a shift by a register, or as the value an STR or STM stores.

#### Exceptions

The vectors are at `&00` reset, `&04` undefined instruction, `&08` SWI, `&0C` prefetch abort, `&10` data abort, `&14` address exception, `&18` IRQ and `&1C` FIQ. After an address exception or a data abort, R14 holds the instruction's address plus 8, so a handler can run it again.

A data address above `&3FFFFFF`, the 64 MB limit, makes the ARM1 itself raise an address exception; a code address never does. An undefined instruction traps even when its condition fails.

#### Cycle costs

A tick, in this document, is one cycle of the 20 MHz crystal (50 ns). A sequential (S) memory cycle takes 3 ticks (150 ns) and a non-sequential (N) one 6 ticks (300 ns): the crystal divided by 3 and by 6 (SW p.33).

Each instruction's cost in cycles is as SW p.32 gives it, except for the two costs in "Where the ARM1 differed from the manuals", below:

- An instruction whose condition fails costs 1S (SW p.32).
- LDM of n registers costs (n+1)S + 1N (SW85 p.21).
- A data-processing instruction that shifts by a register costs one S cycle more than SW p.32 gives.
- An LDR or STR whose offset is shifted by a register costs 1S more than one that is not (SW85 p.21; SW p.32).

#### The ARM1's known bugs

The ARM2's datasheet lists five ARM1 bugs, fixed on the ARM2 (DS87 section 13.2):

1. RRX's carry out is bit 0 OR bit 31, where it should be bit 0. The real AES does this.
2. ROR by a register holding a multiple of 32 gives a carry out of 0, where it should be bit 31. The real AES gave bit 31 (see "Where the ARM1 differed from the manuals", below).
3. An LDM of one register in the last three words of a page, with the next page inaccessible, aborts though the load is legal.
4. An `LDM ...^` of one register with writeback, whose base is a banked register such as R13, can write back to the wrong bank's register (B5).
5. An abort signalled during an internal cycle, one with no memory access, still aborts the ARM1.

#### How the ARM1 differs from the ARM2

Besides the bugs above (DS87 section 13.3):

- LDR and STR offsets shifted by a register: allowed on the ARM1, undefined on the ARM2.
- An undefined instruction with a failing condition: traps on the ARM1, skipped on the ARM2.
- FIQ's own registers: R10 to R14 on the ARM1; R8 to R14 on the ARM2, which adds R8_fiq and R9_fiq.
- MUL and MLA, the coprocessor interface, and LDM of the user registers from a privileged mode: ARM2 only.

#### Where the ARM1 differed from the manuals

| Behaviour | The manual | Measured on the real AES | Shown by |
|---|---|---|---|
| LDM of n registers | (n+1)S (SW p.32) | 3n + 9 ticks of the crystal: (n+1)S + 1N, as SW85 p.21 gives | tests 2 and 13 |
| A data-processing instruction that shifts by a register | one S cycle more than with no shift, 6 ticks in all (SW p.32) | 9 ticks: one S cycle more again | test 2 |
| The carry out of ROR by a register holding a multiple of 32 | 0 (SW p.18, SW85 p.12; DS87 section 13.2 lists it as an ARM1 bug) | bit 31 | test 5 Q2, test 9 |

### The AES board

#### What it is

Acorn's 1986 second processor for the BBC Micro: an ARM1 driven from a 20 MHz crystal, with 2 MB of RAM on the main board and 2 MB on a card, 4 MB in all, joined to the host through the Tube.

#### The memory map

The top two address bits, A25 and A24, pick one of four regions:

- `&0000000`: RAM, repeating every 4 MB.
- `&1000000`: the Tube's eight registers, repeating every 32 bytes up to `&17FFFFF`.
- `&2000000`: nothing is connected. A byte read there gives `&00`, with no abort.
- `&3000000`: the 16 KB Executive ROM, repeating every 16 KB.

#### The ROM and the start

The ROM is a 27128, one byte wide, so a word takes four accesses. When the AES is reset, the ROM appears at address 0 as well as at `&3000000`, so the ARM1 starts by running the Executive. The first write the ARM1 makes, to any address, switches the ROM out of address 0, and RAM is there from then on.

#### RAM timing

The board refreshes its RAM once every 192 ticks (9.6 µs), and the ARM1's clock stops for 9 ticks during each refresh.

RAM works in blocks of 32 words. A sequential RAM cycle costs as much as a non-sequential one when the cycle before it was at a block's last word.

#### ROM and Tube timing

Only RAM has fast sequential cycles. Every ROM or Tube cycle costs a non-sequential cycle plus a wait: 24 ticks (1.2 µs) for the ROM and 6 ticks (0.3 µs) for the Tube. The ARM1 does not wait for the host's 2 MHz clock: no host clock reaches its side of the Tube.

#### The Tube link

The Tube chip is the same part as in Acorn's other second processors. Its PIRQ output drives the ARM1's IRQ input, and its PNMI output drives FIQ. The host's reset line is wired to the ARM1's reset, so the ARM1 stays in reset while the line is active.

#### No aborts, no memory management

The abort pin is wired to 0 V, so the board never signals an abort, and there is no memory management, so every access within the 64 MB completes. Bugs 3 and 5 of the ARM1's known bugs need an abort, so they cannot arise on the AES.

#### The Executive

The Executive is the AES's control program, in the ROM. At start-up it sizes the RAM, shows its banner, `Acorn ARM Second Processor 4096K` with 4 MB fitted (two of the known images leave out `Acorn`), and waits at its `A*` prompt. Known versions are Executive 1.00, dated 6th June and 14th August 1986, and the Brazil versions -.006 (11th July 1986) and -.005 (8th August 1986).

#### How fast it is

The BBC BASIC benchmark ClockSp 2.01 gave a Combined Average of 35.71 on the real AES; two other AESes were reported at 35.68 and 35.73. ClockSp 2.10 gave an Unweighted Average of 32.96 on the same board. Timings of the Tak benchmark programs in the AES's languages are in the "Tak on ARM" thread: <https://stardot.org.uk/forums/viewtopic.php?t=33256>.

## Part 2: what is not known, and what AESulator does

AESulator follows Part 1. Where something about the real machine is not known, AESulator makes a choice. Where the documents conflict or say nothing, that choice is a named switch with a default. Three are settings in Beebium: `rom`, `ram` and `abort` (see [the user guide](user-guide.md)). `abort` is an aid for programmers, not a guess about the board: `off`, the default, matches the board, which never aborts; `strict` aborts any access outside the RAM, the Tube's registers and the ROM once a program has entered user mode. Bugs 3 and 5 cannot occur either way. The rest are fields of `ArmOptions` in `acorn-aes-coprocessor/ArmCore.hpp` and `AesOptions` in `AesOptions.hpp`, and change by editing a default and rebuilding. A few smaller choices have no switch, because no real alternative is known yet. Where the real AES settled a behaviour that had been a switch, it is now plain code. Each item says what is not known, or, where Part 1 gives the circuit's or the manuals' answer, what was not tested. Each item's evidence is in Part 3, under its B number.

### Behaviours

#### B1. A word load or store at an unaligned address

**Not known:** what the ARM1 does with a word address that is not a multiple of 4. **AESulator:** ignores the low two bits. **Switch:** `word_unaligned`; the other choice is a rotated load, as on later ARMs.

#### B2. A word access to the Tube

**Not known:** what the other three bytes read when a word is read from a Tube register. **AESulator:** the register's byte in the low eight bits and `&FF` in the other three; a word write sends the low byte. **Switch:** `tube_width`.

#### B3. Bit 7 of a register shift

**Not known:** what an instruction with a 1 in bit 7 of its register-shift field does. The real AES did not trap on it. **AESulator:** runs it as an ordinary register shift. **Switch:** `enc_a_bit7`, default ignored.

#### B4. LDM of the user registers from a privileged mode

**Not known:** what the ARM1 does with this ARM2 form. In the one case run on the real AES, the user register was not loaded, so AESulator may not match. **AESulator:** loads the user registers. **Switch:** `ldm_user_bank`, default on.

#### B5. Which bank an LDM of the PC with `^` writes back to

**Not known:** which bank's R13 `LDMFD R13!,{PC}^` writes back to when the mode changes. In the one case run on the real AES, SVC R13 was written back and user R13 left alone, but that run never changed mode, so it does not decide the rule. **AESulator:** writes back to user R13, as the ARM2's datasheet describes the ARM1 bug. **Switch:** `ldm_pc_wb`, default user bank.

#### B6. The register banks just after a mode change

**Not known:** which bank the instruction just after a mode change reads and writes. In the one case run on the real AES, both such reads gave what looks like the two banks' values ORed together. **AESulator:** reads its first-cycle registers from the old bank and writes to the new one; it never ORs them. **Switch:** `mode_delay`, default reads old, writes new.

#### B7. A read of a missing RAM bank

**Not known:** what a 2 MB AES reads where the card's RAM would be. **AESulator:** `&FFFFFFFF`, and a write there is lost. **Switch:** `ram_absent_bank`, default all ones.

#### B8. The read at &2000000

**Not known:** whether the real AES always reads `&00` there, as a byte and as a word. **AESulator:** returns `&FFFFFFFF`, with no abort, so the one read made on the real AES, `&00`, is a known difference. **Switch:** `region10_read`, default nothing answers.

#### B9. ROR by a register holding 64 or more

**Not tested:** multiples of 32 above 32 were measured at 64 and 96 only, with one value each; higher ones were not tried. **AESulator:** bit 31 at every multiple of 32, and 0 at every other amount above 31. **Switch:** `ror_reg_carry`, default the real AES's rule.

#### B10. RAM contents at power-on

**Not known:** what a real board's RAM holds at power-on. **AESulator:** starts with RAM cleared to 0; RAM keeps its contents through Break. **Switch:** `ram_power_on`, default zero.

### Timing

#### B11. Which of an LDM's cycles is the non-sequential one

**Not known:** where the N cycle falls; only the total is measured. **AESulator:** makes the first data cycle non-sequential, adds an internal cycle after the last word, and makes the next fetch sequential. No switch.

#### B12. Why a shift by a register costs one cycle more

**Not known:** whether the fetch after it is non-sequential or there is a second internal cycle. **AESulator:** makes the fetch after it non-sequential. **Switch:** `time_reg_shift_next`, default the non-sequential fetch.

#### B13. The refresh period and the refresh cost

**Not tested:** each figure alone. Both come from the circuit; the real AES's loops confirm only their ratio. **AESulator:** 192 ticks and 9 ticks. **Switches:** `time_refresh_period` and `time_refresh_cost`.

#### B14. The block break

**Not tested:** cycles other than data and a plain fetch, the two the real AES showed it for. **AESulator:** charges it on a sequential RAM cycle after a block's last word. **Switch:** `time_block_break`; the other choice is the older rule, a break when the cycle's own address starts a block.

#### B15. A code fetch from the ROM, and a write to the Tube

**Not tested:** neither was timed on the real AES; Part 1's costs for them come from the circuit, which leaves open whether a ROM or Tube cycle starts as a non-sequential or a sequential one. **AESulator:** the same as a load: a non-sequential cycle plus a wait of 8 cycles of the board's 6.67 MHz clock, 24 ticks (1.2 µs), for the ROM and 2, 6 ticks (0.3 µs), for the Tube, with no wait for the host's clock. **Switches:** `time_rom_wait`, `time_tube_wait` and `time_tube_sync`.

#### B16. How long the IRQ, FIQ and abort entries take

**Not known:** their cost; only the undefined-instruction entry was timed. **AESulator:** charges each as an SWI, two sequential cycles and one non-sequential, and sees an interrupt at the end of each instruction, with no extra delay. No switch.

### The Tube

#### B17. The Tube's registers above &1000000

**Not tested:** the repeats; the real AES used only `&1000000`. **Not known:** what an access between `&1800000` and `&1FFFFFF` reads, where the circuit selects nothing. **AESulator:** repeats them up to `&17FFFFF`; above that an access takes the Tube's wait but selects nothing and reads the empty value of B7. **Switch:** `tube_addr_decode`, default the lower half.

#### B18. How the host's reset holds the ARM1

**Not known:** how long the host holds the reset line. The wiring holds the ARM1 in reset for as long as it lasts; no document gives its length, and it was not tested. **AESulator:** holds the ARM1 for as long as the line is active and restarts it when the line lets go. **Switch:** `tube_prst_level`, default holds.

#### B19. The Tube chip's first read after a reset

**Not known:** what the host's first read of Tube register 3 after a reset returns. The real AES gave `&FF`, where a log of the same chip gave `&34`; every other line of that test matched the log. **AESulator:** uses Beebium's own Tube chip, so it gives whatever that chip gives. No switch.

## Part 3: the evidence

The evidence is of four kinds: what the documents say; what the real AES measured; what was worked out from the circuit diagram and the Executive's source; and what is assumed. A program run on AESulator is never evidence about a real board, and none is used here.

### (a) The documents

The short names are those of Part 4.

- **SW, the 1986 Software Reference Manual.** pp.1 and 17: the PC reads 8 ahead. p.5: the 25 registers and the FIQ bank; the modes and R15's layout. Sections 3.2 and 3.3: the vectors, their order, and address exceptions only for data addresses above 64 MB. p.12: the compare instructions' flags. p.14: bit 7 of a register shift printed 0 (B3). pp.14-15: a shift by 0 keeps the carry. p.18, "The First Devices": RRX's carry, and "0" for ROR above 31, bugs 1 and 2 (Part 1's table). p.24: LDM and STM with R15 and `^`. p.29: R14 after a data abort, "subtract 8". p.32: each instruction's cost, a failed condition 1S, the register shift's extra 1S, LDM (n+1)S (Part 1's table). p.33: S and N cycles of 3 and 6 ticks (150 and 300 ns), the crystal divided by 3 and 6. p.36: coprocessor instructions trap. An unaligned word access is "not defined" (B1).
- **SW85, the 1985 CPU Software Manual.** p.5: a mode change does not take effect immediately (B6). pp.6 and 19: coprocessor instructions trap. p.10: bit 7 of a register shift printed "x" (B3). p.12, "The First Devices": RRX's carry, and "0" for ROR above 31, bugs 1 and 2. pp.15-16: LDM and STM writeback, and `^`. p.21: LDM (n+1)S + 1N, and the extra 1S of an LDR or STR offset shifted by a register. An unaligned word access is "not defined" (B1).
- **HW, the Hardware Reference Manual and its circuit diagram.** pp.1-13: the five instruction classes; p.13: exception priority. p.23: 2 MB on the board and 2 MB on a card. p.34: the 27128 ROM, one byte wide, and the wait signal that stretches a cycle. p.35: the boot latch, whose equation has no address term, so any write clears it. p.36: five clocks from one RAM access to the next through a refresh. Section 5.1.2: the RAM bank select by A20 and A21, and the refresh request. Section 5.1.3: the divide by three. Section 5.1.4: the clock logic, where only RAM has a fast sequential path, and the block break. Section 5.1.5: the ROM and Tube selects. Section 5.3: the Tube's wiring. The circuit diagram: the 20 MHz crystal, which a photograph of a real AES also shows; the abort pin wired to 0 V (no aborts); the refresh counter; the reset wiring. The parts list: no memory manager.
- **DS87, the ARM2 datasheet.** Section 13.2: the ARM1 bugs: RRX's carry, ROR by 32n, the false abort on an LDM of one register, the `LDM ...^` writeback to the wrong bank ("most probably the user R13", B5), and the abort during an internal cycle. Section 13.3: the ARM2's changes: MUL and MLA, the coprocessor interface, undefined instructions with a failing condition now skipped, the register-shifted LDR and STR removed, two extra FIQ registers, LDM of the user registers added (B4), and in its register description the FIQ bank as R8_fiq to R14_fiq, and some cycle control signals changed (B12). Section 5.3.1: a one-cycle delay before an interrupt is seen, on the ARM2 (B16). Chapter 9: the ARM2's entry cycles (B16) and its LDM cycle shape (B11).
- **Wilson 1988.** The first ARM made on VTI's 3 micron CMOS process; the AES's ARM run at 3.3 MHz and 6.6 MHz, the crystal divided by 6 and 3.
- **ClockSp posts.** Two forum posts of ClockSp 2.01 on other AESes: 35.68, in a photograph of the screen, and 35.73, in a table of results.

### (b) The real-AES testing

One real AES was tested on 15 November 2026: 4 MB fitted, Executive Brazil -.005, with a BBC Master 128 as its host. Its programs are named by test number. The ROM was read from the board.

- **Test 2, timed loops.** MOV, a failed condition, B, LDR, STR and STM loops took the times SW p.32 gives, to a centisecond, with refresh counted. The register-shift loop took 9 ticks an instruction, not 6 (Part 1's table; B12). LDM of 4, 8 and 12 registers took 3n + 9 ticks (Part 1's table; B11). With 9 ticks a refresh every 192, every loop fits (B13).
- **Test 3, memory timing.** RAM loads as test 2. A ROM load loop took 76 cs, and a Tube load loop 38 cs, where each would take 25 cs if its loads cost a RAM load; both fit the ROM and Tube waits to within half a tick, and the Tube load fits no wait for the host's clock (ROM and Tube timing; B15). The loops ran in RAM, so no code was fetched from the ROM, and nothing was written to the Tube. The Tube's registers answered at `&1000000` (B17).
- **Test 4, the memory map.** RAM aliased at 4, 8 and 12 MB but not at 1 or 2 MB; the ROM read the same in every 16 KB block; no probe aborted (the memory map; no aborts).
- **Test 4b, the read at `&2000000`.** A byte load read `&00`, with no abort (the memory map; B8). It was not repeated as a word.
- **Test 5, the shifter.** Q1: RRX's carry is bit 0 OR bit 31. Q2: a shift by 0 keeps the carry; ROR's carry is 0 at 31, 33, 40 and 255, and 1 at 32, 64 and 96, each of 64 and 96 with one value only (Part 1's table; B9). Q4: the register-shifted LDR ran, one S cycle more than a plain LDR. Part 1: instructions with bit 7 set did not trap; what each did is recorded but not yet read against AESulator (B3). Part 2: the compare instructions without S did not trap, wrote no register and kept the flags.
- **Test 7, address exceptions.** R14 held the instruction plus 8; LDM and STM checked only their first address, later words wrapping; the address checked was the one used; a faulting LDR or STR left its base alone (loads and stores; exceptions). The data-abort case cannot be tried on an AES, which has no aborts.
- **Test 8, undefined instructions.** Coprocessor instructions trapped, including one whose condition failed (exceptions).
- **Test 9, data-processing checksums.** Its checksums over six values and both carries fit only the ROR rule of Part 1 (Part 1's table).
- **Test 10, R15.** R15 read 12 ahead after a shift by a register, as Rn or Rm, and STR and STM stored it 12 ahead (the pipeline).
- **Test 11, modes and banks.** It stopped early, but its register dump keeps two reads made just after mode changes: each matches the OR of the old and new banks' R13, taking IRQ R13 as the Executive sets it (B6), and the user register an `LDM ...^` should have loaded still held its old value (B4). Q6: `LDMFD R13!,{PC}^` wrote back to SVC R13 and left user R13 alone; the word it loaded kept SVC mode (B5).
- **Test 13, RAM placement.** The same loop cost the same in all four RAM banks, the card's included. An LDM across a 32-word block cost 3.1 ticks more than one inside it (RAM timing; B14).
- **Test 15, the start-up snapshot.** RAM saved at start-up; recorded, not yet read (B10).
- **Test 17, the block break and fetches.** A loop whose fetch follows an internal cycle at a block's last word cost the same as one inside a block, and a plain loop across the block cost more: the block-break rule of Part 1 (RAM timing; B14).
- **Test 18, the undefined-instruction entry.** The trap cost 20.3 ticks more than a call, against 21 for two S cycles and one N (B16). The other entries were not timed.
- **Test 19, the Tube.** The flags, the IRQ wiring and the resets were as predicted for the same Tube chip as the other second processors (the Tube link). Register 3 matched a log of the same chip in 147 of 148 lines; the first host read after a reset gave `&FF`, where the log gave `&34` (B19).
- **ClockSp and Tak.** ClockSp 2.01 gave a Combined Average of 35.71, and ClockSp 2.10 an Unweighted Average of 32.96 (how fast it is).
- The FIQ bank was not tested.

**The ClockSp arithmetic.** The two forum posts' 35.68 and 35.73 have a mean of 35.705; with the real AES's 35.71, the three agree to 0.1%. AESulator scores 35.83, about 0.35% above that mean. Worked back from the printed scores, ClockSp's eight sub-test times add up to 1737 cs on the real AES and 1727 cs on AESulator, about 0.6% fast. The score adds rates, so a short sub-test counts for more than a long one.

### (c) Worked out from the circuit and the Executive

From the circuit diagram and its logic equations:

- **The memory map.** A25 and A24 pick the region; A20 and A21 pick the RAM bank, so RAM repeats every 4 MB; A2 to A4 pick the Tube register, so the registers repeat every 32 bytes, and the Tube's select covers A23 low only, up to `&17FFFFF` (B17); nothing is selected at `&2000000` (B8).
- **A missing RAM bank.** The main board drives all four RAM bank selects; its own RAM uses two, and the card answers the other two, so with no card nothing answers (B7).
- **The Tube's data.** The Tube is on the low eight data lines, so a word read leaves the other three bytes undriven (B2).
- **Refresh.** The refresh counter is clocked at the crystal divided by 3 and asks every 64 of its clocks: 192 ticks. Its equation on the circuit diagram and the one printed in HW section 5.1.2 differ (B13). HW p.36's five clocks through a refresh, three more than an N cycle, give 9 ticks.
- **The block break.** The clock chip's equations, HW section 5.1.4, read literally: at its clock the address lines still show the cycle that is ending (B14).
- **The waits.** The wait-state equations give 8 board clocks, 24 ticks (1.2 µs), for the ROM and 2, 6 ticks (0.3 µs), for the Tube, a board clock being the crystal divided by 3; only RAM has a fast sequential path (B15).
- **The host's clock.** HW section 5.3: the host's clock goes only to the Tube chip, so none reaches the ARM's side (B15).
- **The Tube's interrupts and reset.** PIRQ wired to IRQ and PNMI to FIQ; the host's reset line wired through an inverter to the ARM's reset pin, a level, not a pulse (the Tube link; B18).

From the Executive's source (Kernel, Middle, Super1) and the ROM's own bytes:

- **Start-up.** RAM sized from `&1000` upwards in 1 KB steps, then the banner and prompt. The banners, versions and dates are read from each image's own bytes (the Executive).
- **Mode changes.** The instructions after a mode change write the new bank, and a no-op stands before any read (B6). The Executive sets IRQ R13 at start-up, the value used in reading test 11.
- **The Tube.** The Executive uses IRQ and FIQ as the circuit wires them, and makes only byte accesses to the Tube (the Tube link; B2).

From AESulator's own code (the `abort` setting, Part 2): an LDM of one register makes one access, to its own address, and an internal cycle makes no memory access, so neither abort bug can be triggered. `strict` is AESulator's own rule and copies no hardware.

### (d) What is assumed

- Every ARM1 has the same quirks, so a difference between the 1985 and 1986 manuals is a difference between documents, not between chips.
- The Tube chip is the same part, and behaves the same, as in the other second processors: photographs of an AES and of a 6502 second processor show the same part, and the host uses the same Tube code for all of them (the Tube link; B19).
- A ROM or Tube cycle is a non-sequential cycle plus its wait, and a code fetch from the ROM or a write to the Tube costs the same as a load (B15).
- The host's reset holds the ARM1 for as long as it lasts; no document gives its length (B18).
- The B items' defaults where no measurement or document decides: B1 the low bits ignored, a stand-in; B2 and B7 `&FF` for an undriven byte; B4 and B5 the ARM2 datasheet's account of the ARM1; B6 old reads, new writes; B9 every multiple of 32 as 32; B10 RAM cleared; B11 the LDM's shape; B12 the non-sequential fetch, which DS87 section 13.3's changed cycle signals allow; B13 the two figures as worked out; B16 the entries as an SWI, with no interrupt delay; B17 the empty value above `&17FFFFF`.

## Part 4: sources

| Short name | Title | Part number | Issue or date | Where it is online |
|---|---|---|---|---|
| SW | Acorn, ARM Software Reference Manual | 0448,007 | 1986 | <https://chrisacorns.computinghistory.org.uk/docs/Acorn/OEM/OEM.html> (`AcornOEM_ARMSoftwareRM.pdf`) |
| SW85 | Acorn, ARM CPU Software Manual Version 1.00 | not recorded | Issue 1.00, October 1985 | <https://gtoal.com/acorn/arm/> (`ARM_CPU_Software_Manual_Version_1.0.pdf`) |
| HW | Acorn, ARM Hardware Reference Manual, with the circuit diagram | 0448,006 | Issue 1.0, 4 August 1986 | <https://chrisacorns.computinghistory.org.uk/docs/Acorn/OEM/OEM.html> (`AcornOEM_ARMHardwareRM.pdf`); the clearer circuit diagram is in the copy at <https://acorn.huininga.nl/pub/docs/manuals/Acorn/ARM%20Evaluation%20System/> (`ARM Evaluation System - ARM Hardware Reference Manual.pdf`) |
| DS87 | Acorn, ARM datasheet (the ARM2), chapter 13 "Compatibility with Prototype ARMs" | 1 85250 0360 0 | Issue 1.0, 17 March 1987 | <https://home.marutan.net/arcemdocs/> |
| Wilson 1988 | Sophie Wilson, "Some facts about the Acorn RISC Machine", comp.arch | none | 2 November 1988 | <https://groups.google.com/g/comp.arch/c/hPsDLEPf2eo/m/nvJR_d7nnyYJ> |
| The Executive | The Executive's source (Kernel, Middle, Super1) and the ROM's own bytes | none | Executive 1.00 | Source: <https://gtoal.com/acorn/arm/Kernel/>. ROM: <https://acorn.huininga.nl/pub/roms/Acorn/ARM%20Evaluation%20Kit%20Second%20Processor/> (`ARM Evaluation System 1.01.rom`) |
| ClockSp posts | Two forum posts of ClockSp 2.01 on real AESes: a photograph of the screen (35.68), and a table of results (35.73) | none | none recorded | <https://stardot.org.uk/forums/viewtopic.php?p=128579#p128579>; <https://stardot.org.uk/forums/viewtopic.php?p=147162#p147162> |
