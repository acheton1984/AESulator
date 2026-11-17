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
# run_check.py, checks a run's text for its end line and Executive messages
# version 1.0.0, 17 November 2026.

"""The one check the run scripts share (OI-12). A run that
stops with an Executive message falls back to the prompt, so the prompt is no proof: the text
must hold the run's end line and no Executive message, unless that test may end in one.
The messages are the ROM facts file's (SRC, ROM bytes), so no ROM's text is written here.
Use from a run script, or on a folder of saved run-frozen.py logs:
tools/run-py.sh tools/run_check.py <folder>   It exits non-zero if anything is wrong.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "aes-coprocessor" / "tests"))

from aes_host import fact  # noqa: E402

MESSAGES = ["text.undefined", "text.data_abort", "text.prefetch_abort", "text.address_exception",
            "text.break_point", "text.irq_while"]
# The run sheet's "message expected" tests (USER): test 4b, the read of region 10,
# and Q4, the register-shifted LDR. Either may end in a message instead of its end line.
MAY_END_IN_A_MESSAGE = {"OUT04Y", "OUT05Q4"}


def problems(text, end_line=None, may_end_in_a_message=False):
    """What is wrong with one run's text: a list, empty when all is well."""
    found = [fact(key) for key in MESSAGES if fact(key) in text]
    wrong = []
    if found and not may_end_in_a_message:
        wrong.append("Executive message: " + ", ".join(repr(message.strip()) for message in found))
    if end_line and end_line not in text and not (found and may_end_in_a_message):
        wrong.append(f"no {end_line!r} line")
    return wrong


def frozen_log(name, text):
    """A frozen test's spool log, OUTnn...: it ends with Tnn DONE."""
    number = re.match(r"OUT(\d\d)", name)
    end_line = f"T{number.group(1)} DONE" if number else None
    return problems(text, end_line, name in MAY_END_IN_A_MESSAGE)


def finish(found):
    """found: (what, problems) pairs. Prints each problem and exits non-zero if there is any."""
    bad = [(what, wrong) for what, wrong in found if wrong]
    for what, wrong in bad:
        print(f"PROBLEM {what}: {'; '.join(wrong)}", flush=True)
    print(f"run check: {len(found) - len(bad)} of {len(found)} as expected", flush=True)
    if bad:
        sys.exit(1)


def main():
    found = []
    for path in sorted(Path(sys.argv[1]).glob("*-OUT*.txt")):
        name = path.stem.split("-", 1)[1]
        found.append((path.name, frozen_log(name, path.read_bytes().decode("latin-1"))))
    finish(found)


if __name__ == "__main__":
    main()
