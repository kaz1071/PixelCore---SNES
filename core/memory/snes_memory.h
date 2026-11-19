#pragma once

#include <array>
#include <cstdint>

class SnesMemory
{
public:
    static constexpr std::size_t WramSize = 128 * 1024;

    SnesMemory();

    [[nodiscard]] std::uint8_t read8(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);
    void clear();

private:
    std::array<std::uint8_t, WramSize> wram_{};
};
