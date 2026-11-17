# Building AESulator

AESulator is a plugin for Beebium, the BBC Micro emulator. It is built inside a copy of Beebium's source, version v0.15.0 (commit `1c7bd7f`), and is tested with that version only. It has been built and tested on an Intel Mac (macOS 15.8.3) and an Apple M2 Mac (macOS 27.2). Compiling AESulator under Linux hasn't been tested.

## 1. Get Beebium

Clone Beebium and check out v0.15.0. At that version Beebium has no submodules.

```
git clone https://github.com/rob-smallshire/beebium.git
git -C beebium checkout v0.15.0
```

## 2. Get a ROM

AESulator needs the AES's Executive ROM, a 16 KB image. It is not included: you supply your own. Where to look: "Where to find things" in `docs/user-guide.md`.

## 3. The folders

Three things, each wherever you like:

```
beebium/              Beebium's source, v0.15.0: BEEBIUM_DIR
    build/            made in step 6: Beebium's build, the servers, the test environment
AESulator/            this package, the folder holding this file
    aes-coprocessor/  the plugin
    tools/            the build scripts
    aes-paths.sh      your paths, if you use the file (step 4)
your ROM file         the Executive ROM: AES_ROM
```

## 4. Say where things are

Either set the two paths in the terminal, each time you open a terminal:

```
export BEEBIUM_DIR="/path/to/beebium"
export AES_ROM="/path/to/your/rom"
```

Or write them once in a file. Copy the template, then open `aes-paths.sh` and follow the instructions inside it. The scripts read `aes-paths.sh` themselves. In this package's folder:

```
cp aes-paths.template.sh aes-paths.sh
```

A path set with `export` is used ahead of the one in the file.

## 5. Install Beebium's prerequisites

Install what Beebium's own guide lists as prerequisites: [docs/building.md at v0.15.0](https://github.com/rob-smallshire/beebium/blob/v0.15.0/docs/building.md). Skip its build commands: step 6 does the one build, and a second build made another way can clash with it.

Also install [uv](https://docs.astral.sh/uv/getting-started/installation/), which the server tests in step 7 need. With Homebrew: `brew install uv`.

Beebium's list includes Xcode 16 and XcodeGen, which we took to be for its Mac app. The server, which is all this builds, built here with Xcode's Command Line Tools alone.

If your libraries are not where Homebrew puts them, set `CMAKE_PREFIX_PATH` to their folder before step 6: with `export` in the terminal, or as a line `CMAKE_PREFIX_PATH="/path/to/them"` in `aes-paths.sh`. If both are set, the file's is used.

## 6. Add the plugin and build

From this package's folder:

```
tools/plugin-build.sh
```

It copies `aes-coprocessor/` into Beebium's tree as `src/extensions/aes-coprocessor/`. It adds one line to each of two of Beebium's build files, `src/extensions/CMakeLists.txt` and `src/server/CMakeLists.txt`. Then it builds Beebium's servers with the plugin. It writes nothing outside Beebium's folder.

A fresh build took about 10 minutes on the Intel Mac and about 2½ minutes on the M2 Mac. The script compiles 4 files at a time unless `AES_BUILD_JOBS` says otherwise. It uses Ninja if Ninja is installed, and CMake's default otherwise.

## 7. Test

From this package's folder, first the plugin's own tests:

```
aes-coprocessor/tests/run-tests.sh
```

They take about a minute and print almost nothing until a summary line. With the tested ROM (SHA-1 starting `f86bbc48`) it is `257 tests, 0 failed, 0 skipped`. A ROM with no facts file skips the tests that need one, and each `SKIPPED` line says why.

Then the tests in the built server:

```
aes-coprocessor/tests/run-server-tests.sh
```

These need [uv](https://docs.astral.sh/uv/getting-started/installation/), which fetches the right Python and the test packages itself. uv keeps its downloaded Python and its cache in your home folder, and the test environment in Beebium's build folder. With the tested ROM they take about 12 minutes, because the tests tied to the ROM run too, and end `87 passed, 16 skipped`. With a ROM that has no facts file they take about a minute.

## 8. Run

```
$BEEBIUM_DIR/build/src/server/beebium-model-b --fdc acorn-1770 --sideways "slot=14:type=rom:image=acorn-dfs_2_26.rom" --tube-aes "rom=$AES_ROM"
```

`$BEEBIUM_DIR` and `$AES_ROM` exist only in a terminal where you set them with `export` (step 4). The scripts read `aes-paths.sh` themselves, but your terminal does not. If you have not set them, use this form, from Beebium's folder, with your own full paths:

```
cd /path/to/beebium
build/src/server/beebium-model-b --fdc acorn-1770 --sideways "slot=14:type=rom:image=acorn-dfs_2_26.rom" --tube-aes "rom=/path/to/your/rom"
```

The screen (step 9) shows `ARM Second Processor 4096K` (some ROM versions begin it with `Acorn `), `Acorn 1770 DFS` and `BASIC`, then the `A*` prompt.

Keep the quotes, and put your ROM's path after `rom=`. Always give `rom=`: without it the server stops with "missing required parameter 'rom'". The other settings and a fuller example are in `docs/user-guide.md`. Each of Beebium's `beebium-model-*` servers offers `--tube-aes`; the tests use `beebium-model-b`.

The server keeps running; leave this terminal open while you do step 9. To stop it, press Ctrl+C in this terminal, as Beebium's [command-line guide](https://github.com/rob-smallshire/beebium/blob/v0.15.0/docs/cli.md) says.

## 9. See the screen

The server has no window of its own. To see the BBC Micro's screen, start Beebium's macOS app (how to get it: [Beebium's README at v0.15.0](https://github.com/rob-smallshire/beebium/blob/v0.15.0/README.md)), choose File > Connect…, and press Connect. The address it offers is the server's own default.

## If it goes wrong

- "Permission denied" when you start a script: the scripts have lost their executable mark on the way. From this package's folder, run `chmod +x tools/*.sh aes-coprocessor/tests/*.sh`.
- "BEEBIUM_DIR is not set": set it, as in step 4.
- "No Beebium at BEEBIUM_DIR": the folder has moved or was never made. Check the path.
- "The plugin is not in Beebium's tree": run step 6 before the server tests.
- "uv: command not found", or "The server tests need uv": install uv, as in step 5.
- The tests seem to hang: they print nothing while they run. Give them the times in step 7.
- Many `SKIPPED` lines: `AES_ROM` is unset, or your ROM has no facts file in `aes-coprocessor/tests/rom-facts/`.
- Beebium itself will not build: see the troubleshooting section of its guide.
