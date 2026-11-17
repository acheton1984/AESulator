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
# run-frozen.py, runs every program on the frozen test discs
# version 1.0.0, 17 November 2026.

"""Runs every program on the three frozen test discs, each on a fresh boot and its own copy, and
writes its spool log and a summary to a folder. The test: each program ends and
its last line appears. tools/run_check.py then checks each log for its Tnn DONE line and no
Executive message, and the run exits non-zero if one fails (OI-12). The logs are emulator output, never expected values. AESCHECK is left out:
its RUN04Y hangs on purpose (notes/acceptance-suite.md).
Use: tools/run-py.sh tools/run-frozen.py <output folder> [program ...]. With AES_LOG_FOLDER set,
each run goes through the counting wrapper, with its log in that folder.
"""
import collections
import os
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "aes-coprocessor" / "tests"))

import dfs_image  # noqa: E402
from aes_host import Aes, calls_on_r2, copy_frozen, frozen_unchanged  # noqa: E402
from run_check import finish, frozen_log  # noqa: E402

DISCS = {
    "AESTESTS": ["SNAP", "RUN12", "RUN01", "RUN02", "RUN09", "RUN05Q1", "RUN05Q4", "RUN05Q2", "RUN07"],
    "AESTIER2": ["RUN03", "RUN13", "RUN06", "RUN10", "RUN04P1", "RUN08", "RUN11"],
    "AESTIER3": ["RUN11Q6", "RUN05P1", "RUN05P2", "RUN04N", "RUN04Y"],
    "AESTIER4": ["RUN17", "RUN18", "RUN19"],
}
EXEC_HANDLE = 0x0256  # INF, MOS 1.20's copy of the *EXEC handle, as OSBYTE &C6 reads it.
LIMIT_SECONDS = 900
NAMES = {0: "RDCH", 2: "CLI", 4: "BYTE-", 6: "BYTE+", 8: "WORD", 0x0C: "ARGS", 0x0E: "BGET", 0x10: "BPUT",
         0x12: "FIND", 0x14: "FILE", 0x16: "GBPB"}


def run(disc_name, run_file, out_folder):
    with tempfile.TemporaryDirectory() as folder:
        disc = copy_frozen(disc_name, folder)
        before = dfs_image.catalogue(disc.read_bytes())
        if f"$.{run_file}" not in before:
            return f"{disc_name} {run_file}: no such file\n", [(f"{disc_name} {run_file}", ["no such file"])]
        log = os.environ.get("AES_LOG_FOLDER")
        with Aes(disc=disc, tube_log=Path(log) / f"{run_file}.log" if log else None) as aes:
            aes.boot()
            aes.wire.clear()
            aes.forget_text()
            aes.type(f"*EXEC {run_file}\r")
            host = aes.bbc.memory.address.peek

            def done():
                return aes.idle() and host[EXEC_HANDLE] == 0 and aes.at_prompt()

            ended = aes.run(done, LIMIT_SECONDS, chunk=0.05)
            aes.settle()
            # This tool reports a gap in its summary, so it takes the count before reading.
            lost, aes.wire.lost = aes.wire.lost, 0
            r2 = aes.wire.parasite_wrote(2)
            r4 = aes.wire.host_wrote(4)
            screen = aes.screen()
        after = dfs_image.catalogue(disc.read_bytes())
    logs = {name: data for name, (_, _, data) in after.items()
            if name not in before or before[name][2] != data}
    for name, data in logs.items():
        (out_folder / f"{disc_name}-{name.replace('$.', '').replace('.', '_')}.txt").write_bytes(data)
    try:
        calls = collections.Counter(NAMES[call[0]] for call in calls_on_r2(r2))
    except (AssertionError, ValueError, IndexError):
        calls = {"unparsed (some trace lost)": len(r2)}
    last = b""
    for data in logs.values():
        rows = [row for row in data.replace(b"\r", b"\n").split(b"\n") if row.strip()]
        rows = [row for row in rows if not row.endswith(b"*SPOOL")]
        last = rows[-1] if rows else b""
    checks = [(f"{disc_name} {run_file}", [] if ended else ["did not end"])]
    checks += [(f"{disc_name} {name}", frozen_log(name.replace("$.", ""), data.decode("latin-1")))
               for name, data in logs.items() if name.startswith("$.OUT")]
    return (f"{disc_name} {run_file}: ended={ended} lost={lost} logs={sorted(logs)} calls={dict(calls)} "
            f"R4 bytes={len(r4)} last={last.decode('latin-1')!r}\n"
            + ("" if ended else "  screen at the limit:\n" + screen + "\n")), checks


def main():
    out_folder = Path(sys.argv[1])
    out_folder.mkdir(parents=True, exist_ok=True)
    wanted = sys.argv[2:]
    found = []
    with open(out_folder / "summary.txt", "a") as summary:
        for disc_name, files in DISCS.items():
            for run_file in files:
                if wanted and run_file not in wanted:
                    continue
                line, checks = run(disc_name, run_file, out_folder)
                found += checks
                print(line, end="", flush=True)
                summary.write(line)
                summary.flush()
            assert frozen_unchanged(disc_name), f"{disc_name} changed"
    finish(found)


main()
