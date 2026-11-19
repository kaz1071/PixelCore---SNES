#include "core/cpu/w65816.h"

#include "core/memory/snes_memory.h"

#include <cstdint>

namespace {
constexpr std::uint8_t Carry = 0x01;
constexpr std::uint8_t Zero = 0x02;
constexpr std::uint8_t InterruptDisable = 0x04;
constexpr std::uint8_t Decimal = 0x08;
constexpr std::uint8_t Index8 = 0x10;
constexpr std::uint8_t Accumulator8 = 0x20;
constexpr std::uint8_t Overflow = 0x40;
constexpr std::uint8_t Negative = 0x80;

void setNegativeZero(std::uint8_t& status, std::uint16_t value,
                     std::uint16_t mask, std::uint16_t sign)
{
    status = static_cast<std::uint8_t>(status & ~(Zero | Negative));
    const std::uint16_t result = value & mask;
    if (result == 0) {
        status = static_cast<std::uint8_t>(status | Zero);
    }
    if ((result & sign) != 0) {
        status = static_cast<std::uint8_t>(status | Negative);
    }
}
}

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

bool W65816::step(SnesMemory& memory, std::string& error)
{
    error.clear();
    if (registers_.stopped) {
        error = "65C816 is stopped";
        return false;
    }

    const std::uint32_t address =
        (static_cast<std::uint32_t>(registers_.pb) << 16U) | registers_.pc;
    const std::uint8_t opcode = fetch8(memory);
    const bool accumulator8 = (registers_.p & Accumulator8) != 0;
    const bool index8 = (registers_.p & Index8) != 0;
    const std::uint16_t accumulatorMask = accumulator8 ? 0x00FFU : 0xFFFFU;
    const std::uint16_t accumulatorSign = accumulator8 ? 0x0080U : 0x8000U;
    const std::uint16_t indexMask = index8 ? 0x00FFU : 0xFFFFU;
    const std::uint16_t indexSign = index8 ? 0x0080U : 0x8000U;

    switch (opcode) {
    case 0xEA: // NOP
        return true;

    case 0xA9: { // LDA immediate
        const std::uint16_t value = accumulator8 ? fetch8(memory) : fetch16(memory);
        if (accumulator8) {
            registers_.a = static_cast<std::uint16_t>((registers_.a & 0xFF00U) | value);
        } else {
            registers_.a = value;
        }
        setNegativeZero(registers_.p, registers_.a, accumulatorMask, accumulatorSign);
        return true;
    }

    case 0xA2: // LDX immediate
    case 0xA0: { // LDY immediate
        const std::uint16_t value = index8 ? fetch8(memory) : fetch16(memory);
        std::uint16_t& target = opcode == 0xA2 ? registers_.x : registers_.y;
        target = value;
        setNegativeZero(registers_.p, target, indexMask, indexSign);
        return true;
    }

    case 0x29: // AND immediate
    case 0x09: // ORA immediate
    case 0x49: { // EOR immediate
        const std::uint16_t operand = accumulator8 ? fetch8(memory) : fetch16(memory);
        const std::uint16_t value = registers_.a & accumulatorMask;
        std::uint16_t result = value;
        if (opcode == 0x29) {
            result = static_cast<std::uint16_t>(value & operand);
        } else if (opcode == 0x09) {
            result = static_cast<std::uint16_t>(value | operand);
        } else {
            result = static_cast<std::uint16_t>(value ^ operand);
        }
        registers_.a = accumulator8
            ? static_cast<std::uint16_t>((registers_.a & 0xFF00U) | result)
            : result;
        setNegativeZero(registers_.p, registers_.a, accumulatorMask, accumulatorSign);
        return true;
    }
    case 0x2C: { // BIT absolute
        const std::uint16_t address16 = fetch16(memory);
        const std::uint32_t bankBase = static_cast<std::uint32_t>(registers_.db) << 16U;
        std::uint16_t operand = memory.read8(bankBase | address16);
        if (!accumulator8) {
            const auto nextAddress = static_cast<std::uint16_t>(address16 + 1U);
            operand = static_cast<std::uint16_t>(
                operand | (static_cast<std::uint16_t>(memory.read8(bankBase | nextAddress)) << 8U)
            );
        }
        const std::uint16_t maskedOperand = operand & accumulatorMask;
        const std::uint8_t bitFlags = static_cast<std::uint8_t>(
            (maskedOperand & accumulatorSign ? Negative : 0U) |
            (maskedOperand & (accumulatorSign >> 1U) ? Overflow : 0U) |
            ((registers_.a & maskedOperand) == 0 ? Zero : 0U)
        );
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~(Negative | Overflow | Zero)) | bitFlags
        );
        return true;
    }

    case 0x3C: { // BIT absolute,X
        const std::uint16_t baseAddress = fetch16(memory);
        const std::uint16_t address16 = static_cast<std::uint16_t>(baseAddress + registers_.x);
        const std::uint32_t bankBase = static_cast<std::uint32_t>(registers_.db) << 16U;
        std::uint16_t operand = memory.read8(bankBase | address16);
        if (!accumulator8) {
            const auto nextAddress = static_cast<std::uint16_t>(address16 + 1U);
            operand = static_cast<std::uint16_t>(
                operand | (static_cast<std::uint16_t>(memory.read8(bankBase | nextAddress)) << 8U)
            );
        }
        const std::uint16_t maskedOperand = operand & accumulatorMask;
        const std::uint8_t bitFlags = static_cast<std::uint8_t>(
            (maskedOperand & accumulatorSign ? Negative : 0U) |
            (maskedOperand & (accumulatorSign >> 1U) ? Overflow : 0U) |
            ((registers_.a & maskedOperand) == 0 ? Zero : 0U)
        );
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~(Negative | Overflow | Zero)) | bitFlags
        );
        return true;
    }

    case 0x89: { // BIT immediate (65C816)
        const std::uint16_t operand = accumulator8 ? fetch8(memory) : fetch16(memory);
        const bool zero = (registers_.a & operand & accumulatorMask) == 0;
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~Zero) | (zero ? Zero : 0U)
        );
        return true;
    }

    case 0xC9: // CMP immediate
    case 0xE0: // CPX immediate
    case 0xC0: { // CPY immediate
        const bool accumulatorCompare = opcode == 0xC9;
        const std::uint16_t mask = accumulatorCompare ? accumulatorMask : indexMask;
        const std::uint16_t sign = accumulatorCompare ? accumulatorSign : indexSign;
        const std::uint16_t operand = accumulatorCompare
            ? (accumulator8 ? fetch8(memory) : fetch16(memory))
            : (index8 ? fetch8(memory) : fetch16(memory));
        const std::uint16_t value = accumulatorCompare
            ? static_cast<std::uint16_t>(registers_.a & mask)
            : (opcode == 0xE0 ? registers_.x : registers_.y);
        const std::uint16_t result = static_cast<std::uint16_t>((value - operand) & mask);
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~Carry) | (value >= operand ? Carry : 0U)
        );
        setNegativeZero(registers_.p, result, mask, sign);
        return true;
    }
    case 0x18: // CLC
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~Carry);
        return true;
    case 0x38: // SEC
        registers_.p = static_cast<std::uint8_t>(registers_.p | Carry);
        return true;
    case 0x58: // CLI
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~InterruptDisable);
        return true;
    case 0x78: // SEI
        registers_.p = static_cast<std::uint8_t>(registers_.p | InterruptDisable);
        return true;
    case 0xB8: // CLV
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~Overflow);
        return true;
    case 0xD8: // CLD
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~Decimal);
        return true;
    case 0xF8: // SED
        registers_.p = static_cast<std::uint8_t>(registers_.p | Decimal);
        return true;

    case 0xAA: // TAX
    case 0xA8: { // TAY
        const std::uint16_t value = accumulator8
            ? static_cast<std::uint16_t>(registers_.a & 0x00FFU)
            : registers_.a;
        std::uint16_t& target = opcode == 0xAA ? registers_.x : registers_.y;
        target = index8 ? static_cast<std::uint16_t>(value & 0x00FFU) : value;
        setNegativeZero(registers_.p, target, indexMask, indexSign);
        return true;
    }

    case 0x8A: // TXA
    case 0x98: { // TYA
        const std::uint16_t value = opcode == 0x8A ? registers_.x : registers_.y;
        if (accumulator8) {
            registers_.a = static_cast<std::uint16_t>(
                (registers_.a & 0xFF00U) | (value & 0x00FFU)
            );
        } else {
            registers_.a = value;
        }
        setNegativeZero(registers_.p, registers_.a, accumulatorMask, accumulatorSign);
        return true;
    }

    case 0xBA: // TSX
        registers_.x = index8
            ? static_cast<std::uint16_t>(registers_.s & 0x00FFU)
            : registers_.s;
        setNegativeZero(registers_.p, registers_.x, indexMask, indexSign);
        return true;

    case 0x9A: // TXS
        registers_.s = registers_.emulation
            ? static_cast<std::uint16_t>(0x0100U | (registers_.x & 0x00FFU))
            : registers_.x;
        return true;

    case 0xE8: // INX
    case 0xC8: // INY
    case 0xCA: // DEX
    case 0x88: { // DEY
        std::uint16_t& target =
            (opcode == 0xE8 || opcode == 0xCA) ? registers_.x : registers_.y;
        const bool decrement = opcode == 0xCA || opcode == 0x88;
        target = static_cast<std::uint16_t>(
            (target & indexMask) + (decrement ? 0xFFFFU : 1U)
        ) & indexMask;
        setNegativeZero(registers_.p, target, indexMask, indexSign);
        return true;
    }

    case 0x1A: // INC A
    case 0x3A: // DEC A
    case 0x0A: // ASL A
    case 0x4A: // LSR A
    case 0x2A: // ROL A
    case 0x6A: { // ROR A
        const std::uint16_t value = registers_.a & accumulatorMask;
        std::uint16_t result = value;
        switch (opcode) {
        case 0x1A:
            result = static_cast<std::uint16_t>((value + 1U) & accumulatorMask);
            break;
        case 0x3A:
            result = static_cast<std::uint16_t>((value - 1U) & accumulatorMask);
            break;
        case 0x0A:
            registers_.p = static_cast<std::uint8_t>(
                (registers_.p & ~Carry) | ((value & accumulatorSign) != 0 ? Carry : 0U)
            );
            result = static_cast<std::uint16_t>((value << 1U) & accumulatorMask);
            break;
        case 0x4A:
            registers_.p = static_cast<std::uint8_t>(
                (registers_.p & ~Carry) | ((value & 0x0001U) != 0 ? Carry : 0U)
            );
            result = static_cast<std::uint16_t>(value >> 1U);
            break;
        case 0x2A: {
            const bool carryIn = (registers_.p & Carry) != 0;
            registers_.p = static_cast<std::uint8_t>(
                (registers_.p & ~Carry) | ((value & accumulatorSign) != 0 ? Carry : 0U)
            );
            result = static_cast<std::uint16_t>(
                ((value << 1U) | (carryIn ? 1U : 0U)) & accumulatorMask
            );
            break;
        }
        case 0x6A: {
            const bool carryIn = (registers_.p & Carry) != 0;
            registers_.p = static_cast<std::uint8_t>(
                (registers_.p & ~Carry) | ((value & 0x0001U) != 0 ? Carry : 0U)
            );
            result = static_cast<std::uint16_t>(
                (value >> 1U) | (carryIn ? accumulatorSign : 0U)
            );
            break;
        }
        default:
            break;
        }
        registers_.a = accumulator8
            ? static_cast<std::uint16_t>((registers_.a & 0xFF00U) | result)
            : result;
        setNegativeZero(registers_.p, registers_.a, accumulatorMask, accumulatorSign);
        return true;
    }

    case 0xC2: { // REP
        const std::uint8_t mask = fetch8(memory);
        registers_.p = static_cast<std::uint8_t>(registers_.p & ~mask);
        if (registers_.emulation) {
            registers_.p = static_cast<std::uint8_t>(registers_.p | Accumulator8 | Index8);
        }
        if ((registers_.p & Index8) != 0) {
            registers_.x = static_cast<std::uint16_t>(registers_.x & 0x00FFU);
            registers_.y = static_cast<std::uint16_t>(registers_.y & 0x00FFU);
        }
        return true;
    }

    case 0xE2: { // SEP
        const std::uint8_t mask = fetch8(memory);
        registers_.p = static_cast<std::uint8_t>(registers_.p | mask);
        if ((registers_.p & Index8) != 0) {
            registers_.x = static_cast<std::uint16_t>(registers_.x & 0x00FFU);
            registers_.y = static_cast<std::uint16_t>(registers_.y & 0x00FFU);
        }
        return true;
    }

    case 0xFB: { // XCE
        const bool oldEmulation = registers_.emulation;
        registers_.emulation = (registers_.p & Carry) != 0;
        registers_.p = static_cast<std::uint8_t>(
            (registers_.p & ~Carry) | (oldEmulation ? Carry : 0U)
        );
        if (registers_.emulation) {
            registers_.p = static_cast<std::uint8_t>(registers_.p | Accumulator8 | Index8);
            registers_.s = static_cast<std::uint16_t>(0x0100U | (registers_.s & 0x00FFU));
            registers_.x = static_cast<std::uint16_t>(registers_.x & 0x00FFU);
            registers_.y = static_cast<std::uint16_t>(registers_.y & 0x00FFU);
        }
        return true;
    }

    case 0x80: // BRA
    case 0x10: // BPL
    case 0x30: // BMI
    case 0x50: // BVC
    case 0x70: // BVS
    case 0x90: // BCC
    case 0xB0: // BCS
    case 0xD0: // BNE
    case 0xF0: { // BEQ
        const auto displacement = static_cast<std::int8_t>(fetch8(memory));
        bool take = false;
        switch (opcode) {
        case 0x80: take = true; break;
        case 0x10: take = (registers_.p & Negative) == 0; break;
        case 0x30: take = (registers_.p & Negative) != 0; break;
        case 0x50: take = (registers_.p & Overflow) == 0; break;
        case 0x70: take = (registers_.p & Overflow) != 0; break;
        case 0x90: take = (registers_.p & Carry) == 0; break;
        case 0xB0: take = (registers_.p & Carry) != 0; break;
        case 0xD0: take = (registers_.p & Zero) == 0; break;
        case 0xF0: take = (registers_.p & Zero) != 0; break;
        default: break;
        }
        if (take) {
            registers_.pc = static_cast<std::uint16_t>(registers_.pc + displacement);
        }
        return true;
    }

    case 0xAD: // LDA absolute
    case 0xAE: // LDX absolute
    case 0xAC: { // LDY absolute
        const std::uint16_t address16 = fetch16(memory);
        const std::uint32_t effectiveAddress =
            (static_cast<std::uint32_t>(registers_.db) << 16U) | address16;
        std::uint16_t value = memory.read8(effectiveAddress);
        if ((opcode == 0xAD && !accumulator8) ||
            (opcode != 0xAD && !index8)) {
            value = static_cast<std::uint16_t>(
                value | (static_cast<std::uint16_t>(memory.read8(effectiveAddress + 1U)) << 8U)
            );
        }

        if (opcode == 0xAD) {
            if (accumulator8) {
                registers_.a = static_cast<std::uint16_t>(
                    (registers_.a & 0xFF00U) | value
                );
            } else {
                registers_.a = value;
            }
            setNegativeZero(registers_.p, registers_.a, accumulatorMask, accumulatorSign);
        } else {
            std::uint16_t& target = opcode == 0xAE ? registers_.x : registers_.y;
            target = value;
            setNegativeZero(registers_.p, target, indexMask, indexSign);
        }
        return true;
    }

    case 0x8D: // STA absolute
    case 0x8E: // STX absolute
    case 0x8C: { // STY absolute
        const std::uint16_t address16 = fetch16(memory);
        const std::uint32_t effectiveAddress =
            (static_cast<std::uint32_t>(registers_.db) << 16U) | address16;
        const bool accumulatorStore = opcode == 0x8D;
        const std::uint16_t value = opcode == 0x8D
            ? registers_.a
            : (opcode == 0x8E ? registers_.x : registers_.y);
        const bool narrow = accumulatorStore ? accumulator8 : index8;
        memory.write8(effectiveAddress, static_cast<std::uint8_t>(value & 0x00FFU));
        if (!narrow) {
            memory.write8(effectiveAddress + 1U, static_cast<std::uint8_t>(value >> 8U));
        }
        return true;
    }

    case 0xDB: // STP
        registers_.stopped = true;
        return true;

    default:
        error = "Unsupported 65C816 opcode $";
        {
            constexpr char digits[] = "0123456789ABCDEF";
            error.push_back(digits[(opcode >> 4U) & 0x0FU]);
            error.push_back(digits[opcode & 0x0FU]);
            error += " at $";
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
        registers_.p = static_cast<std::uint8_t>(registers_.p | Accumulator8 | Index8);
        registers_.s = static_cast<std::uint16_t>(0x0100U | (registers_.s & 0x00FFU));
    }
    if ((registers_.p & Index8) != 0) {
        registers_.x = static_cast<std::uint16_t>(registers_.x & 0x00FFU);
        registers_.y = static_cast<std::uint16_t>(registers_.y & 0x00FFU);
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
