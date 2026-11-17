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
// AesExtension.hpp, the extension's interface
// version 1.0.0, 17 November 2026.

// The AES as a Beebium extension: owns the Tube chip and the board while attached.
#pragma once

#include "AesTube.hpp"

#include <beebium/extension/CoprocessorExtension.hpp>
#include <beebium/tube/TubeSocket.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>

class AesExtension : public beebium::CoprocessorExtension {
public:
    ~AesExtension() override { shutdown(); }

    std::span<const std::string_view> attaches_to() const override {
        static constexpr std::string_view points[] = {"tube"};
        return points;
    }
    std::span<const std::string_view> provides() const override { return {}; }

    void init(beebium::ExtensionContext& context) override;
    void shutdown() override;

    beebium::Coprocessor* coprocessor() override { return tube_.get(); }
    beebium::TubeHostBackend* tube_backend() override { return ula_.get(); }
    std::string cpu_name() const override { return "ARM1"; }  // USER; DOC, HW.
    uint64_t clock_hz() const override;

private:
    AesOptions options_from_settings() const;

    std::unique_ptr<beebium::TubeUla> ula_;
    std::unique_ptr<AesTube> tube_;
    beebium::TubeSocket* socket_ = nullptr;  // Beebium's; not ours to free.
};
