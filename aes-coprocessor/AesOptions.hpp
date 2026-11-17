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
// AesOptions.hpp, every switch for the board, with its default
// version 1.0.0, 17 November 2026.

// Every switch for the AES board; docs/about-aesulator.md explains them, with examples. Each
// name is the switch's name with _ for the dot: time_rom_wait is time.rom_wait. Beebium offers
// three as settings, rom, ram and abort, filled through set_option when the machine is built
// (USER, 1 Nov 2026). To change any other, edit its default here or in ArmOptions, and rebuild;
// options_defaults_are_in_range checks the edit, as Beebium does not.
#pragma once

#include "ArmCore.hpp"

#include <functional>
#include <string>
#include <vector>

struct AesOptions {
    enum class MachineAbort { Off, Strict };
    enum class MachineCheck { Off, Log, Abort };
    enum class RamRepeat { AddressLines, FittedSize };
    enum class RamPowerOn { Zero, Random };
    enum class RamAbsentBank { AllOnes, LastBus, LowRam };
    enum class RomRepeat { Every16K, None };
    enum class BootLatch { AnyWrite, RamWritesOnly };
    enum class Region10Read { Nothing, Abort, RamRepeat };
    enum class Region10Write { Discard, RamRepeat };
    enum class TubeAddrDecode { LowerHalf, WholeRegion };
    enum class TubeWidth { LowByte, Undefined };
    enum class BlockBreak { CostsN, None };
    enum class TubeSync { None, NextHostEdge };
    enum class Links { Normal, SeqTied };
    enum class TubePrstLevel { Holds, Pulse };

    ArmOptions cpu;

    int ram = 4;  // MB: 2 or 4 (USER, Plugin option 3; 1 MB removed, USER, 5 Nov 2026).
    MachineAbort abort = MachineAbort::Off;
    // USER, Plugin option 4: Executive 1.00, "1.01", SHA-1
    // f86bbc4894e62725b8ef22d44e7f44d37c98ac14. None bundled.
    std::string rom = "armeval_101.rom";
    MachineCheck access_check = MachineCheck::Off;

    RamRepeat ram_repeat = RamRepeat::AddressLines;
    RamPowerOn ram_power_on = RamPowerOn::Zero;
    RamAbsentBank ram_absent_bank = RamAbsentBank::AllOnes;
    RomRepeat rom_repeat = RomRepeat::Every16K;
    BootLatch boot_latch = BootLatch::AnyWrite;
    Region10Read region10_read = Region10Read::Nothing;
    Region10Write region10_write = Region10Write::Discard;
    TubeAddrDecode tube_addr_decode = TubeAddrDecode::LowerHalf;
    TubeWidth tube_width = TubeWidth::LowByte;
    TubePrstLevel tube_prst_level = TubePrstLevel::Holds;

    BlockBreak time_block_break = BlockBreak::CostsN;
    int time_refresh_period = 192;  // Crystal ticks (INF, item 12).
    int time_refresh_cost = 9;      // Crystal ticks (INF from DOC, HW p.36).
    int time_rom_wait = 8;          // PAL clocks (INF, item 14).
    int time_tube_wait = 2;         // PAL clocks (INF, item 14).
    TubeSync time_tube_sync = TubeSync::None;
    int time_ram_bank = 0;  // Extra crystal ticks per access on the 2 MB card (INF).
    Links time_links = Links::Normal;
};

// One row per switch, named as in the profile. Three are also in manifest.json.
struct OptionRow {
    std::string key;
    std::vector<std::string> choices;  // For a number, its least and greatest. Empty for a path.
    std::function<bool(AesOptions&, const std::string&)> set;
    std::function<std::string(const AesOptions&)> get;
};
const std::vector<OptionRow>& option_table();

// False for an unknown name or value, so that nothing is quietly left at its default.
bool set_option(AesOptions& options, const std::string& key, const std::string& value);
