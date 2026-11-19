#include "core/cpu/cpu65816.h"
#include "core/memory/snes_memory.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    SnesMemory memory;
    memory.write8(0x00FFFCU, 0x00U);
    memory.write8(0x00FFFDU, 0x80U);
    memory.write8(0x008000U, 0xEAU);
    memory.write8(0x008001U, 0xA9U);
    memory.write8(0x008002U, 0x00U);

    Cpu65816 cpu;
    cpu.reset(memory);
    check(cpu.registers().pc == 0x8000U, "reset loads the emulation-mode reset vector");
    check(cpu.registers().emulation, "reset enters emulation mode");
    check(cpu.registers().s == 0x01FFU, "reset initializes the emulation stack pointer");
    check(cpu.flag(Cpu65816::Accumulator8), "reset selects an 8-bit accumulator");
    check(cpu.flag(Cpu65816::Index8), "reset selects 8-bit index registers");

    std::string error;
    check(cpu.step(memory, error) == 2U, "NOP consumes two cycles");
    check(error.empty(), "NOP does not report an error");
    check(cpu.registers().pc == 0x8001U, "NOP advances the program counter");

    check(cpu.step(memory, error) == 2U, "8-bit LDA immediate consumes two cycles");
    check(cpu.registers().a == 0U, "LDA immediate loads the accumulator");
    check(cpu.flag(Cpu65816::Zero), "LDA immediate updates the zero flag");

    std::cout << "All 65C816 CPU tests passed.\n";
    return EXIT_SUCCESS;
}
