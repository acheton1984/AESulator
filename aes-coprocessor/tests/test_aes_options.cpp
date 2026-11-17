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
// test_aes_options.cpp, tests the switches against the profile and manifest
// version 1.0.0, 17 November 2026.

// The switches: each row of the project's hardware notes (not shipped; docs/about-aesulator.md
// describes the switches). Three are Beebium settings, compared
// with aes-coprocessor/manifest.json, read as text so that they cannot drift.
#include "../AesOptions.hpp"
#include "ArmTest.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <string>

// Changed 1 Nov 2026 (USER): Beebium offers three settings, and the rest stay at their defaults
// in the code. These tests replace options_keys_match_manifest, options_defaults_match_manifest
// and options_every_choice_lands. The manifest was their independent list; now the three
// settings are compared with it exactly, the profile's defaults for every row are written out
// below, and every row's choices are run from the table, so nothing is checked less.
struct ManifestRow {
    std::string fallback;
    std::vector<std::string> choices;
};

// A choice is a bare value or {"value": ..., "label": ..., "description": ...} (SRC, Beebium's
// manifest loader); only the values are compared. Changed 5 Nov 2026 (USER, OI-18): the
// manifest's choices gained labels, which the earlier pattern could not read.
static std::map<std::string, ManifestRow> manifest_rows() {
    const char* folder = getenv("AES_PLUGIN_DIR");
    std::ifstream file(std::string(folder ? folder : "..") + "/manifest.json");
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    std::map<std::string, ManifestRow> rows;
    const std::regex key("\"key\": \"([a-z0-9_.]+)\"");
    const std::regex fallback("\"default\": \"([^\"]*)\"");
    const std::regex value("\"value\": \"([^\"]+)\"");
    const std::regex word("\"([^\"]+)\"");
    std::vector<std::pair<std::string, size_t>> starts;
    for (std::sregex_iterator it(text.begin(), text.end(), key), end; it != end; ++it) {
        starts.push_back({(*it)[1], size_t(it->position())});
    }
    for (size_t n = 0; n < starts.size(); n++) {
        const size_t stop = n + 1 < starts.size() ? starts[n + 1].second : text.size();
        const std::string body = text.substr(starts[n].second, stop - starts[n].second);
        ManifestRow r;
        std::smatch found;
        if (std::regex_search(body, found, fallback)) r.fallback = found[1];
        const size_t open = body.find("\"choices\": [");
        if (open != std::string::npos) {
            const std::string list = body.substr(open + 12, body.find(']', open) - open - 12);
            const bool labelled = list.find("\"value\"") != std::string::npos;
            for (std::sregex_iterator w(list.begin(), list.end(), labelled ? value : word), e; w != e; ++w) {
                r.choices.push_back((*w)[1]);
            }
        }
        rows[starts[n].first] = r;
    }
    return rows;
}

// The profile's defaults, written out by hand from the project's hardware notes, so that a
// changed default in the code has to be a choice: change the profile and this list with it.
static const std::map<std::string, std::string> kProfileDefaults = {
    {"adx.address_checked", "address-used"},
    {"adx.base_writeback", "false"},
    {"adx.ldm_first_only", "true"},
    {"adx.r14", "8"},
    {"adx.store_to_load", "true"},
    {"boot.latch", "any-write"},
    {"cpu.enc_a_bit7", "ignored"},
    {"cpu.enc_b_rd", "no-write"},
    {"cpu.ldm_pc_wb", "user-bank"},
    {"cpu.ldm_user_bank", "true"},
    {"cpu.ldr_reg_shift", "transfers"},
    {"cpu.mode_delay", "reads-old-writes-new"},
    {"cpu.reg_shift_zero_carry", "kept"},
    {"cpu.reset_r14", "r15-minus-4"},
    {"cpu.ror_reg_carry", "zero-above-31"},
    {"cpu.rrx_carry", "bit0-or-bit31"},
    {"cpu.undef_failed_cond", "traps"},
    {"cpu.word_unaligned", "ignore-low-bits"},
    {"ldm.fault_loads", "complete"},
    {"abort", "off"},
    {"access.check", "off"},
    {"ram", "4"},
    {"r15.reg_shift_read", "12"},
    {"r15.stm_value", "12"},
    {"r15.str_value", "12"},
    {"ram.absent_bank", "all-ones"},
    {"ram.power_on", "zero"},
    {"ram.repeat", "address-lines"},
    {"region10.read", "nothing"},
    {"region10.write", "discard"},
    {"rom.repeat", "every-16k"},
    {"time.block_break", "costs-n"},
    {"time.ldm_cycles", "sw86"},
    {"time.ldr_reg_shift", "one-s"},
    {"time.links", "normal"},
    {"time.ram_bank", "0"},
    {"time.refresh_cost", "9"},
    {"time.refresh_period", "192"},
    {"time.rom_wait", "8"},
    {"time.tube_sync", "none"},
    {"time.tube_wait", "2"},
    {"tube.addr_decode", "lower-half"},
    {"tube.prst_level", "holds"},
    {"tube.width", "low-byte"}
};

TEST(options_three_settings_match_their_rows, "Beebium offers exactly rom, ram and abort, and each matches its table row in default and choices (USER, 1 Nov 2026)") {
    const auto rows = manifest_rows();
    const AesOptions plain;
    CHECK_EQ(rows.size(), 3);
    for (const char* key : {"rom", "ram", "abort"}) CHECK_EQ(rows.count(key), 1);
    for (const OptionRow& row : option_table()) {
        const auto found = rows.find(row.key);
        if (found == rows.end()) continue;
        CHECK_EQ(found->second.choices == row.choices, true);
        if (row.key != "rom") CHECK_EQ(found->second.fallback == row.get(plain), true);
    }
    CHECK_EQ(option_table().size(), 45);
}

TEST(options_every_default_is_the_profiles, "Every row but the ROM path holds the profile's default (USER, build rules; docs/HARDWARE.md)") {
    const AesOptions plain;
    CHECK_EQ(kProfileDefaults.size(), 44);
    long checked = 0;
    for (const OptionRow& row : option_table()) {
        const auto found = kProfileDefaults.find(row.key);
        if (found == kProfileDefaults.end()) continue;
        checked++;
        if (found->second != row.get(plain)) printf("  %s: profile %s, options %s\n", row.key.c_str(), found->second.c_str(), row.get(plain).c_str());
        CHECK_EQ(found->second == row.get(plain), true);
    }
    CHECK_EQ(checked, 44);
}

TEST(options_every_choice_lands, "Every choice in the table is taken, reaches its row and reads back, and every row has one to choose, so no switch is a dead name (USER, build rules; design)") {
    long choices = 0;
    for (const OptionRow& row : option_table()) {
        if (row.key == "rom") continue;
        CHECK_EQ(row.choices.size() >= 2, true);
        for (const std::string& choice : row.choices) {
            AesOptions o;
            CHECK_EQ(set_option(o, row.key, choice), true);
            CHECK_EQ(row.get(o) == choice, true);
            choices++;
        }
    }
    CHECK_EQ(choices >= 90, true);
}

TEST(options_each_sets_its_own, "A setting reaches its own field: ram, a board switch and a core switch (USER, Plugin option 3; design)") {
    AesOptions o;
    CHECK_EQ(set_option(o, "ram", "2"), true);
    CHECK_EQ(o.ram, 2);
    CHECK_EQ(set_option(o, "rom", "/some/where.rom"), true);
    CHECK_EQ(o.rom == "/some/where.rom", true);
    CHECK_EQ(set_option(o, "abort", "strict"), true);
    CHECK_EQ(o.abort == AesOptions::MachineAbort::Strict, true);
    CHECK_EQ(set_option(o, "time.rom_wait", "5"), true);
    CHECK_EQ(o.time_rom_wait, 5);
    CHECK_EQ(set_option(o, "cpu.rrx_carry", "bit0"), true);
    CHECK_EQ(o.cpu.rrx_carry == ArmOptions::RrxCarry::Bit0, true);
    CHECK_EQ(set_option(o, "adx.r14", "4"), true);
    CHECK_EQ(o.cpu.adx_r14, 4);
    CHECK_EQ(set_option(o, "adx.base_writeback", "true"), true);
    CHECK_EQ(o.cpu.adx_base_writeback, true);
    CHECK_EQ(set_option(o, "tube.prst_level", "pulse"), true);
    CHECK_EQ(o.tube_prst_level == AesOptions::TubePrstLevel::Pulse, true);
}

TEST(options_removed_choices, "ram 1 and abort empty are no longer choices; 2, 4, off and strict still are (USER, 5 Nov 2026)") {
    AesOptions o;
    CHECK_EQ(set_option(o, "ram", "1"), false);
    CHECK_EQ(set_option(o, "abort", "empty"), false);
    CHECK_EQ(set_option(o, "ram", "2"), true);
    CHECK_EQ(set_option(o, "abort", "strict"), true);
    CHECK_EQ(set_option(o, "abort", "off"), true);
}

TEST(options_refuses_nonsense, "An unknown name or value is refused, never quietly left at the default (design)") {
    AesOptions o;
    // The same names with good values go through, so a refusal below means something.
    CHECK_EQ(set_option(o, "ram", "4"), true);
    CHECK_EQ(set_option(o, "abort", "off"), true);
    CHECK_EQ(set_option(o, "time.rom_wait", "0"), true);
    o.time_rom_wait = 8;
    CHECK_EQ(set_option(o, "machine.rams", "2"), false);
    CHECK_EQ(set_option(o, "ram", "3"), false);
    CHECK_EQ(set_option(o, "abort", "maybe"), false);
    CHECK_EQ(set_option(o, "time.rom_wait", "soon"), false);
    CHECK_EQ(set_option(o, "time.rom_wait", "-1"), false);
    CHECK_EQ(set_option(o, "time.refresh_period", "0"), false);  // It would hang AesTiming.
    CHECK_EQ(set_option(o, "r15.str_value", "9"), false);
    CHECK_EQ(o.ram, 4);
    CHECK_EQ(o.time_rom_wait, 8);
}

// Added 1 Nov 2026 (USER): fault 132 showed that nothing checked the upper limits. Beebium
// offers only three settings, so in a running server nothing reaches these rows;
// options_defaults_are_in_range (test_a_defaults.cpp) guards the edited defaults. The limits are
// written out as docs/user-guide.md states them, not read from the table.
TEST(options_every_ranged_row_keeps_its_limits, "Each number's least and greatest values are taken, and one below or one above is refused, row by row (USER, 1 Nov 2026; design)") {
    const struct { const char* key; int low, high; } rows[] = {
        {"time.refresh_period", 1, 100000},
        {"time.refresh_cost", 0, 1000},
        {"time.rom_wait", 0, 1000},
        {"time.tube_wait", 0, 1000},
        {"time.ram_bank", 0, 1000},
    };
    for (const auto& row : rows) {
        AesOptions o;
        CHECK_EQ(set_option(o, row.key, std::to_string(row.low)), true);
        CHECK_EQ(set_option(o, row.key, std::to_string(row.high)), true);
        CHECK_EQ(set_option(o, row.key, std::to_string(row.low - 1)), false);
        CHECK_EQ(set_option(o, row.key, std::to_string(row.high + 1)), false);
    }
}
