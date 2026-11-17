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
# fault-check.py, puts each known fault in a copy and checks the tests notice
# version 1.0.0, 17 November 2026.

# Puts each known fault into a copy of the plugin code and checks that the tests notice.
# Run from the project folder: python3 -u tools/fault-check.py | tee results/fault-check.log
# Give fault numbers to run only those: python3 -u tools/fault-check.py 99 100
# Works in a scratch folder, never on aes-coprocessor/. Takes about 20 minutes in full. Paths:
# tools/paths.sh. The ROM must be there and the run is strict: a skipped test catches nothing.
import glob, hashlib, importlib.util, os, pathlib, shutil, subprocess, sys, time

project = pathlib.Path(__file__).resolve().parent.parent
settings = subprocess.check_output(["sh", "-c", '. "$AES_SOURCE/tools/paths.sh" && env -0'],
                                   env={**os.environ, "AES_SOURCE": str(project)})
os.environ.update(line.split("=", 1) for line in settings.decode().split("\0") if "=" in line)
if not os.path.isfile(os.environ.get("AES_ROM", "")):
    sys.exit("The fault check needs the ROM: set AES_ROM in aes-paths.sh.")
os.environ["AES_REQUIRE_ALL"] = "1"
work = pathlib.Path(os.environ["AES_SCRATCH"]) / "fault-check"
beebium = pathlib.Path(os.environ["BEEBIUM_DIR"]) / "src/core"
flags = ["-std=c++20", "-O1", "-fsanitize=bounds", "-fno-sanitize-recover=bounds", f"-I{beebium}/include",
         "-Ibeebium", f'-DAES_ROM_FACTS_DIR="{work}/tests/rom-facts"']

spec = importlib.util.spec_from_file_location("fault_list", project / "tools/fault-list.py")
faults = importlib.util.module_from_spec(spec)
spec.loader.exec_module(faults)

# The VM's own disk: the compiler can misread a freshly edited file on the shared folder.
shutil.rmtree(work, ignore_errors=True)
(work / "tests").mkdir(parents=True)
# plugin_entry.cpp needs Beebium's extension API, which only Beebium's build links (SRC, aes-coprocessor/CMakeLists.txt).
for f in glob.glob(str(project / "aes-coprocessor/*.[ch]pp")):
    if not f.endswith("plugin_entry.cpp"):
        shutil.copy(f, work)
for f in glob.glob(str(project / "aes-coprocessor/tests/*.[ch]pp")):
    shutil.copy(f, work / "tests")
shutil.copytree(project / "aes-coprocessor/tests/rom-facts", work / "tests/rom-facts")
(work / "beebium").mkdir()
for f in glob.glob(str(project / "aes-coprocessor/beebium/AesTube.*")) + [str(project / "aes-coprocessor/beebium/TubeLog.hpp")]:
    shutil.copy(f, work / "beebium")
shutil.copy(project / "aes-coprocessor/manifest.json", work)
os.environ["AES_PLUGIN_DIR"] = str(work)
os.chdir(work)

sources = {f: open(f).read() for f in glob.glob("*.cpp") + ["beebium/AesTube.cpp"]}
for f in sorted(sources):
    print("sha1", hashlib.sha1(sources[f].encode()).hexdigest(), f)
print("faults", len(faults.F), flush=True)
for t in glob.glob("tests/test_*.cpp") + list(sources):
    subprocess.check_call(["clang++", *flags, "-c", t, "-o", t[:-4] + ".o"])
# Beebium's Tube chip is apparatus, linked and never read: it takes no faults.
subprocess.check_call(["clang++", *flags, "-c", str(beebium / "src/TubeUla.cpp"), "-o", "tests/tube_ula.o"])
test_objects = sorted(glob.glob("tests/*.o"))  # In name order: the run-last test needs it.


def build_and_run(file, text):
    open("faulty/m.cpp", "w").write(text)
    others = [f[:-4] + ".o" for f in sources if f != file]
    built = subprocess.run(["clang++", *flags, "-I.", "faulty/m.cpp", *others, *test_objects, "-o", "m"],
                           capture_output=True, text=True)
    if built.returncode:
        return "compile", [], built.stderr[:300]
    try:
        ran = subprocess.run(["./m"], capture_output=True, text=True, errors="replace", timeout=60)
    except subprocess.TimeoutExpired:
        return "hang", [], ""
    failed = [l[8:] for l in ran.stdout.splitlines() if l.startswith("FAILED")]
    last = (ran.stdout.splitlines() or [""])[-1]
    return ("pass" if ran.returncode == 0 else "fail"), failed, last


# If the unfaulted core fails, every fault would look caught.
os.mkdir("faulty")
state, _, last = build_and_run("ArmCore.cpp", sources["ArmCore.cpp"])
print("BASELINE", state, last, flush=True)
if state != "pass":
    sys.exit("The unfaulted core does not pass, so the check means nothing.")

caught, missed, not_run = 0, [], []
chosen = [int(n) for n in sys.argv[1:]]
for number, fault in enumerate(faults.F, 1):
    if chosen and number not in chosen:
        continue
    file, name, old, new = fault if len(fault) == 4 else ("ArmCore.cpp", *fault)
    if old not in sources[file]:
        print(number, "NOT FOUND", name, flush=True)
        not_run.append(number)
        continue
    state, failed, info = build_and_run(file, sources[file].replace(old, new, 1))
    if state == "compile":
        print(number, "COMPILE FAIL", name, info, flush=True)
        not_run.append(number)
        continue
    if state == "pass":
        missed.append(number)
    else:
        caught += 1
    word = "MISSED" if state == "pass" else "caught"
    print(f"{number:2d} {word} {state:4s} {len(failed):2d}  {name}  [{'; '.join(failed[:3])}]", flush=True)
print("TOTAL", len(faults.F), "caught", caught, "missed", missed, "not run", not_run, flush=True)
