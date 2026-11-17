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
// TubeLog.hpp, the counting wrapper, a test build only
// version 1.0.0, 17 November 2026.

// The counting wrapper: stands between the core and the board, and between the board and the
// Tube, in a test build only (USER, 2 Nov 2026; compiled in by AES_TUBE_LOG). Records in order
// every access the ARM makes in region 01: the address the ARM issued and the register it
// reached, so a mapping fault cannot hide itself (USER, 1 Nov 2026). Changes nothing it passes.
// Each row: tick, R or W, address, register (0-7, or - for none), value, how many in a row.
// Written to a file row by row, so it cannot overrun as Beebium's ring does. Last row: end, count.
#pragma once

#include "../AesMemory.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

class TubeLog : public ArmBus, public TubePort {
public:
    static std::string path_from_environment() {
        const char* path = std::getenv("AES_TUBE_LOG");
        return path ? path : "";
    }

    TubeLog(const std::string& path, AesTiming& timing)
        : timing_(timing), file_(path.empty() ? nullptr : std::fopen(path.c_str(), "w")) {}
    ~TubeLog() override { close(); }

    void connect(TubePort& tube, ArmBus& memory) {
        tube_ = &tube;
        memory_ = &memory;
    }

    void close() {
        if (!file_) return;
        write_row();
        std::fprintf(file_, "end %llu\n", static_cast<unsigned long long>(total_));
        std::fclose(file_);
        file_ = nullptr;
    }

    uint32_t read_word(uint32_t addr, bool seq) override {
        begin(addr);
        const uint32_t value = memory_->read_word(addr, seq);
        finish(addr, 'R', value);
        return value;
    }
    uint32_t read_byte(uint32_t addr, bool seq) override {
        begin(addr);
        const uint32_t value = memory_->read_byte(addr, seq);
        finish(addr, 'R', value);
        return value;
    }
    void write_word(uint32_t addr, uint32_t value, bool seq) override {
        begin(addr);
        memory_->write_word(addr, value, seq);
        finish(addr, 'W', value);
    }
    void write_byte(uint32_t addr, uint8_t value, bool seq) override {
        begin(addr);
        memory_->write_byte(addr, value, seq);
        finish(addr, 'W', value);
    }
    void internal(uint32_t addr) override {
        memory_->internal(addr);
        take_abort();
    }

    uint8_t read(int reg) override {
        const uint8_t value = tube_->read(reg);
        note('R', address_, reg, value);
        reached_ = true;
        return value;
    }
    void write(int reg, uint8_t value) override {
        tube_->write(reg, value);
        note('W', address_, reg, value);
        reached_ = true;
    }

private:
    struct Row {
        char direction = 0;
        uint32_t address = 0;
        int reg = -1;
        uint8_t value = 0;
        uint64_t tick = 0, count = 0;
    };

    void begin(uint32_t addr) {
        address_ = addr;
        reached_ = false;
    }
    void finish(uint32_t addr, char direction, uint32_t value) {
        if (in_tube(addr) && !reached_) note(direction, addr, -1, uint8_t(value));
        take_abort();
    }
    // The core reads and clears the pin on the bus it holds, which here is this object.
    void take_abort() {
        abort_line = memory_->abort_line;
        memory_->abort_line = false;
    }
    void note(char direction, uint32_t addr, int reg, uint8_t value) {
        total_++;
        if (row_.count && row_.direction == direction && row_.address == addr && row_.reg == reg &&
            row_.value == value) {
            row_.count++;
            return;
        }
        write_row();
        row_ = Row{direction, addr, reg, value, timing_.ticks(), 1};
    }
    void write_row() {
        if (!file_ || !row_.count) return;
        char reg[4] = "-";
        if (row_.reg >= 0) std::snprintf(reg, sizeof reg, "%d", row_.reg);
        std::fprintf(file_, "%llu %c %X %s %02X %llu\n", static_cast<unsigned long long>(row_.tick),
                     row_.direction, row_.address, reg, row_.value,
                     static_cast<unsigned long long>(row_.count));
        std::fflush(file_);  // A server stopped with a signal loses nothing already closed.
        row_ = Row{};
    }

    AesTiming& timing_;
    std::FILE* file_;
    TubePort* tube_ = nullptr;
    ArmBus* memory_ = nullptr;
    uint32_t address_ = 0;
    bool reached_ = false;
    Row row_;
    uint64_t total_ = 0;
};
