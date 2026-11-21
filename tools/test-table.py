#!/usr/bin/env python3
# Copyright 2026 James Sargent <acheton1984@letterboxes.org>
#
# This file is part of AESulator, a plugin for Beebium.
#
# AESulator is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version. AESulator is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
# You should have received a copy of the GNU General Public License along with AESulator.
# If not, see <https://www.gnu.org/licenses/>.

# AESulator: the Acorn ARM Evaluation System, a Beebium plugin.
# test-table.py, writes the table of tests
# version 2.0.0, 20 November 2026.

# Writes scratch/plugin-tests.md (not committed), the table of the plugin's tests, from the tests themselves.
# run-tests.sh runs it after a passing run, so nobody edits the table by hand.

import glob, os, pathlib, re

project = pathlib.Path(__file__).resolve().parent.parent
what = {
    "test_a_defaults.cpp": "Built-in defaults within their limits; runs first",
    "test_data.cpp": "Data processing, shifts, conditions",
    "test_memory.cpp": "LDR, STR, LDM, STM",
    "test_control.cpp": "Branches, register banks, mode changes, reset, pipeline",
    "test_exceptions.cpp": "SWI, undefined, IRQ, FIQ, address exceptions, aborts",
    "test_cycles.cpp": "Cycle counts, the manuals' example programs",
    "test_aes_memory.cpp": "The memory map and boot latch",
    "test_aes_tube_abort.cpp": "Tube addresses, ABORT, the map's timing",
    "test_aes_timing.cpp": "Board time",
    "test_aes_board.cpp": "One small program, whole board",
    "test_aes_exceptions.cpp": "Exceptions on the whole board, own handlers",
    "test_aes_options.cpp": "Settings against the manifest",
    "test_aes_tube.cpp": "The ARM's side of the Tube, driven as Beebium drives it",
    "test_aes_banner.cpp": "The banner through the Tube chip, tied to the ROM's SHA-1",
    "test_aes_rom_facts.cpp": "The ROM facts files, against the ROM",
    "test_aes_debug.cpp": "The debugger's registers, memory, steps and resets, as Beebium's debugger sees them",
    "test_aes_debug_stops.cpp": "The debugger's breakpoints and watchpoints",
    "test_aes_tube_log.cpp": "The counting wrapper, a test-build tool",
    "test_z_executive.cpp": "Exceptions through the real Executive, no Tube",
}
sections, total = [], 0
for name, heading in what.items():
    text = open(project / "acorn-aes-coprocessor/tests" / name, encoding="utf8").read()
    tests = re.findall(r'^TEST\((\w+), "((?:[^"\\]|\\.)*)"\)', text, re.M)
    total += len(tests)
    rows = ["| Test | What it checks | Labels |", "|---|---|---|"]
    for test, title in tests:
        labels = ", ".join(l for l in ("DOC", "SRC", "INF", "USER", "UNK") if re.search(rf"\b{l}\b", title))
        labels = labels or ("DS87, adapted" if "DS87" in title else "Design")
        rows.append(f"| `{test}` | {title.replace('|', '/')} | {labels} |")
    sections.append(f"## {heading} (`{name}`, {len(tests)})\n\n" + "\n".join(rows))
found = sum(len(re.findall(r"^TEST\(", open(f, encoding="utf8").read(), re.M))
            for f in glob.glob(str(project / "acorn-aes-coprocessor/tests/test_*.cpp")))
if found != total:
    raise SystemExit(f"test-table: {found} tests in the files, {total} in the table: add the new file")
head = (f"# Plugin tests\n\nThe tests in `acorn-aes-coprocessor/tests/`, {total} in all. Written by `tools/test-table.py` "
        "after each passing `run-tests.sh`: do not edit by hand. The real-AES test map is in "
        "the old repository (tag `6c233f0`).\n\n")
(project / "scratch").mkdir(exist_ok=True)
open(project / "scratch/plugin-tests.md", "w", encoding="utf8").write(head + "\n\n".join(sections) + "\n")
