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
// AesExtension.cpp, the plugin as Beebium sees it: settings, start and stop
// version 1.0.0, 17 November 2026.

// The plugin as Beebium sees it: reads the settings, says why a ROM was refused, and
// starts and stops the AES with the machine.

#include "AesExtension.hpp"

#include <beebium/ClockText.hpp>
#include <beebium/extension/ExtensionContext.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

// USER, 1 Nov 2026: says what is wrong with the path, once AesMemory has refused it. Beebium
// takes any text as a filepath (SRC, ExtensionArgParser.cpp), so the missing file is ours to
// report. A folder and an unreadable file get one message, not told apart (USER).
std::string rom_refusal(const std::string& path) {
    std::error_code error;
    if (std::filesystem::status(path, error).type() == std::filesystem::file_type::not_found) {
        return "AES: no file at " + path;
    }
    const uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size == AesMemory::kRomBytes) return "AES: could not read " + path + " as a file";
    return "AES: " + path + " is " + std::to_string(size) + " bytes; a ROM must be exactly 16,384";
}

}  // namespace

AesOptions AesExtension::options_from_settings() const {
    AesOptions options;
    for (const OptionRow& row : option_table()) {
        const auto value = config_value(row.key);
        if (value && !set_option(options, row.key, std::string(*value))) {
            throw std::runtime_error("AES: " + row.key + " cannot be " + std::string(*value));
        }
    }
    return options;
}

void AesExtension::init(beebium::ExtensionContext& context) {
    socket_ = &context.get<beebium::TubeSocket>();
    const AesOptions options = options_from_settings();
    ula_ = std::make_unique<beebium::TubeUla>();
    tube_ = std::make_unique<AesTube>(options, *ula_);
    // USER, Plugin option 4: read from a file at run time, none bundled.
    if (!tube_->load_rom()) throw std::runtime_error(rom_refusal(options.rom));
    socket_->install_backend(ula_.get());
    socket_->install_coprocessor(tube_.get());
    std::cout << "  " << cpu_name() << " (" << beebium::format_clock_mhz(clock_hz()) << ") coprocessor\n";
}

void AesExtension::shutdown() {
    if (!tube_) return;
    socket_->remove_coprocessor();
    socket_->install_backend(nullptr);
    tube_.reset();
    ula_.reset();
}

// A label (DOC, Beebium guide section 2): derived from the figures the board is driven by.
uint64_t AesExtension::clock_hz() const {
    return beebium::nominal_clock_hz(AesTube::board_label());
}
