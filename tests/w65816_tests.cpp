#include "core/cpu/w65816.h"
#include "core/memory/snes_memory.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\\n';
        std::exit(1);
    }
}

void loadProgram(SnesMemory& memory, std::initializer_list<std::uint8_t> program)
{
    std::vector<std::uint8_t> rom(0x8000, 0xEA);
    std::size_t index = 0;
    for (const std::uint8_t byte : program) {
        rom[index++] = byte;
    }
    rom[0x7FFC] = 0x00;
    rom[0x7FFD] = 0x80;
    memory.loadLoRom(std::move(rom));
}
}

int main()
{
    SnesMemory memory;
    W65816 cpu;
    std::string error;

    loadProgram(memory, {0xA9, 0x42, 0xEA, 0xDB}); // LDA #$42, NOP, STP
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
    check(error == "65C816 is stopped", "stopped CPU reports a useful error");

    loadProgram(memory, {
        0x38, 0x18, // SEC, CLC
        0x78, 0x58, // SEI, CLI
        0xF8, 0xD8, // SED, CLD
        0xB8        // CLV
    });
    cpu.reset(memory);
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U), "SEC sets carry");
    check(cpu.step(memory, error) && !(cpu.registers().p & 0x01U), "CLC clears carry");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x04U), "SEI sets interrupt disable");
    check(cpu.step(memory, error) && !(cpu.registers().p & 0x04U), "CLI clears interrupt disable");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x08U), "SED sets decimal mode");
    check(cpu.step(memory, error) && !(cpu.registers().p & 0x08U), "CLD clears decimal mode");
    cpu.setRegisters(W65816::Registers{.p = 0x74, .emulation = false});
    check(cpu.step(memory, error) && !(cpu.registers().p & 0x40U), "CLV clears overflow");

    loadProgram(memory, {
        0xA9, 0x7F, // LDA #$7F
        0xAA,       // TAX
        0xE8,       // INX
        0xCA,       // DEX
        0xA8,       // TAY
        0x88,       // DEY
        0x8A        // TXA
    });
    cpu.reset(memory);
    check(cpu.step(memory, error), "LDA for transfer test executes");
    check(cpu.step(memory, error) && cpu.registers().x == 0x7F, "TAX transfers accumulator to X");
    check(cpu.step(memory, error) && cpu.registers().x == 0x80, "INX increments 8-bit X");
    check((cpu.registers().p & 0x80U) != 0, "INX updates negative flag");
    check(cpu.step(memory, error) && cpu.registers().x == 0x7F, "DEX decrements 8-bit X");
    check(cpu.step(memory, error) && cpu.registers().y == 0x7F, "TAY transfers accumulator to Y");
    check(cpu.step(memory, error) && cpu.registers().y == 0x7E, "DEY decrements 8-bit Y");
    check(cpu.step(memory, error) && (cpu.registers().a & 0xFFU) == 0x7F, "TXA transfers X to accumulator");

    loadProgram(memory, {
        0xA9, 0x00, // LDA #$00
        0xF0, 0x02, // BEQ skips next LDA
        0xA9, 0xFF,
        0xA9, 0x55, // LDA #$55
        0xD0, 0x02, // BNE skips next LDA
        0xA9, 0xEE,
        0x80, 0x02, // BRA skips next LDA
        0xA9, 0xDD,
        0xEA
    });
    cpu.reset(memory);
    check(cpu.step(memory, error), "LDA zero executes before BEQ");
    check(cpu.step(memory, error) && cpu.registers().pc == 0x8006, "BEQ branches when zero flag is set");
    check(cpu.step(memory, error) && cpu.registers().a == 0x55, "instruction after BEQ is reached");
    check(cpu.step(memory, error) && cpu.registers().pc == 0x800C, "BNE branches when zero flag is clear");
    check(cpu.step(memory, error) && cpu.registers().pc == 0x8010, "BRA performs an unconditional relative branch");

    loadProgram(memory, {0xC2, 0x30, 0xE2, 0x10, 0xFB});
    cpu.reset(memory);
    check(cpu.step(memory, error), "REP executes in emulation mode");
    check((cpu.registers().p & 0x30U) == 0x30U, "REP cannot clear M or X in emulation mode");
    auto native = cpu.registers();
    native.emulation = false;
    native.p = 0x00;
    native.x = 0xABCD;
    native.y = 0x1234;
    cpu.setRegisters(native);
    check(cpu.step(memory, error), "SEP executes in native mode");
    check((cpu.registers().p & 0x10U) != 0, "SEP sets index width flag");
    check(cpu.registers().x == 0x00CD && cpu.registers().y == 0x0034,
          "setting 8-bit index width clears index register high bytes");
    auto enterEmulation = cpu.registers();
    enterEmulation.p = static_cast<std::uint8_t>(enterEmulation.p | 0x01U);
    cpu.setRegisters(enterEmulation);
    check(cpu.step(memory, error), "XCE executes");
    check(cpu.registers().emulation, "XCE enters emulation mode from carry");
    check((cpu.registers().p & 0x30U) == 0x30U, "entering emulation forces 8-bit widths");

    loadProgram(memory, {0xA2, 0x34, 0x12, 0x9A, 0xBA});
    cpu.reset(memory);
    auto nativeStack = cpu.registers();
    nativeStack.emulation = false;
    nativeStack.p = 0x00;
    cpu.setRegisters(nativeStack);
    check(cpu.step(memory, error) && cpu.registers().x == 0x1234, "LDX immediate loads a 16-bit value");
    check(cpu.step(memory, error) && cpu.registers().s == 0x1234, "TXS transfers X to stack pointer in native mode");
    check(cpu.step(memory, error) && cpu.registers().x == 0x1234, "TSX transfers stack pointer to X");

    std::cout << "All 65C816 tests passed.\\n";
    return 0;
}
