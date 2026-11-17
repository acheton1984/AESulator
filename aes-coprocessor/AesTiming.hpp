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
// AesTiming.hpp, the board timing interface
// version 1.0.0, 17 November 2026.

// Board time in crystal ticks, 50 ns each (DOC, SW p.33; USER: 20 MHz).
#pragma once

#include "AesOptions.hpp"

#include <cstdint>

extern const int kS, kN, kPalClock, kHostCycle;

class AesTiming {
public:
    enum class Place { Ram, RamCard, Rom, Tube, Nothing };

    explicit AesTiming(const AesOptions& options);

    void memory_cycle(bool seq, Place place, uint32_t addr);
    void internal_cycle(Place place, uint32_t addr);
    uint64_t ticks() const { return ticks_; }

private:
    void pass(int cost, bool refresh_can_start);

    const AesOptions opt_;
    uint64_t ticks_ = 0;
    uint64_t next_refresh_;
};
