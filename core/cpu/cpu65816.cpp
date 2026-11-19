#include "core/cpu/cpu65816.h"

#include "core/memory/snes_memory.h"

#include <cstdint>

void Cpu65816::reset(const SnesMemory& memory) noexcept {
    registers_ = Registers{};
    const std::uint16_t vector = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(memory.read8(0x00FFFCU)) |
        (static_cast<std::uint16_t>(memory.read8(0x00FFFDU)) << 8U));
    registers_.pc = vector;
}

std::uint8_t Cpu65816::fetch8(SnesMemory& memory) noexcept {
    const std::uint32_t address =
        (static_cast<std::uint32_t>(registers_.pb) << 16U) | registers_.pc;
    const std::uint8_t value = memory.read8(address);
    registers_.pc = static_cast<std::uint16_t>(registers_.pc + 1U);
    return value;
}

std::uint16_t Cpu65816::fetch16(SnesMemory& memory) noexcept {
    const std::uint16_t low = fetch8(memory);
    const std::uint16_t high = fetch8(memory);
    return static_cast<std::uint16_t>(low | (high << 8U));
}

void Cpu65816::setNz8(std::uint8_t value) noexcept {
    registers_.p = static_cast<std::uint8_t>(registers_.p & ~(Zero | Negative));
    if (value == 0U) {
        registers_.p = static_cast<std::uint8_t>(registers_.p | Zero);
    }
    if ((value & 0x80U) != 0U) {
        registers_.p = static_cast<std::uint8_t>(registers_.p | Negative);
    }
}

void Cpu65816::setNz16(std::uint16_t value) noexcept {
    registers_.p = static_cast<std::uint8_t>(registers_.p & ~(Zero | Negative));
    if (value == 0U) {
        registers_.p = static_cast<std::uint8_t>(registers_.p | Zero);
    }
    if ((value & 0x8000U) != 0U) {
        registers_.p = static_cast<std::uint8_t>(registers_.p | Negative);
    }
}

std::uint8_t Cpu65816::step(SnesMemory& memory, std::string& error) noexcept {
    error.clear();
    const std::uint8_t opcode = fetch8(memory);

    switch (opcode) {
    case 0xEA: // NOP
        return 2;

    case 0xA9: // LDA immediate
        if (flag(Accumulator8)) {
            const std::uint8_t value = fetch8(memory);
            registers_.a = static_cast<std::uint16_t>((registers_.a & 0xFF00U) | value);
            setNz8(value);
            return 2;
        } else {
            registers_.a = fetch16(memory);
            setNz16(registers_.a);
            return 3;
        }

    case 0xA2: // LDX immediate
        if (flag(Index8)) {
            const std::uint8_t value = fetch8(memory);
            registers_.x = value;
            setNz8(value);
            return 2;
        } else {
            registers_.x = fetch16(memory);
            setNz16(registers_.x);
            return 3;
        }

    case 0xA0: // LDY immediate
        if (flag(Index8)) {
            const std::uint8_t value = fetch8(memory);
            registers_.y = value;
            setNz8(value);
            return 2;
        } else {
            registers_.y = fetch16(memory);
            setNz16(registers_.y);
            return 3;
        }

    case 0x8D: { // STA absolute
        const std::uint16_t address = fetch16(memory);
        const std::uint32_t fullAddress =
            (static_cast<std::uint32_t>(registers_.db) << 16U) | address;
        memory.write8(fullAddress, static_cast<std::uint8_t>(registers_.a & 0x00FFU));
        if (!flag(Accumulator8)) {
            memory.write8((fullAddress + 1U) & 0x00FFFFFFU,
                          static_cast<std::uint8_t>(registers_.a >> 8U));
        }
        return 4;
    }

    case 0xC2: { // REP
        const std::uint8_t mask = fetch8(memory);
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~mask);
        if (registers_.emulation) {
            registers_.p = static_cast<std::uint8_t>(registers_.p | Accumulator8 | Index8);
        }
        return 3;
    }

    case 0xE2: { // SEP
        const std::uint8_t mask = fetch8(memory);
        registers_.p = static_cast<std::uint8_t>(registers_.p | mask);
        return 3;
    }

    case 0x4C: // JMP absolute
        registers_.pc = fetch16(memory);
        return 3;

    default:
        error = "Unsupported 65C816 opcode: " + std::to_string(opcode);
        return 0;
    }
}

const Cpu65816::Registers& Cpu65816::registers() const noexcept {
    return registers_;
}

bool Cpu65816::flag(StatusFlag flagValue) const noexcept {
    return (registers_.p & static_cast<std::uint8_t>(flagValue)) != 0U;
}
