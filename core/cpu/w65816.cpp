#include "core/cpu/w65816.h"

#include "core/memory/snes_memory.h"

void W65816::reset(const SnesMemory& memory)
{
    registers_ = Registers{};
    registers_.emulation = true;
    registers_.p = 0x34;
    registers_.s = 0x01FF;
    registers_.pb = 0;
    registers_.pc = static_cast<std::uint16_t>(
        memory.read8(0x00FFFC) |
        (static_cast<std::uint16_t>(memory.read8(0x00FFFD)) << 8U)
    );
}

bool W65816::step(const SnesMemory& memory, std::string& error)
{
    if (registers_.stopped) {
        error = "65C816 is stopped";
        return false;
    }

    const std::uint32_t address =
        (static_cast<std::uint32_t>(registers_.pb) << 16U) | registers_.pc;
    const std::uint8_t opcode = fetch8(memory);

    switch (opcode) {
    case 0xEA: // NOP
        return true;

    case 0xA9: { // LDA immediate
        if ((registers_.p & 0x20U) != 0) {
            registers_.a = static_cast<std::uint16_t>(
                (registers_.a & 0xFF00U) | fetch8(memory)
            );
        } else {
            registers_.a = fetch16(memory);
        }
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~0x82U) |
            ((registers_.a & ((registers_.p & 0x20U) ? 0x00FFU : 0xFFFFU)) == 0 ? 0x02U : 0U) |
            ((registers_.a & ((registers_.p & 0x20U) ? 0x0080U : 0x8000U)) != 0 ? 0x80U : 0U)
        );
        return true;
    }

    case 0xA2: // LDX immediate
    case 0xA0: { // LDY immediate
        const std::uint16_t value = (registers_.p & 0x10U) != 0
            ? fetch8(memory)
            : fetch16(memory);
        std::uint16_t& target = opcode == 0xA2 ? registers_.x : registers_.y;
        if ((registers_.p & 0x10U) != 0) {
            target = static_cast<std::uint16_t>((target & 0xFF00U) | value);
        } else {
            target = value;
        }
        const std::uint16_t mask = (registers_.p & 0x10U) != 0 ? 0x00FFU : 0xFFFFU;
        const std::uint16_t sign = (registers_.p & 0x10U) != 0 ? 0x0080U : 0x8000U;
        const std::uint16_t result = target & mask;
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~0x82U) |
            (result == 0 ? 0x02U : 0U) |
            ((result & sign) != 0 ? 0x80U : 0U)
        );
        return true;
    }

    case 0xDB: // STP
        registers_.stopped = true;
        return true;

    default:
        error = "Unsupported 65C816 opcode $" ;
        {
            constexpr char digits[] = "0123456789ABCDEF";
            error.push_back(digits[(opcode >> 4U) & 0x0FU]);
            error.push_back(digits[opcode & 0x0FU]);
            error += " at $" ;
            for (int shift = 20; shift >= 0; shift -= 4) {
                error.push_back(digits[(address >> shift) & 0x0FU]);
            }
        }
        return false;
    }
}

const W65816::Registers& W65816::registers() const
{
    return registers_;
}

void W65816::setRegisters(const Registers& registers)
{
    registers_ = registers;
    if (registers_.emulation) {
        registers_.p = static_cast<std::uint8_t>(registers_.p | 0x30U);
        registers_.s = static_cast<std::uint16_t>(0x0100U | (registers_.s & 0x00FFU));
    }
}

std::uint8_t W65816::fetch8(const SnesMemory& memory)
{
    const std::uint32_t address =
        (static_cast<std::uint32_t>(registers_.pb) << 16U) | registers_.pc;
    const std::uint8_t value = memory.read8(address);
    registers_.pc = static_cast<std::uint16_t>(registers_.pc + 1U);
    return value;
}

std::uint16_t W65816::fetch16(const SnesMemory& memory)
{
    const std::uint8_t low = fetch8(memory);
    const std::uint8_t high = fetch8(memory);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8U));
}
