#pragma once

#include <cstdint>
#include <string>

class SnesMemory;

class Cpu65816 {
public:
    struct Registers {
        std::uint16_t a = 0;
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        std::uint16_t s = 0x01FF;
        std::uint16_t d = 0;
        std::uint16_t pc = 0;
        std::uint8_t db = 0;
        std::uint8_t pb = 0;
        std::uint8_t p = 0x34;
        bool emulation = true;
    };

    enum StatusFlag : std::uint8_t {
        Carry = 0x01,
        Zero = 0x02,
        InterruptDisable = 0x04,
        Decimal = 0x08,
        Index8 = 0x10,
        Accumulator8 = 0x20,
        Overflow = 0x40,
        Negative = 0x80
    };

    void reset(const SnesMemory& memory) noexcept;
    [[nodiscard]] std::uint8_t step(SnesMemory& memory, std::string& error) noexcept;

    [[nodiscard]] const Registers& registers() const noexcept;
    [[nodiscard]] bool flag(StatusFlag flag) const noexcept;

private:
    [[nodiscard]] std::uint8_t fetch8(SnesMemory& memory) noexcept;
    [[nodiscard]] std::uint16_t fetch16(SnesMemory& memory) noexcept;
    void setNz8(std::uint8_t value) noexcept;
    void setNz16(std::uint16_t value) noexcept;

    Registers registers_{};
};
