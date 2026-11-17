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
// AesTube.hpp, the coprocessor's interface
// version 1.0.0, 17 November 2026.

// The AES on the far side of Beebium's Tube: the core, the board and the time between them.
// Beebium's TubeUla is linked and called, never copied (USER, 31 Oct 2026).
#pragma once

#include "../AesMemory.hpp"
#include "../AesOptions.hpp"
#include "../AesTiming.hpp"
#include "../ArmCore.hpp"

#include <beebium/tube/Coprocessor.hpp>
#include <beebium/tube/CoprocessorClock.hpp>
#include <beebium/tube/TubeUla.hpp>

#ifdef AES_TUBE_LOG
#include "TubeLog.hpp"
#endif

class AesTube : public beebium::Coprocessor, public TubePort {
public:
    AesTube(const AesOptions& options, beebium::TubeUla& ula);

    bool load_rom() { return memory_.load_rom(); }
    void set_rom(const std::vector<uint8_t>& image) { memory_.set_rom(image); }

    void run_until(uint64_t host_cycle) override;
    void pause() override { paused_ = true; }
    void resume() override { paused_ = false; }
    bool is_paused() const override { return paused_; }
    void reset() override;
    beebium::BoardTiming board_timing() const override { return board_label(); }
    static const beebium::BoardTiming& board_label();

    uint8_t read(int reg) override { return ula_.coprocessor_read(uint8_t(reg)); }
    void write(int reg, uint8_t value) override { ula_.coprocessor_write(uint8_t(reg), value); }

    uint64_t ticks_due() const { return ticks_due_; }
    uint64_t ticks_spent() const { return timing_.ticks(); }
    ArmCore& core() { return core_; }
    AesMemory& memory() { return memory_; }

private:
    bool held_by_prst();
    void restart();
    bool interrupt_will_be_taken() const;
    int tube_instruction_cost(uint32_t at, int words) const;
    bool must_wait_for_tube();

    const AesOptions opt_;
    beebium::TubeUla& ula_;
    AesTiming timing_;
#ifdef AES_TUBE_LOG
    TubeLog log_;  // The test build only (USER, 2 Nov 2026).
#endif
    AesMemory memory_;
    ArmCore core_;
    beebium::CoprocessorClock clock_;
    uint64_t ticks_due_ = 0;
    bool prst_was_active_ = false;
    bool paused_ = false;
};
