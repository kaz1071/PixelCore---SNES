#include "core/memory/snes_memory.h"

#include <utility>

SnesMemory::SnesMemory() = default;

bool SnesMemory::mapLoRom(std::uint32_t address, std::size_t& offset) const
{
    if (rom_.empty()) {
        return false;
    }

    address &= 0x00FFFFFFU;
    const auto bank = static_cast<std::uint8_t>(address >> 16U);
    const auto within_bank = static_cast<std::uint16_t>(address);

    // bank is uint8_t, so every value is already <= 0xFF.
    const bool rom_bank = (bank <= 0x7D) || (bank >= 0x80);
    if (!rom_bank || within_bank < 0x8000) {
        return false;
    }

    const std::size_t bank_number = static_cast<std::size_t>(bank & 0x7FU);
    offset = (bank_number * 0x8000U) + (within_bank & 0x7FFFU);
    offset %= rom_.size();
    return true;
}

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

    std::size_t rom_offset = 0;
    if (mapLoRom(address, rom_offset)) {
        return rom_[rom_offset];
    }

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

void SnesMemory::loadLoRom(std::vector<std::uint8_t> rom)
{
    rom_ = std::move(rom);
}

void SnesMemory::clear()
{
    wram_.fill(0);
    rom_.clear();
}
