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
# paths.sh, reads aes-paths.sh and sets the defaults
# version 1.0.0, 17 November 2026.

# Sourced by our scripts, which set AES_SOURCE to this folder's parent first. Where things are:
# aes-paths.sh beside tools/ holds what cannot be worked out, and a variable already set wins.
# The rest follows Beebium: its build folder is build/ (SRC, find_in_build_tree), and it runs
# from BEEBIUM_SERVER and BEEBIUM_ROM_DIR, which we always name (SRC, its ci.yml).
# The file may set a variable plainly (BEEBIUM_DIR="..."), so what was set before it is kept and put back.
if [ -f "$AES_SOURCE/aes-paths.sh" ]; then
    for aes_name in BEEBIUM_DIR BEEBIUM_BUILD AES_ROM AES_PROJECT AES_BUILD_JOBS; do
        eval "aes_set_$aes_name=\"\${$aes_name-}\""
    done
    . "$AES_SOURCE/aes-paths.sh"
    for aes_name in BEEBIUM_DIR BEEBIUM_BUILD AES_ROM AES_PROJECT AES_BUILD_JOBS; do
        eval "[ -z \"\$aes_set_$aes_name\" ] || $aes_name=\"\$aes_set_$aes_name\""
    done
fi
if [ -z "$BEEBIUM_DIR" ]; then
    echo "BEEBIUM_DIR is not set: copy aes-paths.template.sh to aes-paths.sh and fill it in." >&2
    exit 1
fi
: "${BEEBIUM_BUILD:=$BEEBIUM_DIR/build}"
: "${BEEBIUM_SERVER:=$BEEBIUM_BUILD/src/server/beebium-model-b}"
: "${BEEBIUM_ROM_DIR:=$BEEBIUM_DIR/roms}"
: "${AES_SCRATCH:=$BEEBIUM_BUILD/aes}"
export BEEBIUM_DIR BEEBIUM_BUILD BEEBIUM_SERVER BEEBIUM_ROM_DIR AES_SCRATCH
[ -z "$AES_ROM" ] || export AES_ROM
[ -z "$AES_PROJECT" ] || export AES_PROJECT

# A moved Beebium must not pass unnoticed: a build would quietly start again at the old place.
need_beebium() {
    if [ ! -f "$BEEBIUM_DIR/CMakeLists.txt" ]; then
        echo "No Beebium at BEEBIUM_DIR, $BEEBIUM_DIR: fix aes-paths.sh, or use tools/beebium-build.sh --copy-from <Beebium folder>." >&2
        exit 1
    fi
}

# The Python environment the tests and the tripwire run in, set here only.
python_env() {
    export UV_PROJECT_ENVIRONMENT="$AES_SCRATCH/venv"
    # Private libraries, if Beebium was built with them.
    if [ -n "$CMAKE_PREFIX_PATH" ]; then export DYLD_FALLBACK_LIBRARY_PATH="$CMAKE_PREFIX_PATH/lib"; fi
}
