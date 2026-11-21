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
# run-py.sh, runs a Python tool in the tests' environment
# version 2.0.0, 20 November 2026.

# Runs a Python tool in the tests' environment: tools/run-py.sh tools/run-frozen.py <folder>

set -e
here="$(cd "$(dirname "$0")/../acorn-aes-coprocessor/tests" && pwd)"
. "$here/test-env.sh"
exec uv run --frozen --project "$tree/tests" python "$@"
