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
// AesTiming.cpp, cycle costs and refresh, in crystal ticks
// version 1.0.0, 17 November 2026.

// Board time in crystal ticks: what each memory cycle and internal cycle costs, with the
// 32-word block break, refresh, and the ROM and Tube wait states.

#include "AesTiming.hpp"

// DOC, SW p.33, SW85 p.21: S 150 ns, N 300 ns, at 50 ns a tick.
const int kS = 3, kN = 6;
// INF, item 48: the PAL clock is M8REF, taken as crystal / 3.
const int kPalClock = 3;
// DOC, 2 MHz host: 10 crystal ticks to a host cycle.
const int kHostCycle = 10;
const uint32_t kBlockWordBits = 0x7C;  // A2-A6: the word within a 32-word block.

namespace {

bool is_ram(AesTiming::Place place) {
    return place == AesTiming::Place::Ram || place == AesTiming::Place::RamCard;
}

}  // namespace

AesTiming::AesTiming(const AesOptions& options)
    : opt_(options), next_refresh_(options.time_refresh_period) {}

// INF from DOC, HW 5.1.4 RFADD, with RT = RSTLATCH, item 13: a pending refresh starts only
// when not stretching, so at the cycle's end here, and not on a sequential RAM cycle unless
// at a block end. It stops the clock (DOC, HW IC9). The request counter runs on M8REF, so
// a late refresh does not move the next (INF, item 12).
void AesTiming::pass(int cost, bool refresh_can_start) {
    ticks_ += cost;
    if (refresh_can_start && ticks_ >= next_refresh_) {
        ticks_ += opt_.time_refresh_cost;
        while (next_refresh_ <= ticks_) next_refresh_ += opt_.time_refresh_period;
    }
}

void AesTiming::memory_cycle(bool seq, Place place, uint32_t addr) {
    // time.links (INF): a tied SEQ leaves no sequential cycles.
    if (opt_.time_links == AesOptions::Links::SeqTied) seq = false;
    const bool ram = is_ram(place);
    // time.block_break (INF from DOC, IC9): A2-A6 back to 0 starts a new 32-word block.
    const bool new_block = seq && (addr & kBlockWordBits) == 0;
    const bool breaks = new_block && opt_.time_block_break == AesOptions::BlockBreak::CostsN;

    // INF from DOC, HW 5.1.4: every fast term of PHIEN needs /A24*/A25, so only RAM has an
    // S path; ROM, Tube and empty cycles cost N. INF, 7a: N, not S, under the wait is a guess.
    int cost = (ram && seq && !breaks) ? kS : kN;
    // INF, 7a: WAITB stretches the cycle (DOC, HW p.34), so the wait adds to its cost.
    if (place == Place::Rom) cost += opt_.time_rom_wait * kPalClock;
    if (place == Place::Tube) cost += opt_.time_tube_wait * kPalClock;
    if (place == Place::RamCard) cost += opt_.time_ram_bank;
    pass(cost, !ram || !seq || new_block);

    // time.tube_sync (INF): none by default, as no host clock reaches the parasite side.
    if (place == Place::Tube && opt_.time_tube_sync == AesOptions::TubeSync::NextHostEdge) {
        ticks_ += (kHostCycle - ticks_ % kHostCycle) % kHostCycle;
    }
}

// DOC, SW p.32: costs as S. DOC, HW 4.3: SEQ stays high with no memory use, and IC9 has no
// internal-cycle term in RFADD. Its block end is A2-A6 all 1 (INF, 7a: the same word as
// the pins show; the sequential case above waits for the crossing).
void AesTiming::internal_cycle(Place place, uint32_t addr) {
    const bool seq = opt_.time_links != AesOptions::Links::SeqTied;
    const bool block_end = (addr & kBlockWordBits) == kBlockWordBits;
    pass(kS, !is_ram(place) || !seq || block_end);
}
