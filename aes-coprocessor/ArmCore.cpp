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
// ArmCore.cpp, the ARM1 processor core
// version 1.0.0, 17 November 2026.

// The ARM1 core: runs each instruction and takes each exception, reaching memory only
// through ArmBus. Its uncertain behaviours are the switches in ArmOptions (ArmCore.hpp).

#include "ArmCore.hpp"

#include <bit>

namespace {

const uint32_t kFlagN = 1u << 31, kFlagZ = 1u << 30, kFlagC = 1u << 29, kFlagV = 1u << 28;
const uint32_t kUserPsr = 0xF0000000, kAllPsr = 0xFC000003;
const uint32_t kBadAddress = 0xFC000000, kAddress = 0x03FFFFFF;
const uint32_t kWordAddress = kAddress & ~3u;
const int kUser = 0, kFiq = 1, kIrq = 2, kSvc = 3;
const uint32_t kVecReset = 0x00, kVecUndefined = 0x04, kVecSwi = 0x08, kVecPrefetchAbort = 0x0C;
const uint32_t kVecDataAbort = 0x10, kVecAddress = 0x14, kVecIrq = 0x18, kVecFiq = 0x1C;

// DOC, SW p.5: 25 registers. Slots 0-14 user, 15-19 FIQ R10-14, 20-21 IRQ R13-14,
// 22-23 SVC R13-14, and R15 held apart.
const uint8_t kBank[4][15] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 20, 21},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 22, 23},
};

uint32_t ror32(uint32_t v, unsigned n) { return std::rotr(v, int(n)); }
bool bit(uint32_t v, unsigned n) { return (v >> n) & 1; }

}  // namespace

ArmCore::ArmCore(ArmBus& bus, const ArmOptions& options) : bus_(bus), opt_(options) {
    // UNK, 7a: register contents at power-on. They read 0 here.
    r15_ = kI | kF | kSvc;
    use_banks_of_current_mode();
}

uint32_t& ArmCore::reg_in_mode(int mode, int n) { return regs_[kBank[mode][n]]; }

void ArmCore::set_r15(uint32_t value) {
    r15_ = value;
    restart_pending_ = false;
    mode_just_changed_ = false;
    use_banks_of_current_mode();
    refill_pipeline();
}

void ArmCore::set_reset(bool active) {
    if (reset_line_ && !active) restart_pending_ = true;
    reset_line_ = active;
}

// R15 reads are stated relative to the instruction (DOC, SW p.1: 8 bytes on), and
// the instruction's address does not move when the instruction jumps.
uint32_t ArmCore::pc_from_instruction(int bytes) const {
    return (instruction_address_ + bytes) & kPcMask;
}
uint32_t ArmCore::r15_from_instruction(int bytes) const {
    return (r15_ & ~kPcMask) | pc_from_instruction(bytes);
}

void ArmCore::use_banks_of_current_mode() {
    bus_.user_mode = (r15_ & 3) == kUser;
    bank_ = kBank[r15_ & 3];
    first_cycle_bank_ = bank_;
}

// DOC, SW p.3: user mode can change only the flags. cpu.mode_delay: the source writes
// banked registers straight after TEQP (SRC, Super1:156-157, Kernel:983-1000,
// Middle:545-547) but puts a no-op before reads (SRC, Middle:111-113, Kernel:922-924).
void ArmCore::write_psr(uint32_t value, bool jumping) {
    const int old_mode = r15_ & 3;
    const uint32_t mask = old_mode == kUser ? kUserPsr : kAllPsr;
    r15_ = (r15_ & ~mask) | (value & mask);
    bus_.user_mode = (r15_ & 3) == kUser;
    if (int(r15_ & 3) == old_mode) return;

    // INF, 7a: with a jump the new mode is in force at the target, because the
    // refill outlasts the one-cycle delay of SW85 p.5.
    if (jumping || opt_.mode_delay == ArmOptions::ModeDelay::None) {
        mode_just_changed_ = false;
        use_banks_of_current_mode();
        return;
    }
    mode_just_changed_ = true;
    if (opt_.mode_delay == ArmOptions::ModeDelay::ReadsOldWritesNew) {
        first_cycle_bank_ = bank_;
        bank_ = kBank[r15_ & 3];
    }
}

bool ArmCore::take_abort() {
    const bool aborted = bus_.abort_line;
    bus_.abort_line = false;
    return aborted;
}

void ArmCore::fetch_next_word() {
    pipe_[0] = pipe_[1];
    pipe_aborted_[0] = pipe_aborted_[1];
    pipe_[1] = bus_.read_word(pc(), next_fetch_sequential_);
    pipe_aborted_[1] = take_abort();
    set_pc(pc() + 4);
    next_fetch_sequential_ = true;
}

// DOC, SW p.32: cycles N, S to refill, then S for the next instruction's own first cycle:
// B and R15time. Order INF, DS87 ch.9, adapted.
void ArmCore::refill_pipeline() {
    next_fetch_sequential_ = false;
    fetch_next_word();
    fetch_next_word();
}

void ArmCore::enter(uint32_t vector, int mode, bool mask_fiq, uint32_t link_pc) {
    regs_[kBank[mode][14]] = (r15_ & ~kPcMask) | (link_pc & kPcMask);
    r15_ = (r15_ & ~(kPcMask | 3)) | kI | (mask_fiq ? kF : 0) | vector | uint32_t(mode);
    mode_just_changed_ = false;
    use_banks_of_current_mode();
    refill_pipeline();
}

// R14 = instruction + 4 (INF, SW p.30: return is a plain transfer to R15; SRC, fpe.s:107-109).
void ArmCore::undefined() { enter(kVecUndefined, kSvc, false, pc_from_instruction(4)); }

bool ArmCore::condition(uint32_t cond) const {
    const bool n = r15_ & kFlagN, z = r15_ & kFlagZ, c = r15_ & kFlagC, v = r15_ & kFlagV;
    switch (cond) {
        case 0: return z;
        case 1: return !z;
        case 2: return c;
        case 3: return !c;
        case 4: return n;
        case 5: return !n;
        case 6: return v;
        case 7: return !v;
        case 8: return c && !z;
        case 9: return !c || z;
        case 10: return n == v;
        case 11: return n != v;
        case 12: return n == v && !z;
        case 13: return n != v || z;
        case 14: return true;
        default: return false;
    }
}

void ArmCore::step() {
    if (reset_line_) {
        // DOC, HW "Reset": dummy fetches, the address climbing from where reset began.
        bus_.read_word(pc(), true);
        take_abort();
        set_pc(pc() + 4);
        return;
    }
    if (restart_pending_) {
        restart_pending_ = false;
        // cpu.reset_r14 (INF): SW p.28 prints "R4", taken as a slip for R14.
        const bool minus_4 = opt_.reset_r14 == ArmOptions::ResetR14::R15Minus4;
        enter(kVecReset, kSvc, true, minus_4 ? pc() - 4 : pc());
        return;
    }

    // The word two ahead is being fetched as this instruction starts (DOC, SW p.1).
    instruction_address_ = pc() - 8;
    const uint32_t ir = pipe_[0];
    const bool prefetch_aborted = pipe_aborted_[0];
    fetch_next_word();

    // DOC, SW pp.29-30: levels checked at the end of each instruction, FIQ first.
    // INF, 7a: entry costs as SWI, and the synchroniser delay is left out.
    // Instruction + 4 makes SUBS PC,R14,#4 return here (SRC, Kernel:187, 892).
    if (fiq_line_ && !(r15_ & kF)) {
        enter(kVecFiq, kFiq, true, pc_from_instruction(4));
    } else if (irq_line_ && !(r15_ & kI)) {
        enter(kVecIrq, kIrq, false, pc_from_instruction(4));
    } else if (prefetch_aborted) {
        // DOC, SW p.29, then "subtract 4" to re-execute. INF, 7a: condition not consulted.
        enter(kVecPrefetchAbort, kSvc, false, pc_from_instruction(4));
    } else {
        execute(ir);
    }

    // INF, 7a: cpu.mode_delay covers one instruction, the one after the mode change.
    if (mode_just_changed_) {
        mode_just_changed_ = false;
    } else {
        use_banks_of_current_mode();
    }
}

void ArmCore::execute(uint32_t ir) {
    const uint32_t top = (ir >> 24) & 0xF;
    const bool is_undefined = top >= 0xC && top != 0xF;
    // Q3, cpu.undef_failed_cond (DOC, DS13).
    if (is_undefined && opt_.undef_failed_cond == ArmOptions::UndefFailedCond::Traps) {
        undefined();
        return;
    }
    if (!condition(ir >> 28)) return;

    switch (top >> 1) {
        case 0: case 1: exec_data(ir); break;
        case 2: case 3: exec_single(ir); break;
        case 4: exec_block(ir); break;
        case 5: exec_branch(ir); break;
        default:
            if (is_undefined) {
                undefined();
            } else {
                // DOC, SW p.25: the PC adjusted to the word after the SWI.
                enter(kVecSwi, kSvc, false, pc_from_instruction(4));
            }
            break;
    }
}

void ArmCore::exec_branch(uint32_t ir) {
    // DOC, SW p.7: next instruction plus PSR.
    if (bit(ir, 24)) reg(14) = r15_from_instruction(4);
    const int32_t words = int32_t(ir << 8) >> 8;
    set_pc(pc_from_instruction(8) + uint32_t(words) * 4);
    refill_pipeline();
}

// Operand 2 from a register, for data processing and LDR/STR alike (DOC, SW p.19).
// INF, 7a: so R15 as an LDR or STR offset carries the PSR, as it does as Rm.
uint32_t ArmCore::shifted_rm(uint32_t ir, bool& carry) {
    const int rm = ir & 15;
    const int type = (ir >> 5) & 3;
    const bool by_reg = bit(ir, 4);
    // r15.reg_shift_read (INF, DS87 6.3.6, adapted). A register shift reads Rs in
    // its first cycle and Rm in its second (INF, 7a, DS87 9.2, adapted).
    uint32_t v;
    if (rm == 15) {
        v = r15_from_instruction(by_reg ? opt_.r15_reg_shift_read : 8);
    } else {
        v = by_reg ? reg(rm) : first_cycle_reg(rm);
    }

    unsigned n;
    if (!by_reg) {
        n = (ir >> 7) & 31;
        if (n == 0) {
            if (type == 0) return v;  // DOC, SW p.14: LSL #0 leaves value and carry.
            if (type == 3) {
                // Q1, cpu.rrx_carry (DOC, SW p.18, SW85 p.12, DS13).
                const uint32_t in = carry ? 0x80000000 : 0;
                carry = opt_.rrx_carry == ArmOptions::RrxCarry::Bit0OrBit31
                            ? bit(v, 0) || bit(v, 31)
                            : bit(v, 0);
                return in | (v >> 1);
            }
            n = 32;  // DOC, SW p.13: LSR and ASR by 0 mean 32.
        }
    } else {
        const int rs = (ir >> 8) & 15;
        // INF, 7a, DS87 6.3.6, adapted: R15 as Rs is the PC alone, 8 ahead.
        n = (rs == 15 ? pc_from_instruction(8) : first_cycle_reg(rs)) & 0xFF;
        if (n == 0) {
            // cpu.reg_shift_zero_carry (DOC, SW pp.14-15: the carry is not altered).
            if (opt_.reg_shift_zero_carry == ArmOptions::RegShiftZeroCarry::Cleared) carry = false;
            return v;
        }
    }
    // DOC, SW pp.14-15: one table for both; register amounts reach 32 and over.
    switch (type) {
        case 0:
            carry = n < 32 ? bit(v, 32 - n) : (n == 32 && bit(v, 0));
            return n < 32 ? v << n : 0;
        case 1:
            carry = n < 32 ? bit(v, n - 1) : (n == 32 && bit(v, 31));
            return n < 32 ? v >> n : 0;
        case 2:
            carry = n < 32 ? bit(v, n - 1) : bit(v, 31);
            return uint32_t(int32_t(v) >> (n < 32 ? n : 31));
        default: {
            const unsigned r = n & 31;
            if (n < 32) {
                carry = bit(v, n - 1);
            } else {
                // Q2, cpu.ror_reg_carry (DOC, SW p.18, against DS13 and DS87 6.3.3: item 50).
                switch (opt_.ror_reg_carry) {
                    case ArmOptions::RorRegCarry::ZeroAbove31:
                        carry = false;
                        break;
                    case ArmOptions::RorRegCarry::ZeroAtMultiplesOf32:
                        carry = r != 0 && bit(v, r - 1);
                        break;
                    case ArmOptions::RorRegCarry::Arm2:
                        carry = bit(v, r == 0 ? 31 : r - 1);
                        break;
                }
            }
            return ror32(v, r);
        }
    }
}

// Rn, or a base: as either, R15 is the PC alone (DOC, SW pp.17, 21).
uint32_t ArmCore::rn_value(int rn, bool by_reg) {
    if (rn == 15) return pc_from_instruction(by_reg ? opt_.r15_reg_shift_read : 8);
    return by_reg ? reg(rn) : first_cycle_reg(rn);
}

void ArmCore::exec_data(uint32_t ir) {
    const int op = (ir >> 21) & 15;
    const int rn = (ir >> 16) & 15;
    const int rd = (ir >> 12) & 15;
    const bool set = bit(ir, 20);
    const bool is_compare = op >= 8 && op <= 11;
    const bool old_c = r15_ & kFlagC;
    bool shifter_c = old_c;
    bool by_reg = false;
    uint32_t op2;

    if (bit(ir, 25)) {
        const unsigned rot = ((ir >> 8) & 15) * 2;
        op2 = ror32(ir & 0xFF, rot);
        // INF, 7a, SW pp.11, 15: as a rotate, C is the last bit into bit 31.
        if (rot != 0) shifter_c = bit(op2, 31);
    } else {
        by_reg = bit(ir, 4);
        // Family A, cpu.enc_a_bit7 (INF, SW85 p.10 prints bit 7 as x; SW p.14 prints 0).
        if (by_reg && bit(ir, 7) && opt_.enc_a_bit7 == ArmOptions::EncABit7::Traps) {
            undefined();
            return;
        }
        op2 = shifted_rm(ir, shifter_c);
        if (by_reg) bus_.internal(pc_from_instruction(12));  // DOC, SW p.14: one extra cycle.
    }
    // Family B, cpu.enc_b_rd: flags unchanged is DOC, SW p.12; the rest UNK.
    if (is_compare && !set && opt_.enc_b_rd == ArmOptions::EncBRd::Trap) {
        undefined();
        return;
    }

    // DOC, SW p.17: as Rn, R15 is the PC alone. Offset and cycle as in shifted_rm.
    const uint32_t op1 = rn_value(rn, by_reg);
    // One sum serves every arithmetic operation because C is not-borrow (DOC, SW p.3)
    // and SBC and RSC borrow by adding the carry (DOC, SW p.12).
    uint32_t a = op1, b = op2;
    bool carry_in = false;
    bool arithmetic = true;
    uint32_t result = 0;
    switch (op) {
        case 2: case 10: b = ~op2; carry_in = true; break;
        case 3: a = op2; b = ~op1; carry_in = true; break;
        case 4: case 11: break;
        case 5: carry_in = old_c; break;
        case 6: b = ~op2; carry_in = old_c; break;
        case 7: a = op2; b = ~op1; carry_in = old_c; break;
        case 0: case 8: result = op1 & op2; arithmetic = false; break;
        case 1: case 9: result = op1 ^ op2; arithmetic = false; break;
        case 12: result = op1 | op2; arithmetic = false; break;
        case 13: result = op2; arithmetic = false; break;
        case 14: result = op1 & ~op2; arithmetic = false; break;
        default: result = ~op2; arithmetic = false; break;
    }
    const uint64_t wide = uint64_t(a) + b + carry_in;
    if (arithmetic) result = uint32_t(wide);

    if (is_compare && !set) {
        if (opt_.enc_b_rd == ArmOptions::EncBRd::WritesRd) {
            if (rd != 15) {
                reg(rd) = result;
            } else {
                set_pc(result);
                refill_pipeline();
            }
        }
        return;
    }
    if (rd == 15) {
        if (is_compare) {
            // DOC, SW p.17: the instruction behind this is already fetched, so no jump.
            write_psr(result, false);
            return;
        }
        if (set) write_psr(result, true);
        set_pc(result);  // DOC, SW p.17: S clear writes the 24 PC bits only.
        refill_pipeline();
        return;
    }
    if (!is_compare) reg(rd) = result;
    if (set) {
        set_flag(kFlagN, bit(result, 31));
        set_flag(kFlagZ, result == 0);
        if (arithmetic) {
            set_flag(kFlagC, wide >> 32);
            set_flag(kFlagV, bit((a ^ result) & (b ^ result), 31));
        } else {
            set_flag(kFlagC, shifter_c);  // DOC, SW p.12: V unaffected.
        }
    }
}

void ArmCore::exec_single(uint32_t ir) {
    const bool has_rm = bit(ir, 25), pre = bit(ir, 24), up = bit(ir, 23), is_byte = bit(ir, 22);
    const bool wb_bit = bit(ir, 21), load = bit(ir, 20);
    const int rn = (ir >> 16) & 15;
    const int rd = (ir >> 12) & 15;
    const bool by_reg = has_rm && bit(ir, 4);
    // INF, 7a: bit 21 in the post-indexed forms is T, which needs nothing here because
    // the TRANS pin is not connected (DOC, low-res).

    // Q4, cpu.ldr_reg_shift (DOC, DS13, SW85 p.21). Bit 7 follows cpu.enc_a_bit7
    // (INF, 7a: SW p.19 says these are the same shift bits).
    const bool bit7_traps = bit(ir, 7) && opt_.enc_a_bit7 == ArmOptions::EncABit7::Traps;
    if (by_reg && (opt_.ldr_reg_shift == ArmOptions::LdrRegShift::Traps || bit7_traps)) {
        undefined();
        return;
    }
    bool carry_for_rrx = r15_ & kFlagC;
    const uint32_t offset = has_rm ? shifted_rm(ir, carry_for_rrx) : ir & 0xFFF;
    if (by_reg && opt_.time_ldr_reg_shift == ArmOptions::LdrRegShiftTime::OneS) {
        bus_.internal(pc_from_instruction(12));
    }

    // DOC, SW p.21: as a base, R15 is the PC alone. INF, 7a: +12 as for data processing.
    const uint32_t base = rn_value(rn, by_reg);
    const uint32_t moved = up ? base + offset : base - offset;
    const uint32_t address = pre ? moved : base;
    const bool writeback = !pre || wb_bit;  // DOC, SW p.21: post-indexed always writes back.
    const uint32_t bus_address = is_byte ? address & kAddress : address & kWordAddress;
    const auto read_bus = [&] {
        return is_byte ? bus_.read_byte(bus_address, false) : bus_.read_word(bus_address, false);
    };

    // r15.str_value (INF, DS87 6.5.4, adapted). The data is read in the second cycle
    // (INF, DS87 9.5, adapted), so after a mode change it comes from the new bank.
    const uint32_t store_value =
        load ? 0 : (rd == 15 ? r15_from_instruction(opt_.r15_str_value) : reg(rd));
    const auto write_bus = [&] {
        is_byte ? bus_.write_byte(bus_address, uint8_t(store_value), false)
                : bus_.write_word(bus_address, store_value, false);
    };

    const bool check_used = opt_.adx_address_checked == ArmOptions::AddressChecked::AddressUsed;
    if ((check_used ? address : base) & kBadAddress) {
        // adx.store_to_load (INF, DS87 5.3.3, adapted). INF, 7a: the bus cycle still runs.
        if (load || opt_.adx_store_to_load) {
            read_bus();
        } else {
            write_bus();
        }
        take_abort();
        // adx.base_writeback (DOC, SW 3.2(1): the state just before execution).
        if (writeback && opt_.adx_base_writeback && rn != 15) reg(rn) = moved;
        enter(kVecAddress, kSvc, false, pc_from_instruction(opt_.adx_r14));
        return;
    }

    // Write the base before a load lands, so the load wins (INF, 7a, DS87 9.4, adapted),
    // and after a store's data is read, so the old base goes out (INF, 7a, DS87 9.5, adapted).
    bool pc_changed = false;
    const auto write_base = [&] {
        if (!writeback) return;
        if (rn == 15) {
            set_pc(moved);  // INF, 7a, DS87 9.4, adapted; the PSR stays (DOC, SW p.21).
            pc_changed = true;
        } else {
            reg(rn) = moved;
        }
    };
    if (load) {
        uint32_t value = read_bus();
        // cpu.word_unaligned: UNK by default; the alternative is INF, DS87 6.5.3, adapted.
        if (!is_byte && opt_.word_unaligned == ArmOptions::WordUnaligned::Rotate) {
            value = ror32(value, (address & 3) * 8);
        }
        if (take_abort()) {  // DOC, SW p.29: as though not executed.
            enter(kVecDataAbort, kSvc, false, pc_from_instruction(8));
            return;
        }
        bus_.internal(pc_from_instruction(12));
        write_base();
        if (rd == 15) {
            set_pc(value);  // DOC, SW p.21: never the PSR.
            pc_changed = true;
        } else {
            reg(rd) = value;
        }
    } else {
        write_bus();
        if (take_abort()) {
            enter(kVecDataAbort, kSvc, false, pc_from_instruction(8));
            return;
        }
        write_base();
    }

    if (pc_changed) {
        refill_pipeline();
    } else {
        next_fetch_sequential_ = load;  // STR ends in N, LDR in S (DOC, SW p.32).
    }
}

void ArmCore::exec_block(uint32_t ir) {
    const bool pre = bit(ir, 24), up = bit(ir, 23), psr_bit = bit(ir, 22);
    const bool load = bit(ir, 20);
    const int rn = (ir >> 16) & 15;
    const uint32_t list = ir & 0xFFFF;
    const bool writeback = bit(ir, 21) && rn != 15;  // DOC, SW p.24.
    const int count = std::popcount(list);
    // UNK, 7a: SW p.24 says an empty list is undefined. Here nothing happens.
    if (count == 0) return;

    // INF, 7a, DS87 6.6.4, adapted: in user mode the bit is ignored for LDM too.
    const int start_mode = r15_ & 3;
    const bool privileged = start_mode != kUser;
    // DOC, SW85 p.16 for STM. cpu.ldm_user_bank (INF, DS87 6.6.4, adapted) for LDM without R15.
    const bool user_bank = privileged && psr_bit &&
                           (load ? !bit(list, 15) && opt_.ldm_user_bank : true);
    // Q6, cpu.ldm_pc_wb (INF, hedged: DS13 "most probably", extended from SW85 p.16).
    const bool pc_only_user_bank = load && privileged && psr_bit && list == 0x8000 && rn != 15 &&
                    kBank[start_mode][rn] != rn &&
                    opt_.ldm_pc_wb == ArmOptions::LdmPcWb::UserBank;
    // INF, 7a: LDM forcing the user bank writes back as STM does.
    const auto write_back = [&](uint32_t value) {
        ((user_bank || pc_only_user_bank) ? regs_[kBank[kUser][rn]] : reg(rn)) = value;
    };

    // DOC, SW p.24: with R15 as base the PSR bits join the address. INF, 7a: 8 ahead.
    const uint32_t base = rn == 15 ? r15_from_instruction(8) : first_cycle_reg(rn);
    const uint32_t lowest = up ? base : base - 4u * count;
    const uint32_t new_base = up ? base + 4u * count : lowest;
    // DOC, SW85 p.15: a block always climbs from its lowest address. "Up before" and
    // "down after" skip the first word of that range.
    uint32_t address = lowest + (pre == up ? 4 : 0);

    // adx.ldm_first_only: DOC, SW 3.2: only the first transfer is checked.
    bool address_fault = (address & kBadAddress) != 0;
    bool data_abort = false;
    if (!load) {
        bool first = true;
        for (int i = 0; i < 16; i++) {
            if (!bit(list, i)) continue;
            if (!opt_.adx_ldm_first_only && (address & kBadAddress)) address_fault = true;
            const uint32_t at = address & kWordAddress;
            uint32_t value;
            if (i == 15) {
                value = r15_from_instruction(opt_.r15_stm_value);  // INF, DS87 6.6.3, adapted.
            } else {
                value = user_bank ? regs_[kBank[kUser][i]] : reg(i);
            }
            if (address_fault && opt_.adx_store_to_load) {
                bus_.read_word(at, !first);  // INF, DS87 6.6.7, adapted: no write.
            } else {
                bus_.write_word(at, value, !first);
            }
            if (take_abort()) data_abort = true;
            address += 4;
            // DOC, SW p.24: first register goes out, then the base is written back.
            if (first && writeback) write_back(new_base);
            first = false;
        }
        next_fetch_sequential_ = false;  // DOC, SW p.32: the 2 N of STM.
    } else {
        const bool sw85 = opt_.time_ldm_cycles == ArmOptions::LdmCycles::Sw85;
        // ldm.fault_loads. Complete: DOC, SW 3.2(1), p.29. StopOverwriting: INF, DS87
        // 6.6.7, 6.6.8, adapted; there the load before the fault is lost too, so each
        // load waits for the next cycle before it lands.
        const bool complete = opt_.ldm_fault_loads == ArmOptions::LdmFaultLoads::Complete;
        bool stopped = address_fault && !complete;
        int waiting = -1;
        uint32_t waiting_value = 0;
        uint32_t loaded_r15 = 0;
        bool r15_loaded = false;
        const auto land = [&](int i, uint32_t value) {
            if (i == 15) {
                loaded_r15 = value;
                r15_loaded = true;
            } else if (user_bank) {
                regs_[kBank[kUser][i]] = value;
            } else {
                reg(i) = value;
            }
        };

        bool first = true;
        for (int i = 0; i < 16; i++) {
            if (!bit(list, i)) continue;
            if (!opt_.adx_ldm_first_only && (address & kBadAddress)) {
                address_fault = true;
                stopped = !complete;
            }
            const uint32_t at = address & kWordAddress;
            // time.ldm_cycles: SW p.32 has no N here and SW85 p.21 has one (conflict C2).
            // INF, 7a: the default puts its first data cycle as S.
            const uint32_t value = bus_.read_word(at, !(first && sw85));
            if (take_abort()) {
                stopped = !complete;
                data_abort = true;
            }
            address += 4;
            if (first && writeback) write_back(new_base);
            first = false;
            if (!stopped) {
                if (waiting >= 0) land(waiting, waiting_value);
                waiting = i;
                waiting_value = value;
            }
        }
        if (!stopped && waiting >= 0) land(waiting, waiting_value);
        if (sw85) bus_.internal(pc_from_instruction(12));

        const bool faulted = address_fault || data_abort;
        if (faulted && rn != 15) {
            // DOC, SW p.29: the base is always updated, even over a load. The restore
            // without writeback belongs to the alternative (INF, DS87 6.6.8, adapted).
            if (writeback) {
                write_back(new_base);
            } else if (!complete) {
                reg(rn) = base;
            }
        }

        // DOC, SW p.40: a fault leaves the PC preserved.
        if (r15_loaded && !faulted) {
            // DOC, SW85 p.15: PSR only with the bit set, as far as the mode allows.
            if (psr_bit) write_psr(loaded_r15, true);
            set_pc(loaded_r15);
            refill_pipeline();
        }
    }

    if (address_fault) {
        enter(kVecAddress, kSvc, false, pc_from_instruction(opt_.adx_r14));
    } else if (data_abort) {
        enter(kVecDataAbort, kSvc, false, pc_from_instruction(8));  // DOC, SW p.29: "subtract 8".
    }
}
