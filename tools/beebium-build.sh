#!/bin/sh
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
# beebium-build.sh, configures and builds Beebium
# version 2.0.0, 20 November 2026.

# Configures and builds Beebium. Paths: tools/paths.sh. Libraries kept in a folder of their own,
# outside the system's, are named by CMAKE_PREFIX_PATH.
# Use: tools/beebium-build.sh [--copy-from <Beebium folder>]   Stops at the first error.
# --copy-from makes BEEBIUM_DIR as a working copy, leaving out oracle/ (never read, never built)
# and .git. Without it, a missing BEEBIUM_DIR is an error: Beebium may have moved.
# Builds Beebium's servers and plugins (target beebium-servers), plus the two things only the
# server tests use: the plugin with the Tube log, and test_aes_machine.
# Generator: an existing build folder keeps its own; a new one uses Ninja if it is installed,
# otherwise CMake's default. The compiler is CMake's choice, or CC and CXX if set.
# AES_BUILD_JOBS compiles that many files at a time, 4 if unset. Under Ninja, Beebium's
# beebium_heavy_compile pool keeps its largest compiles to one at a time.
# -L to that folder's lib makes our binaries use its libc++, the one its libraries use.

set -e
here="$(cd "$(dirname "$0")" && pwd)"
AES_SOURCE="$(cd "$here/.." && pwd)"
. "$here/paths.sh"

if [ "$1" = --copy-from ]; then
    if [ -e "$BEEBIUM_DIR" ]; then
        echo "BEEBIUM_DIR already exists: $BEEBIUM_DIR" >&2
        exit 1
    fi
    mkdir -p "$BEEBIUM_DIR"
    rsync -a --exclude=/oracle --exclude=/.git "$2/" "$BEEBIUM_DIR/"
fi
need_beebium

E="$CMAKE_PREFIX_PATH"
if [ -n "$E" ]; then
    set -- -DCMAKE_PREFIX_PATH="$E" -DOPENSSL_ROOT_DIR="$E" -DCMAKE_EXE_LINKER_FLAGS="-L$E/lib" \
        -DCMAKE_SHARED_LINKER_FLAGS="-L$E/lib" -DCMAKE_MODULE_LINKER_FLAGS="-L$E/lib" \
        -DCMAKE_BUILD_RPATH="$E/lib"
else
    set --
fi
if [ ! -f "$BEEBIUM_BUILD/CMakeCache.txt" ] && command -v ninja >/dev/null; then
    set -- -G Ninja "$@"
fi

cmake -S "$BEEBIUM_DIR" -B "$BEEBIUM_BUILD" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$BEEBIUM_BUILD" --parallel "${AES_BUILD_JOBS:-4}" \
    --target beebium-servers beebium_ext_aes_log_plugin test_aes_machine
