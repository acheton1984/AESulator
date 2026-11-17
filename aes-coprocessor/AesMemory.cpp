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
// AesMemory.cpp, the board's memory map, ROM swap and Tube addresses
// version 1.0.0, 17 November 2026.

// The AES memory map behind ArmBus: RAM, ROM, the Tube addresses, region 10 and the ROM
// swap at reset. Each memory cycle is passed to AesTiming to be costed.
//
// The map, as docs/about-aesulator.md gives it:
//   &0000000  RAM, repeating every 4 MB (INF). At reset the ROM also appears here, until the
//             first write of any kind (DOC).
//   &1000000  the Tube's eight registers, repeating every 32 bytes up to &17FFFFF (INF).
//   &2000000  nothing answers (INF).
//   &3000000  the 16 KB Executive ROM, repeating every 16 KB (INF).
// A read of a missing RAM bank returns &FFFFFFFF; what the real board returns is UNK.

#include "AesMemory.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <random>

namespace {

const uint32_t kMb = 0x100000;
const uint32_t kAddress = 0x03FFFFFF;
const uint32_t kRegion = 0x00FFFFFF;
const uint32_t kUpperHalf = 0x00800000;
const uint32_t kTube = 0x01000000, kTubeBytes = 32;  // SRC, Kernel:79, 101-108.
const uint32_t kRom = 0x03000000;  // SRC, Kernel:84.
const uint32_t kNothing = 0xFFFFFFFF;  // UNK: arbitrary, no document gives this value.

int tube_register(uint32_t addr) { return (addr >> 2) & 7; }  // DOC, HW 5.3: A2-A4.

// DOC, HW p.34: four accesses build a ROM word; SRC, the reset vector sets the byte order.
uint32_t word_at(const std::vector<uint8_t>& bytes, uint32_t at) {
    return bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 | uint32_t(bytes[at + 3]) << 24;
}

}  // namespace

AesMemory::AesMemory(const AesOptions& options, AesTiming& timing, TubePort& tube)
    : opt_(options), timing_(timing), tube_(tube), ram_(4 * kMb), rom_(kRomBytes) {
    // USER, Plugin option 3.
    const int mb = opt_.ram;
    if (mb != 2 && mb != 4) throw std::invalid_argument("ram: 2 or 4");
    fitted_bytes_ = uint32_t(mb) * kMb;
    power_on();
}

bool AesMemory::load_rom() { return load_rom(opt_.rom); }

bool AesMemory::load_rom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<uint8_t> image{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (image.size() != kRomBytes) return false;
    rom_ = image;
    return true;
}

void AesMemory::power_on() {
    // ram.power_on (INF; USER: zero is a convenience, real DRAM powers up random). A fixed
    // seed (INF, 7a), so that two runs can be compared.
    if (opt_.ram_power_on == AesOptions::RamPowerOn::Random) {
        std::mt19937 random(1);
        for (uint8_t& byte : ram_) byte = uint8_t(random());
    } else {
        std::fill(ram_.begin(), ram_.end(), 0);
    }
    reset();
}

// abort strict is lax again after every reset (USER, 4 Nov 2026).
void AesMemory::reset() {
    boot_latch_ = true;
    strict_on_ = false;
}

// ram.repeat (INF, HW 5.1.2): A20, A21 pick the bank; A22, A23 are ignored. Machine.ram
// fits whole banks from bank 0 (USER; INF for 1 MB).
AesMemory::Target AesMemory::ram_target(uint32_t addr) const {
    if (opt_.ram_repeat == AesOptions::RamRepeat::FittedSize) {
        return {Kind::Ram, (addr & kRegion) % fitted_bytes_, true};
    }
    const uint32_t offset = addr & (4 * kMb - 1);
    if (offset < fitted_bytes_) return {Kind::Ram, offset, true};
    const bool repeats = opt_.ram_absent_bank == AesOptions::RamAbsentBank::LowRam;
    return {Kind::Ram, repeats ? offset % fitted_bytes_ : offset, false};
}

// rom.repeat (INF from DOC: the ROM has A2-A13 only, and RCS only A24, A25).
AesMemory::Target AesMemory::rom_target(uint32_t addr) const {
    const uint32_t offset = addr & kRegion;
    if (opt_.rom_repeat == AesOptions::RomRepeat::Every16K) return {Kind::Rom, offset & (kRomBytes - 1), true};
    return {Kind::Rom, offset, offset < kRomBytes};
}

// abort strict (USER, 31 Oct 2026): a what-if, not a claim about the board. Only
// installed RAM, the Tube's 32 bytes and the 16 KB ROM exist; every repeat is empty.
bool AesMemory::outside_strict_map(uint32_t addr) const {
    const bool in_ram = addr < fitted_bytes_;
    const bool in_tube_regs = addr >= kTube && addr < kTube + kTubeBytes;
    const bool in_rom = addr >= kRom && addr < kRom + kRomBytes;
    return !in_ram && !in_tube_regs && !in_rom;
}

// DOC, HW 5.1.5: A25, A24 pick the region.
AesMemory::Target AesMemory::decode(uint32_t addr, bool for_write) const {
    addr &= kAddress;
    if (strict_on_ && outside_strict_map(addr)) {
        return {Kind::Empty, addr, false};  // INF, 7a: nothing is stored or read there.
    }
    switch (addr >> 24) {
        case 0:
            // boot.latch (DOC, HW pp.34-35): low addresses read the ROM until the first
            // write. UNK, 7a: that write reaches RAM here, a stand-in: RAS and CAS need RT
            // (HW 5.1.4), and whether it lands is a race the equations don't settle.
            return boot_latch_ && !for_write ? rom_target(addr) : ram_target(addr);
        case 1: {
            // tube.addr_decode (INF, HW 5.3: chip select on A23). The strobes ignore A23,
            // so the upper half still takes the Tube wait.
            const bool lower = !(addr & kUpperHalf);
            return {Kind::Tube, addr, lower || opt_.tube_addr_decode == AesOptions::TubeAddrDecode::WholeRegion};
        }
        case 2: {
            // region10.read, .write (INF from DOC, HW 5.1.4: RAS and CAS need /A24*/A25; the
            // abort is a conflict: UTIL 2.3, Brazil p.4). The repeats are what-ifs.
            const bool repeats = for_write
                                     ? opt_.region10_write == AesOptions::Region10Write::RamRepeat
                                     : opt_.region10_read == AesOptions::Region10Read::RamRepeat;
            return repeats ? ram_target(addr) : Target{Kind::Region10, addr, false};
        }
        default:
            return rom_target(addr);
    }
}

AesTiming::Place AesMemory::place_of(const Target& target, uint32_t addr) const {
    using Place = AesTiming::Place;
    // INF, IC18 "do not wait for RAM" (/A24*/RSTLATB) needs the latch clear, so with A24
    // low a cycle under the latch waits as ROM, region 10 and writes included. UNK, 7a:
    // the clearing write's full wait is probably too long.
    if (boot_latch_ && !(addr & kA24)) return Place::Rom;
    // DOC low-res: banks 2 and 3 are on the card.
    if (target.kind == Kind::Ram) return target.offset >= 2 * kMb ? Place::RamCard : Place::Ram;
    if (target.kind == Kind::Rom) return Place::Rom;
    if (target.kind == Kind::Tube) return Place::Tube;
    return Place::Nothing;
}

void AesMemory::note_cycle(const Target& target, uint32_t addr, bool seq, bool for_write) {
    timing_.memory_cycle(seq, place_of(target, addr), addr);

    // abort strict, checking on: an access outside its map (USER, 4 Nov 2026; empty removed, USER, 5 Nov 2026).
    if (target.kind == Kind::Empty) abort_line = true;
    // region10.read Abort. USER, 31 Oct 2026: abort off means no ABORT at all.
    if (!for_write && target.kind == Kind::Region10 &&
        opt_.region10_read == AesOptions::Region10Read::Abort && !abort_tied()) abort_line = true;
}

// abort strict (USER, 4 Nov 2026): lax until the first access in user mode, so the boot and
// the Executive's RAM probe, all in privileged modes (SRC, ROM &1824-&18C0), pass.
void AesMemory::watch_mode() {
    if (user_mode && opt_.abort == AesOptions::MachineAbort::Strict) strict_on_ = true;
}

AesMemory::Target AesMemory::start_read(uint32_t addr, bool seq) {
    watch_mode();
    const Target target = decode(addr, false);
    note_cycle(target, addr, seq, false);
    return target;
}

// UNK: what a line nothing drives reads. One value everywhere: empty reads and Tube lanes.
uint32_t AesMemory::undriven() const {
    return opt_.ram_absent_bank == AesOptions::RamAbsentBank::LastBus ? last_bus_ : kNothing;
}

// DOC, sheet: the Tube is on D0-D7 only, so the other lanes are undriven.
uint32_t AesMemory::tube_lanes(uint32_t addr) {
    return (undriven() & ~0xFFu) | tube_.read(tube_register(addr));
}

uint32_t AesMemory::read_lanes(const Target& target) const {
    const uint32_t at = target.offset & ~3u;
    if (target.kind == Kind::Ram && target.fitted) return word_at(ram_, at);
    if (target.kind == Kind::Rom && target.fitted) return word_at(rom_, at);
    // ram.absent_bank, used for all that nothing answers: an unfitted bank, region 10, the
    // Tube's upper half, past the ROM. INF, 7a: LowRam repeats RAM only for an unfitted bank.
    if (target.kind == Kind::Ram && opt_.ram_absent_bank == AesOptions::RamAbsentBank::LowRam) {
        return word_at(ram_, at);
    }
    return undriven();
}

uint32_t AesMemory::read_word(uint32_t addr, bool seq) {
    const Target target = start_read(addr, seq);
    uint32_t value;
    if (target.kind == Kind::Tube) suspect();
    if (target.kind == Kind::Tube && target.fitted) {
        // tube.width (UNK).
        const uint32_t lanes = tube_lanes(addr);
        value = opt_.tube_width == AesOptions::TubeWidth::LowByte ? lanes : undriven();
    } else {
        value = read_lanes(target);
    }
    last_bus_ = value;
    return value;
}

uint32_t AesMemory::read_byte(uint32_t addr, bool seq) {
    const Target target = start_read(addr, seq);
    // INF, 7a: A0, A1 are not wired to the Tube, so the register is still read, but the ARM
    // takes the lane they select (DOC, HW 3.4), and only lane 0 is driven.
    const uint32_t word = target.kind == Kind::Tube && target.fitted ? tube_lanes(addr) : read_lanes(target);
    last_bus_ = word;
    return (word >> (8 * (target.offset & 3))) & 0xFF;
}

// boot.latch: any write, Tube and ROM included (DOC, HW p.35: the equation has no address
// term). RamWritesOnly is a what-if.
void AesMemory::note_write(const Target& target) {
    if (opt_.boot_latch == AesOptions::BootLatch::AnyWrite || target.kind == Kind::Ram) {
        boot_latch_ = false;
    }
    if (target.kind == Kind::Rom) suspect();
}

AesMemory::Target AesMemory::start_write(uint32_t addr, bool seq) {
    watch_mode();
    const Target target = decode(addr, true);
    note_cycle(target, addr, seq, true);
    note_write(target);
    return target;
}

void AesMemory::write_word(uint32_t addr, uint32_t value, bool seq) {
    const Target target = start_write(addr, seq);
    if (target.kind == Kind::Tube) {
        suspect();
        // tube.width (UNK).
        const bool low_byte = opt_.tube_width == AesOptions::TubeWidth::LowByte;
        if (target.fitted) tube_.write(tube_register(addr), uint8_t(low_byte ? value : undriven()));
    } else if (target.kind == Kind::Ram && target.fitted) {
        // ram.absent_write (INF): nothing answers, so an unfitted bank keeps nothing.
        const uint32_t at = target.offset & ~3u;
        for (int i = 0; i < 4; i++) ram_[at + i] = uint8_t(value >> (8 * i));
    }
    last_bus_ = value;
}

void AesMemory::write_byte(uint32_t addr, uint8_t value, bool seq) {
    const Target target = start_write(addr, seq);
    if (target.kind == Kind::Tube) {
        if (target.fitted) tube_.write(tube_register(addr), value);
    } else if (target.kind == Kind::Ram && target.fitted) {
        ram_[target.offset] = value;
    }
    last_bus_ = value * 0x01010101u;  // DOC, HW 3.4: a byte is on all four lanes.
}

void AesMemory::internal(uint32_t addr) {
    timing_.internal_cycle(place_of(decode(addr, false), addr), addr);
}

// access.check (provisional design, Plugin option 5): ROM writes and Tube width. The
// abort needs abort strict (USER, 31 Oct 2026; empty removed, USER, 5 Nov 2026).
void AesMemory::suspect() {
    if (opt_.access_check == AesOptions::MachineCheck::Off) return;
    suspect_accesses_++;
    if (opt_.access_check == AesOptions::MachineCheck::Abort && !abort_tied()) abort_line = true;
}

uint8_t AesMemory::peek(uint32_t addr) const {
    const Target target = decode(addr, false);
    if (target.kind == Kind::Tube && target.fitted) return uint8_t(kNothing);
    return (read_lanes(target) >> (8 * (target.offset & 3))) & 0xFF;
}
