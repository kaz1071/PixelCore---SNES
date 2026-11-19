#pragma once

#include <cstdint>
#include <string>

class SnesMemory;

class W65816
{
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
        bool stopped = false;
    };

    void reset(const SnesMemory& memory);
    [[nodiscard]] bool step(SnesMemory& memory, std::string& error);

    [[nodiscard]] const Registers& registers() const;
    void setRegisters(const Registers& registers);

private:
    Registers registers_{};

    [[nodiscard]] std::uint8_t fetch8(const SnesMemory& memory);
    [[nodiscard]] std::uint16_t fetch16(const SnesMemory& memory);
};
