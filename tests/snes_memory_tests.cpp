#include "core/memory/snes_memory.h"

#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    SnesMemory memory;

    memory.write8(0x7E0000, 0x12);
    check(memory.read8(0x7E0000) == 0x12, "WRAM bank 7E stores and reads data");
    check(memory.read8(0x000000) == 0x12, "bank 00 low memory mirrors WRAM");

    memory.write8(0x7F1234, 0xA5);
    check(memory.read8(0x7F1234) == 0xA5, "WRAM bank 7F stores and reads data");

    memory.write8(0x001FFF, 0x5A);
    check(memory.read8(0x7E1FFF) == 0x5A, "last byte of low WRAM mirror maps correctly");

    memory.write8(0x002000, 0xFF);
    check(memory.read8(0x7E2000) == 0, "address outside low WRAM mirror is not written");

    memory.write8(0x800123, 0xC3);
    check(memory.read8(0x7E0123) == 0xC3, "bank 80 low memory mirrors WRAM");

    memory.clear();
    check(memory.read8(0x7E0000) == 0, "clear resets WRAM");

    std::cout << "All SNES memory tests passed.\n";
    return 0;
}
