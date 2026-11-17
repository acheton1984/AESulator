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
// AesTube.cpp, runs the ARM against the host's clock, through the Tube
// version 1.0.0, 17 November 2026.

// Runs the ARM against the host's clock: the core and the board catch up to Beebium's time,
// with Beebium's Tube chip between them and the BBC.

#include "AesTube.hpp"

#include <bit>

namespace {

// Ticks a host cycle: kHostCycle, whose one home (with its DOC) is AesTiming.cpp.
const beebium::ClockRatio kTicksPerHostCycle{uint32_t(kHostCycle), 1};
// DOC, SW p.33: 3 ticks is the S cycle. The label only: nothing outside the 6502 runner
// reads the rest (SRC, Beebium's source), and our timing is AesTiming.
const beebium::BoardTiming kLabel{kTicksPerHostCycle, 3, 3, 0, 1};
const int kTubeSyncMost = kHostCycle - 1;  // time.tube_sync can add up to a host cycle less one.
const int kLoadOffsetReach = 4096;  // DOC, SW p.19: a 12-bit immediate offset.

}  // namespace

AesTube::AesTube(const AesOptions& options, beebium::TubeUla& ula)
    : opt_(options),
      ula_(ula),
      timing_(opt_),
#ifdef AES_TUBE_LOG
      log_(TubeLog::path_from_environment(), timing_),
      memory_(opt_, timing_, log_),
      core_(log_, opt_.cpu),
#else
      memory_(opt_, timing_, *this),
      core_(memory_, opt_.cpu),
#endif
      clock_(kTicksPerHostCycle) {
#ifdef AES_TUBE_LOG
    log_.connect(*this, memory_);
#endif
}

const beebium::BoardTiming& AesTube::board_label() { return kLabel; }

// DOC, Beebium contract: Reset. RAM stays (INF: DRAM keeps its data), and the budget starts
// afresh, because a carried overshoot belongs to the program that has gone.
void AesTube::reset() {
    restart();
    clock_.rebase();
    prst_was_active_ = false;
    ticks_due_ = timing_.ticks();
}

void AesTube::restart() {
    core_.set_reset(true);
    memory_.reset();
    core_.set_reset(false);
}

// tube.prst_level (DOC rough scan, HW 5.3: pin 37 to ARM RST through IC5, a level; INF that
// it holds). P is the only PRST Beebium shows (SRC, TubeUla.hpp). Holding stops the core
// rather than running its climbing reset fetches: 7a, those fetches could reach the Tube.
bool AesTube::held_by_prst() {
    const bool active = ula_.coprocessor_reset_active();
    const bool ended = prst_was_active_ && !active;
    prst_was_active_ = active;
    if (ended) restart();
    return active && opt_.tube_prst_level == AesOptions::TubePrstLevel::Holds;
}

bool AesTube::interrupt_will_be_taken() const {
    const uint32_t psr = core_.r15();
    return (ula_.pnmi_level() && !(psr & ArmCore::kF)) || (ula_.pirq() && !(psr & ArmCore::kI));
}

// The most a Tube instruction at `at` normally costs: its fetch, each word priced at the Tube,
// its last cycle and one refresh per period of that. Design choice, no document (profile 8a):
// the prices are AesTiming's. Only a Tube word has to fit,
// so a ROM word, dearer than a Tube word, needs no price of its own. Not counted, both listed in
// 8a: a refill after the access (R15 loaded or written back, or an exception entered), and
// refreshes held back through sequential cycles, which can end a long block late.
int AesTube::tube_instruction_cost(uint32_t at, int words) const {
    const bool rom = ((at >> 24) & 3) == 3 || (memory_.boot_latch_set() && !(at & AesMemory::kA24));
    const int fetch = kN + (rom ? opt_.time_rom_wait * kPalClock : opt_.time_ram_bank);
    const bool sync = opt_.time_tube_sync == AesOptions::TubeSync::NextHostEdge;
    const int word = kN + opt_.time_tube_wait * kPalClock + (sync ? kTubeSyncMost : 0);
    const int cost = fetch + words * word + kS;
    return cost + (1 + cost / opt_.time_refresh_period) * opt_.time_refresh_cost;
}

// Design, item 27 (DOC, Beebium guide section 6, contract Step 3b): an instruction that
// touches the Tube must run only if it ends inside the time due, or the host would see an
// access from its future. The core can't be stopped part-way, so the batch stops before it.
// The core stays as it is (USER, 1 Nov 2026), so the next instruction is read here. The test
// errs towards waiting: a wait loses no time, because what is due carries over.
// INF, 7a: a word rewritten after its fetch is judged as memory holds it, not as the pipeline does.
bool AesTube::must_wait_for_tube() {
    const uint32_t at = (core_.r15() & ArmCore::kPcMask) - 8;
    const uint32_t top = memory_.peek(at + 3);
    const bool single = ((top >> 2) & 3) == 1;
    const bool block = ((top >> 1) & 7) == 4;
    if (!single && !block) return false;

    const uint32_t low = memory_.peek(at) | memory_.peek(at + 1) << 8 | memory_.peek(at + 2) << 16;
    const int words = block ? std::popcount(low & 0xFFFF) : 1;
    // DOC, SW85 p.21, SW p.32: an offset shifted by a register takes one more S cycle.
    const int shift = single && (top & 2) && (low & 0x10) ? kS : 0;
    if (int64_t(ticks_due_ - timing_.ticks()) >= tube_instruction_cost(at, words) + shift) return false;

    if (single && (top & 2)) return true;  // A register offset could reach anywhere.
    const int rn = (low >> 16) & 15;
    // DOC, SW p.1: R15 reads 8 ahead of the instruction.
    const uint32_t base = rn == 15 ? at + 8 : core_.reg_in_mode(core_.r15() & 3, rn);
    return in_tube(base) || in_tube(base - kLoadOffsetReach) || in_tube(base + kLoadOffsetReach);
}

// DOC, Beebium guide section 6: a budget in ticks. What is due is counted from the host's
// time and what is spent from the board's, so neither drifts, and an overshoot of one
// instruction comes off the next batch. Time paused is lost (DOC, contract, Pause); time held
// by PRST is lost too, a design choice.
void AesTube::run_until(uint64_t host_cycle) {
    const uint64_t due = clock_.cycles_due(host_cycle);
    if (paused_) return;
    if (held_by_prst()) return;
    ticks_due_ += due;
    while (timing_.ticks() < ticks_due_) {
        // tube.irq_wiring (INF from SRC; DOC low-res): levels, sampled once an instruction.
        core_.set_irq(ula_.pirq());
        core_.set_fiq(ula_.pnmi_level());
        if (!interrupt_will_be_taken() && must_wait_for_tube()) break;
        core_.step();
    }
}
