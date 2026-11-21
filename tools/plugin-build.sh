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
# plugin-build.sh, copies the plugin into Beebium's tree and builds it
# version 2.0.0, 20 November 2026.

# Copies acorn-aes-coprocessor/ one way into Beebium's tree and builds it there. The tree gets its
# only two edits here (DOC, Beebium guide section 7); nobody edits it by hand. Beebium itself
# stays at v0.20.1. Use: tools/plugin-build.sh   Paths: tools/paths.sh.
# AES_PLUGIN_SRC names another source folder, for a run against an empty stub.

set -e
here="$(cd "$(dirname "$0")" && pwd)"
AES_SOURCE="$(cd "$here/.." && pwd)"
. "$here/paths.sh"
need_beebium
# A tree that built AESulator 1.0.0 still has the plugin under its old name, which defines the same
# targets, so the build would fail with a message about duplicate targets (review, 21 Nov 2026).
if [ -d "$BEEBIUM_DIR/src/extensions/aes-coprocessor" ] ||
    grep -q 'add_subdirectory(aes-coprocessor)' "$BEEBIUM_DIR/src/extensions/CMakeLists.txt"; then
    echo "This Beebium folder still holds AESulator 1.0.0's plugin (src/extensions/aes-coprocessor)." >&2
    echo "Use a fresh clone of Beebium v0.20.1, as BUILDING.md says, and point BEEBIUM_DIR at it." >&2
    exit 1
fi
# By content, and a changed file gets a new date: with its old date Ninja would keep the old
# object, and code from an earlier build, such as a stub, would stay in without a word.
rsync -rlc --delete "${AES_PLUGIN_SRC:-$here/../acorn-aes-coprocessor}/" "$BEEBIUM_DIR/src/extensions/acorn-aes-coprocessor/"
grep -q acorn-aes-coprocessor "$BEEBIUM_DIR/src/extensions/CMakeLists.txt" ||
    echo 'add_subdirectory(acorn-aes-coprocessor)' >>"$BEEBIUM_DIR/src/extensions/CMakeLists.txt"
grep -q beebium_ext_aes_plugin "$BEEBIUM_DIR/src/server/CMakeLists.txt" ||
    sed -i '' 's/^        beebium_ext_acorn_65c102_plugin$/&\n        beebium_ext_aes_plugin/' "$BEEBIUM_DIR/src/server/CMakeLists.txt"
exec "$here/beebium-build.sh"
