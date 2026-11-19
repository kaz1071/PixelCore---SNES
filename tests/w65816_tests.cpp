#include "core/cpu/w65816.h"
#include "core/memory/snes_memory.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

    memory.write8(0x00FFFC, 0x00);
    memory.write8(0x00FFFD, 0x80);
    cpu.reset(memory);

    check(cpu.registers().pc == 0x8000, "reset loads the emulation-mode reset vector");
    check(cpu.registers().emulation, "reset enters emulation mode");
    check(cpu.registers().s == 0x01FF, "reset initializes the emulation stack pointer");
    check(cpu.registers().p == 0x34, "reset initializes processor status");

    // The low-bank mirror exposes WRAM at 00:0000-1FFF for this initial CPU smoke test.
    memory.write8(0x000000, 0xA9); // LDA #$42
    memory.write8(0x000001, 0x42);
    memory.write8(0x000002, 0xEA); // NOP
    W65816::Registers registers = cpu.registers();
    registers.pc = 0x0000;
    cpu.setRegisters(registers);

    check(cpu.step(memory, error), "LDA immediate executes");
    check(cpu.registers().a == 0x42, "LDA immediate loads accumulator");
    check((cpu.registers().p & 0x02U) == 0, "LDA clears zero flag for nonzero result");
    check(cpu.step(memory, error), "NOP executes");
    check(cpu.registers().pc == 0x0003, "instructions advance the program counter");

    memory.write8(0x000003, 0xDB);
    check(cpu.step(memory, error), "STP executes");
    check(cpu.registers().stopped, "STP sets stopped state");
    check(!cpu.step(memory, error), "stopped CPU refuses to execute another instruction");

    std::cout << "All 65C816 tests passed.\n";
    return 0;
}
