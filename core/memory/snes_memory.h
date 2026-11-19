#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class SnesMemory
{
public:
    static constexpr std::size_t WramSize = 128 * 1024;

    SnesMemory();

    [[nodiscard]] std::uint8_t read8(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);
    void loadLoRom(std::vector<std::uint8_t> rom);
    void clear();

private:
    std::array<std::uint8_t, WramSize> wram_{};
    std::vector<std::uint8_t> rom_;

    [[nodiscard]] bool mapLoRom(std::uint32_t address, std::size_t& offset) const;
};
