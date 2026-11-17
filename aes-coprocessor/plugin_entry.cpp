// Copyright 2026 James Sargent <acheton1984@letterboxes.org>
//
// This file is part of AESulator, a plugin for Beebium.
//
// AESulator is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version. AESulator is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with AESulator.
// If not, see <https://www.gnu.org/licenses/>.

// AESulator: the Acorn ARM Evaluation System, a Beebium plugin.
// plugin_entry.cpp, the function Beebium's loader calls
// version 1.0.0, 17 November 2026.

// The name Beebium's loader looks for (DOC, Beebium guide section 7). At the top of the folder,
// as in Beebium's own coprocessors.
#include "beebium/AesExtension.hpp"

#include <beebium/extension/Extension.hpp>
#include <beebium/extension/ExtensionManifest.hpp>

extern "C" {
BEEBIUM_PLUGIN_EXPORT
beebium::Extension* beebium_create_extension(const beebium::ExtensionManifest& manifest) {
    auto* extension = new AesExtension();
    extension->set_manifest(manifest);
    return extension;
}
}
