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
// AesOptions.cpp, the table of switches, by name
// version 1.0.0, 17 November 2026.

// The table of switches, by name: sets and checks each AesOptions field from a name and a
// value. Three are Beebium settings (manifest.json); the rest change by editing a default.

#include "AesOptions.hpp"

#include <charconv>
#include <utility>

namespace {

using A = AesOptions;
using C = ArmOptions;

template <class Field>
using Names = std::vector<std::pair<std::string, Field>>;

// A switch with a fixed list of values. `field` finds it in the options.
template <class Field>
OptionRow choice(const std::string& key, Field& (*field)(A&), Names<Field> names) {
    std::vector<std::string> choices;
    for (const auto& [name, value] : names) choices.push_back(name);
    const auto get = [field, names](const A& options) {
        const Field now = field(const_cast<A&>(options));
        for (const auto& [name, value] : names) {
            if (value == now) return name;
        }
        return std::string();
    };
    const auto set = [field, names](A& options, const std::string& text) {
        for (const auto& [name, value] : names) {
            if (name == text) {
                field(options) = value;
                return true;
            }
        }
        return false;
    };
    return {key, choices, set, get};
}

// A number with limits. time.refresh_period must not be 0: AesTiming's refresh count would
// never end.
OptionRow number(const std::string& key, int& (*field)(A&), int low, int high) {
    const auto get = [field](const A& options) { return std::to_string(field(const_cast<A&>(options))); };
    const auto set = [field, low, high](A& options, const std::string& text) {
        int value = 0;
        const char* end = text.data() + text.size();
        const auto [at, error] = std::from_chars(text.data(), end, value);
        if (error != std::errc() || at != end || value < low || value > high) return false;
        field(options) = value;
        return true;
    };
    return {key, {std::to_string(low), std::to_string(high)}, set, get};
}

OptionRow path(const std::string& key) {
    const auto get = [](const A& options) { return options.rom; };
    const auto set = [](A& options, const std::string& text) {
        options.rom = text;
        return true;
    };
    return {key, {}, set, get};
}

const Names<bool> kBool = {{"true", true}, {"false", false}};
const Names<int> kAhead = {{"12", 12}, {"8", 8}};  // r15 rows: bytes ahead (profile section 3).

}  // namespace

// USER: the keys are the switches' names (docs/about-aesulator.md). Beebium offers three of
// these (manifest.json); the rest are changed by editing their defaults. A profile row with
// no alternative in the code, such as ram.absent_write, has no row: nothing to choose.
#define FIELD(name) [](A& o) -> auto& { return o.name; }
const std::vector<OptionRow>& option_table() {
    static const std::vector<OptionRow> table = {
        choice<int>("ram", FIELD(ram), {{"2", 2}, {"4", 4}}),  // USER, Plugin option 3; 1 MB removed, USER, 5 Nov 2026.
        path("rom"),
        choice<A::MachineAbort>("abort", FIELD(abort), {{"off", A::MachineAbort::Off}, {"strict", A::MachineAbort::Strict}}),
        choice<A::MachineCheck>("access.check", FIELD(access_check), {{"off", A::MachineCheck::Off}, {"log", A::MachineCheck::Log}, {"abort", A::MachineCheck::Abort}}),

        choice<C::RrxCarry>("cpu.rrx_carry", FIELD(cpu.rrx_carry), {{"bit0-or-bit31", C::RrxCarry::Bit0OrBit31}, {"bit0", C::RrxCarry::Bit0}}),
        choice<C::RorRegCarry>("cpu.ror_reg_carry", FIELD(cpu.ror_reg_carry), {{"zero-above-31", C::RorRegCarry::ZeroAbove31}, {"zero-at-multiples-of-32", C::RorRegCarry::ZeroAtMultiplesOf32}, {"arm2", C::RorRegCarry::Arm2}}),
        choice<C::RegShiftZeroCarry>("cpu.reg_shift_zero_carry", FIELD(cpu.reg_shift_zero_carry), {{"kept", C::RegShiftZeroCarry::Kept}, {"cleared", C::RegShiftZeroCarry::Cleared}}),
        choice<C::LdrRegShift>("cpu.ldr_reg_shift", FIELD(cpu.ldr_reg_shift), {{"transfers", C::LdrRegShift::Transfers}, {"traps", C::LdrRegShift::Traps}}),
        choice<C::EncABit7>("cpu.enc_a_bit7", FIELD(cpu.enc_a_bit7), {{"ignored", C::EncABit7::Ignored}, {"traps", C::EncABit7::Traps}}),
        choice<C::EncBRd>("cpu.enc_b_rd", FIELD(cpu.enc_b_rd), {{"no-write", C::EncBRd::NoWrite}, {"writes-rd", C::EncBRd::WritesRd}, {"trap", C::EncBRd::Trap}}),
        choice<C::UndefFailedCond>("cpu.undef_failed_cond", FIELD(cpu.undef_failed_cond), {{"traps", C::UndefFailedCond::Traps}, {"skipped", C::UndefFailedCond::Skipped}}),
        choice<C::WordUnaligned>("cpu.word_unaligned", FIELD(cpu.word_unaligned), {{"ignore-low-bits", C::WordUnaligned::IgnoreLowBits}, {"rotate", C::WordUnaligned::Rotate}}),

        choice<int>("r15.reg_shift_read", FIELD(cpu.r15_reg_shift_read), kAhead),
        choice<int>("r15.str_value", FIELD(cpu.r15_str_value), kAhead),
        choice<int>("r15.stm_value", FIELD(cpu.r15_stm_value), kAhead),
        choice<C::ModeDelay>("cpu.mode_delay", FIELD(cpu.mode_delay), {{"reads-old-writes-new", C::ModeDelay::ReadsOldWritesNew}, {"old-bank-whole-instruction", C::ModeDelay::OldBankWholeInstruction}, {"none", C::ModeDelay::None}}),
        choice<bool>("cpu.ldm_user_bank", FIELD(cpu.ldm_user_bank), kBool),
        choice<C::LdmPcWb>("cpu.ldm_pc_wb", FIELD(cpu.ldm_pc_wb), {{"user-bank", C::LdmPcWb::UserBank}, {"current-bank", C::LdmPcWb::CurrentBank}}),
        choice<C::ResetR14>("cpu.reset_r14", FIELD(cpu.reset_r14), {{"r15-minus-4", C::ResetR14::R15Minus4}, {"r15", C::ResetR14::R15}}),

        choice<int>("adx.r14", FIELD(cpu.adx_r14), {{"8", 8}, {"4", 4}}),
        choice<bool>("adx.ldm_first_only", FIELD(cpu.adx_ldm_first_only), kBool),
        choice<C::AddressChecked>("adx.address_checked", FIELD(cpu.adx_address_checked), {{"address-used", C::AddressChecked::AddressUsed}, {"base", C::AddressChecked::Base}}),
        choice<bool>("adx.base_writeback", FIELD(cpu.adx_base_writeback), kBool),
        choice<bool>("adx.store_to_load", FIELD(cpu.adx_store_to_load), kBool),
        choice<C::LdmFaultLoads>("ldm.fault_loads", FIELD(cpu.ldm_fault_loads), {{"complete", C::LdmFaultLoads::Complete}, {"stop-overwriting", C::LdmFaultLoads::StopOverwriting}}),

        choice<A::RamRepeat>("ram.repeat", FIELD(ram_repeat), {{"address-lines", A::RamRepeat::AddressLines}, {"fitted-size", A::RamRepeat::FittedSize}}),
        choice<A::RamPowerOn>("ram.power_on", FIELD(ram_power_on), {{"zero", A::RamPowerOn::Zero}, {"random", A::RamPowerOn::Random}}),
        choice<A::RamAbsentBank>("ram.absent_bank", FIELD(ram_absent_bank), {{"all-ones", A::RamAbsentBank::AllOnes}, {"last-bus", A::RamAbsentBank::LastBus}, {"low-ram", A::RamAbsentBank::LowRam}}),
        choice<A::RomRepeat>("rom.repeat", FIELD(rom_repeat), {{"every-16k", A::RomRepeat::Every16K}, {"none", A::RomRepeat::None}}),
        choice<A::BootLatch>("boot.latch", FIELD(boot_latch), {{"any-write", A::BootLatch::AnyWrite}, {"ram-writes-only", A::BootLatch::RamWritesOnly}}),
        choice<A::Region10Read>("region10.read", FIELD(region10_read), {{"nothing", A::Region10Read::Nothing}, {"abort", A::Region10Read::Abort}, {"ram-repeat", A::Region10Read::RamRepeat}}),
        choice<A::Region10Write>("region10.write", FIELD(region10_write), {{"discard", A::Region10Write::Discard}, {"ram-repeat", A::Region10Write::RamRepeat}}),
        choice<A::TubeAddrDecode>("tube.addr_decode", FIELD(tube_addr_decode), {{"lower-half", A::TubeAddrDecode::LowerHalf}, {"whole-region", A::TubeAddrDecode::WholeRegion}}),
        choice<A::TubeWidth>("tube.width", FIELD(tube_width), {{"low-byte", A::TubeWidth::LowByte}, {"undefined", A::TubeWidth::Undefined}}),
        choice<A::TubePrstLevel>("tube.prst_level", FIELD(tube_prst_level), {{"holds", A::TubePrstLevel::Holds}, {"pulse", A::TubePrstLevel::Pulse}}),

        choice<C::LdmCycles>("time.ldm_cycles", FIELD(cpu.time_ldm_cycles), {{"sw86", C::LdmCycles::Sw86}, {"sw85", C::LdmCycles::Sw85}}),
        choice<C::LdrRegShiftTime>("time.ldr_reg_shift", FIELD(cpu.time_ldr_reg_shift), {{"one-s", C::LdrRegShiftTime::OneS}, {"none", C::LdrRegShiftTime::None}}),
        choice<A::BlockBreak>("time.block_break", FIELD(time_block_break), {{"costs-n", A::BlockBreak::CostsN}, {"none", A::BlockBreak::None}}),
        number("time.refresh_period", FIELD(time_refresh_period), 1, 100000),
        number("time.refresh_cost", FIELD(time_refresh_cost), 0, 1000),
        number("time.rom_wait", FIELD(time_rom_wait), 0, 1000),
        number("time.tube_wait", FIELD(time_tube_wait), 0, 1000),
        choice<A::TubeSync>("time.tube_sync", FIELD(time_tube_sync), {{"none", A::TubeSync::None}, {"next-host-edge", A::TubeSync::NextHostEdge}}),
        number("time.ram_bank", FIELD(time_ram_bank), 0, 1000),
        choice<A::Links>("time.links", FIELD(time_links), {{"normal", A::Links::Normal}, {"seq-tied", A::Links::SeqTied}}),
    };
    return table;
}
#undef FIELD

bool set_option(AesOptions& options, const std::string& key, const std::string& value) {
    for (const OptionRow& row : option_table()) {
        if (row.key == key) return row.set(options, value);
    }
    return false;
}
