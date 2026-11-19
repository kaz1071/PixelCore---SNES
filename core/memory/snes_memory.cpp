#include "core/memory/snes_memory.h"

SnesMemory::SnesMemory() = default;

std::uint8_t SnesMemory::read8(std::uint32_t address) const
{
    address &= 0x00FFFFFFU;
    const auto bank = static_cast<std::uint8_t>(address >> 16U);
    const auto offset = static_cast<std::uint16_t>(address);

    if (bank == 0x7E || bank == 0x7F) {
        const std::uint32_t physical =
            (static_cast<std::uint32_t>(bank - 0x7E) << 16U) | offset;
        return wram_[physical];
    }

    const bool low_mirror_bank =
        (bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF);
    if (low_mirror_bank && offset <= 0x1FFF) {
        return wram_[offset];
    }

    // Unmapped reads currently return the SNES open-bus default.
    return 0;
}

void SnesMemory::write8(std::uint32_t address, std::uint8_t value)
{
    address &= 0x00FFFFFFU;
    const auto bank = static_cast<std::uint8_t>(address >> 16U);
    const auto offset = static_cast<std::uint16_t>(address);

    if (bank == 0x7E || bank == 0x7F) {
        const std::uint32_t physical =
            (static_cast<std::uint32_t>(bank - 0x7E) << 16U) | offset;
        wram_[physical] = value;
        return;
    }

    const bool low_mirror_bank =
        (bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF);
    if (low_mirror_bank && offset <= 0x1FFF) {
        wram_[offset] = value;
    }
}

void SnesMemory::clear()
{
    wram_.fill(0);
}
