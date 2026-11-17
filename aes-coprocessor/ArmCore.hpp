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
// ArmCore.hpp, the core's interface, switches and bus
// version 1.0.0, 17 November 2026.

// ARM1 CPU core for the Acorn ARM Evaluation System plugin.
// Written from the documents only: docs/about-aesulator.md explains the switches.
#pragma once

#include <cstdint>

// Every uncertain behaviour is a named switch here, or a listed guess in section 7a
// of the behaviour profile with its label at the code site. The switch x is the
// profile's row cpu.x, with the same default.
struct ArmOptions {
    enum class RrxCarry { Bit0OrBit31, Bit0 };
    enum class RorRegCarry { ZeroAbove31, ZeroAtMultiplesOf32, Arm2 };
    enum class RegShiftZeroCarry { Kept, Cleared };
    enum class LdrRegShift { Transfers, Traps };
    enum class EncABit7 { Ignored, Traps };
    enum class EncBRd { NoWrite, WritesRd, Trap };
    enum class UndefFailedCond { Traps, Skipped };
    enum class WordUnaligned { IgnoreLowBits, Rotate };
    enum class ModeDelay { ReadsOldWritesNew, OldBankWholeInstruction, None };
    enum class LdmPcWb { UserBank, CurrentBank };
    enum class ResetR14 { R15Minus4, R15 };
    enum class AddressChecked { AddressUsed, Base };
    enum class LdmFaultLoads { Complete, StopOverwriting };
    enum class LdmCycles { Sw86, Sw85 };
    enum class LdrRegShiftTime { OneS, None };

    RrxCarry rrx_carry = RrxCarry::Bit0OrBit31;
    RorRegCarry ror_reg_carry = RorRegCarry::ZeroAbove31;
    RegShiftZeroCarry reg_shift_zero_carry = RegShiftZeroCarry::Kept;
    LdrRegShift ldr_reg_shift = LdrRegShift::Transfers;
    EncABit7 enc_a_bit7 = EncABit7::Ignored;
    EncBRd enc_b_rd = EncBRd::NoWrite;
    UndefFailedCond undef_failed_cond = UndefFailedCond::Traps;
    WordUnaligned word_unaligned = WordUnaligned::IgnoreLowBits;

    int r15_reg_shift_read = 12;
    int r15_str_value = 12;
    int r15_stm_value = 12;
    ModeDelay mode_delay = ModeDelay::ReadsOldWritesNew;
    bool ldm_user_bank = true;
    LdmPcWb ldm_pc_wb = LdmPcWb::UserBank;
    ResetR14 reset_r14 = ResetR14::R15Minus4;

    int adx_r14 = 8;
    bool adx_ldm_first_only = true;
    AddressChecked adx_address_checked = AddressChecked::AddressUsed;
    bool adx_base_writeback = false;
    bool adx_store_to_load = true;
    LdmFaultLoads ldm_fault_loads = LdmFaultLoads::Complete;

    LdmCycles time_ldm_cycles = LdmCycles::Sw86;
    LdrRegShiftTime time_ldr_reg_shift = LdrRegShiftTime::OneS;
};

// Design, USER, build rules: the core holds no board facts; the board is behind this.
// `seq` is the SEQ pin (DOC, HW p.11): the board's timing depends on it. The
// address bus has 26 lines (DOC, SW p.1).
class ArmBus {
public:
    virtual ~ArmBus() = default;
    virtual uint32_t read_word(uint32_t addr, bool seq) = 0;
    virtual uint32_t read_byte(uint32_t addr, bool seq) = 0;
    virtual void write_word(uint32_t addr, uint32_t value, bool seq) = 0;
    virtual void write_byte(uint32_t addr, uint8_t value, bool seq) = 0;
    // Counts as an S cycle (DOC, SW p.32), so timing must see it.
    virtual void internal(uint32_t addr) = 0;

    // The ABORT pin. The core clears it after each memory cycle because the pin
    // applies to one bus cycle (DOC, SW p.28).
    bool abort_line = false;
    // abort strict (USER, 4 Nov 2026) starts at the first access in user mode. The mode bits
    // only: LDRT and STRT in a privileged mode do not set it (USER, 4 Nov 2026).
    bool user_mode = false;
};

class ArmCore {
public:
    static constexpr uint32_t kPcMask = 0x03FFFFFC;
    static constexpr uint32_t kI = 1u << 27, kF = 1u << 26;  // DOC, SW: I and F in R15's top bits.

    ArmCore(ArmBus& bus, const ArmOptions& options);

    void step();

    // Levels, not edges: DOC, HW "Reset" and "irq"; PIRQ and PNMI are direct (HW 5.3).
    void set_reset(bool active);
    void set_irq(bool active) { irq_line_ = active; }
    void set_fiq(bool active) { fiq_line_ = active; }

    // Design, notes item 28: the mode is a parameter so that the debugger sees every bank.
    uint32_t& reg_in_mode(int mode, int n);
    uint32_t r15() const { return r15_; }
    void set_r15(uint32_t value);

private:
    uint32_t pc() const { return r15_ & kPcMask; }
    uint32_t pc_from_instruction(int bytes) const;
    uint32_t r15_from_instruction(int bytes) const;
    uint32_t& reg(int n) { return regs_[bank_[n]]; }
    uint32_t first_cycle_reg(int n) const { return regs_[first_cycle_bank_[n]]; }
    void set_pc(uint32_t value) { r15_ = (r15_ & ~kPcMask) | (value & kPcMask); }
    void set_flag(uint32_t flag, bool on) { r15_ = on ? (r15_ | flag) : (r15_ & ~flag); }
    void use_banks_of_current_mode();
    void write_psr(uint32_t value, bool jumping);
    bool take_abort();
    void fetch_next_word();
    void refill_pipeline();
    void enter(uint32_t vector, int mode, bool mask_fiq, uint32_t link_pc);
    void undefined();
    bool condition(uint32_t cond) const;
    uint32_t shifted_rm(uint32_t ir, bool& carry);
    uint32_t rn_value(int rn, bool by_reg);
    void execute(uint32_t ir);
    void exec_data(uint32_t ir);
    void exec_single(uint32_t ir);
    void exec_block(uint32_t ir);
    void exec_branch(uint32_t ir);

    ArmBus& bus_;
    const ArmOptions opt_;

    uint32_t regs_[24] = {};
    // INF, DOC, SW p.1: the fetch address, not the R15 a program reads (pc_from_instruction).
    uint32_t r15_ = 0;
    uint32_t instruction_address_ = 0;

    // INF, profile cpu.mode_delay: two banks because it can make the first cycle of the instruction
    // after a mode change read a different bank from the one the rest of it uses.
    const uint8_t* bank_ = nullptr;
    const uint8_t* first_cycle_bank_ = nullptr;
    bool mode_just_changed_ = false;

    uint32_t pipe_[2] = {};
    bool pipe_aborted_[2] = {};
    bool next_fetch_sequential_ = true;

    bool reset_line_ = false;
    bool restart_pending_ = true;
    bool irq_line_ = false;
    bool fiq_line_ = false;
};
