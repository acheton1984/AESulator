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
# aes-paths.template.sh, where things are: copy to aes-paths.sh and fill in
# version 1.0.0, 17 November 2026.

# Copy this file to aes-paths.sh, in the same folder; the scripts read
# aes-paths.sh themselves.
# Remove the # at the start of a line and put your own path between the quotes.
# A variable already set in your terminal with export is used instead of the
# line here.

# Beebium's source folder, v0.15.0. Needed.
#BEEBIUM_DIR="/path/to/beebium"

# The Executive ROM. Without it the tests tied to the ROM skip.
#AES_ROM="/path/to/rom"

# Beebium's build folder, only if it is not build/ inside Beebium's folder.
#BEEBIUM_BUILD="/path/to/build"

# How many files the build compiles at a time; 4 if unset.
#AES_BUILD_JOBS="4"
