#include "core/cpu/w65816.h"
#include "core/memory/snes_memory.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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
    W65816 cpu;
    std::string error;

    std::vector<std::uint8_t> rom(0x8000, 0x00);
    rom[0x0000] = 0xA9; // LDA #$42
    rom[0x0001] = 0x42;
    rom[0x0002] = 0xEA; // NOP
    rom[0x0003] = 0xDB; // STP
    rom[0x7FFC] = 0x00; // Reset vector = $8000
    rom[0x7FFD] = 0x80;
    memory.loadLoRom(std::move(rom));

    cpu.reset(memory);
    check(cpu.registers().pc == 0x8000, "reset loads the LoROM reset vector");
    check(cpu.registers().emulation, "reset enters emulation mode");
    check(cpu.registers().s == 0x01FF, "reset initializes the emulation stack pointer");
    check(cpu.registers().p == 0x34, "reset initializes processor status");

    check(cpu.step(memory, error), "LDA immediate executes");
    check(cpu.registers().a == 0x42, "LDA immediate loads accumulator");
    check((cpu.registers().p & 0x02U) == 0, "LDA clears zero flag for nonzero result");
    check(cpu.step(memory, error), "NOP executes");
    check(cpu.registers().pc == 0x8003, "instructions advance the program counter");

    check(cpu.step(memory, error), "STP executes");
    check(cpu.registers().stopped, "STP sets stopped state");
    check(!cpu.step(memory, error), "stopped CPU refuses to execute another instruction");

    std::cout << "All 65C816 tests passed.\n";
    return 0;
}
