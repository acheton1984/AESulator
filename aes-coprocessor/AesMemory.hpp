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
// AesMemory.hpp, the memory map's interface
// version 1.0.0, 17 November 2026.

// The AES memory map behind ArmBus: RAM, ROM, Tube, region 10 and the reset ROM swap.
#pragma once

#include "AesOptions.hpp"
#include "AesTiming.hpp"
#include "ArmCore.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// beebium/AesTube puts Beebium's Tube behind this (USER, 31 Oct 2026). `reg` is 0-7, the chip's
// numbering (DOC, HW 5.3: PA0-PA2).
class TubePort {
public:
    virtual ~TubePort() = default;
    virtual uint8_t read(int reg) = 0;
    virtual void write(int reg, uint8_t value) = 0;
};

inline bool in_tube(uint32_t address) { return ((address >> 24) & 3) == 1; }  // DOC, HW 5.1.5.

class AesMemory : public ArmBus {
public:
    static constexpr uint32_t kRomBytes = 0x4000;  // DOC, HW parts list: a 27128.
    static constexpr uint32_t kA24 = 0x01000000;

    AesMemory(const AesOptions& options, AesTiming& timing, TubePort& tube);

    // rom (USER, Plugin option 4): read at run time; false if missing or not 16 KB.
    bool load_rom();
    bool load_rom(const std::string& path);
    void set_rom(const std::vector<uint8_t>& image) { rom_ = image; }

    void power_on();
    void reset();

    uint32_t read_word(uint32_t addr, bool seq) override;
    uint32_t read_byte(uint32_t addr, bool seq) override;
    void write_word(uint32_t addr, uint32_t value, bool seq) override;
    void write_byte(uint32_t addr, uint8_t value, bool seq) override;
    void internal(uint32_t addr) override;

    // Design, notes item 28: the debugger sees memory with no time passing and no change.
    uint8_t peek(uint32_t addr) const;
    int suspect_accesses() const { return suspect_accesses_; }
    bool boot_latch_set() const { return boot_latch_; }
    bool strict_on() const { return strict_on_; }
    bool outside_strict_map(uint32_t addr) const;

private:
    enum class Kind { Ram, Rom, Tube, Region10, Empty };
    struct Target {
        Kind kind;
        uint32_t offset;
        bool fitted;
    };

    Target decode(uint32_t addr, bool for_write) const;
    Target ram_target(uint32_t addr) const;
    Target rom_target(uint32_t addr) const;
    AesTiming::Place place_of(const Target& target, uint32_t addr) const;
    void note_cycle(const Target& target, uint32_t addr, bool seq, bool for_write);
    Target start_read(uint32_t addr, bool seq);
    Target start_write(uint32_t addr, bool seq);
    uint32_t read_lanes(const Target& target) const;
    uint32_t undriven() const;
    uint32_t tube_lanes(uint32_t addr);
    bool abort_tied() const { return opt_.abort == AesOptions::MachineAbort::Off; }
    void suspect();
    void note_write(const Target& target);
    void watch_mode();

    const AesOptions opt_;
    AesTiming& timing_;
    TubePort& tube_;
    std::vector<uint8_t> ram_;
    std::vector<uint8_t> rom_;
    uint32_t fitted_bytes_;
    uint32_t last_bus_ = 0;
    bool boot_latch_ = true;
    bool strict_on_ = false;
    int suspect_accesses_ = 0;
};
