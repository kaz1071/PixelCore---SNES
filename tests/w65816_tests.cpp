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
        std::cerr << "FAIL: " << message << '\n';
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
    auto clearOverflow = cpu.registers();
    clearOverflow.p = 0x74;
    clearOverflow.emulation = false;
    cpu.setRegisters(clearOverflow);
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

    loadProgram(memory, {
        0xA9, 0x5A,       // LDA #$5A
        0x8D, 0x34, 0x12, // STA $1234
        0xA9, 0x00,       // LDA #$00
        0xAD, 0x34, 0x12, // LDA $1234
        0xA2, 0xA5,       // LDX #$A5
        0x8E, 0x35, 0x12, // STX $1235
        0xA0, 0x3C,       // LDY #$3C
        0x8C, 0x36, 0x12  // STY $1236
    });
    cpu.reset(memory);
    auto dataBank = cpu.registers();
    dataBank.db = 0x7E;
    cpu.setRegisters(dataBank);
    check(cpu.step(memory, error), "LDA immediate prepares absolute store test");
    check(cpu.step(memory, error), "STA absolute executes");
    check(memory.read8(0x7E1234) == 0x5A, "STA absolute writes accumulator to data bank");
    check(cpu.step(memory, error), "LDA immediate clears accumulator before absolute load");
    check(cpu.step(memory, error) && cpu.registers().a == 0x5A,
          "LDA absolute reads from data bank");
    check(cpu.step(memory, error) && cpu.registers().x == 0xA5,
          "LDX immediate prepares absolute index store");
    check(cpu.step(memory, error) && memory.read8(0x7E1235) == 0xA5,
          "STX absolute writes X to data bank");
    check(cpu.step(memory, error) && cpu.registers().y == 0x3C,
          "LDY immediate prepares absolute index store");
    check(cpu.step(memory, error) && memory.read8(0x7E1236) == 0x3C,
          "STY absolute writes Y to data bank");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xA9, 0x34, 0x12,       // LDA #$1234
        0x8D, 0x00, 0x20,       // STA $2000
        0xA9, 0x00, 0x00,       // LDA #$0000
        0xAD, 0x00, 0x20        // LDA $2000
    });
    cpu.reset(memory);
    auto wideDataBank = cpu.registers();
    wideDataBank.emulation = false;
    wideDataBank.p = 0x00;
    wideDataBank.db = 0x7E;
    cpu.setRegisters(wideDataBank);
    check(cpu.step(memory, error), "REP enables 16-bit register widths");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA immediate loads both accumulator bytes");
    check(cpu.step(memory, error), "16-bit STA absolute executes");
    check(memory.read8(0x7E2000) == 0x34 && memory.read8(0x7E2001) == 0x12,
          "16-bit STA absolute writes little-endian accumulator");
    check(cpu.step(memory, error), "16-bit LDA immediate clears accumulator");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA absolute reads both bytes");

    loadProgram(memory, {
        0xA9, 0x80, // LDA #$80
        0x0A,       // ASL A
        0xA9, 0x01, // LDA #$01
        0x4A,       // LSR A
        0xA9, 0x80, // LDA #$80
        0x38,       // SEC
        0x2A,       // ROL A
        0xA9, 0x01, // LDA #$01
        0x6A,       // ROR A
        0xA9, 0xFF, // LDA #$FF
        0x1A,       // INC A
        0x3A        // DEC A
    });
    cpu.reset(memory);
    check(cpu.step(memory, error) && cpu.registers().a == 0x80,
          "LDA prepares accumulator shift tests");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00 &&
          (cpu.registers().p & 0x01U) != 0 && (cpu.registers().p & 0x02U) != 0,
          "ASL accumulator shifts left and sets carry and zero");
    check(cpu.step(memory, error), "LDA prepares logical right shift");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00 &&
          (cpu.registers().p & 0x01U) != 0,
          "LSR accumulator shifts right and sets carry");
    check(cpu.step(memory, error), "LDA prepares rotate left");
    check(cpu.step(memory, error), "SEC prepares rotate-left carry input");
    check(cpu.step(memory, error) && cpu.registers().a == 0x01 &&
          (cpu.registers().p & 0x01U) != 0,
          "ROL accumulator rotates carry into bit zero");
    check(cpu.step(memory, error), "LDA prepares rotate right");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80 &&
          (cpu.registers().p & 0x01U) != 0,
          "ROR accumulator rotates carry into the sign bit");
    check(cpu.step(memory, error), "LDA prepares accumulator increment");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00 &&
          (cpu.registers().p & 0x02U) != 0,
          "INC accumulator wraps at 8-bit width and sets zero");
    check(cpu.step(memory, error) && cpu.registers().a == 0xFF &&
          (cpu.registers().p & 0x80U) != 0,
          "DEC accumulator wraps at 8-bit width and sets negative");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0x00, 0x80,       // LDA #$8000
        0x0A,                   // ASL A
        0xA9, 0x00, 0x80,       // LDA #$8000
        0x38, 0x2A,             // SEC, ROL A
        0xA9, 0x01, 0x00,       // LDA #$0001
        0x6A                    // ROR A
    });
    cpu.reset(memory);
    auto nativeAccumulator = cpu.registers();
    nativeAccumulator.emulation = false;
    nativeAccumulator.p = 0x00;
    cpu.setRegisters(nativeAccumulator);
    check(cpu.step(memory, error), "REP enables 16-bit accumulator for shift tests");
    check(cpu.step(memory, error) && cpu.registers().a == 0x8000,
          "16-bit LDA prepares ASL");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0000 &&
          (cpu.registers().p & 0x01U) != 0,
          "ASL accumulator uses the 16-bit sign bit");
    check(cpu.step(memory, error) && cpu.registers().a == 0x8000,
          "16-bit LDA prepares ROL");
    check(cpu.step(memory, error) && cpu.step(memory, error) &&
          cpu.registers().a == 0x0001 && (cpu.registers().p & 0x01U) != 0,
          "16-bit ROL shifts through carry");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0001,
          "16-bit LDA prepares ROR");
    check(cpu.step(memory, error) && cpu.registers().a == 0x8000,
          "16-bit ROR inserts carry at bit fifteen");

    loadProgram(memory, {
        0xA9, 0xF0, // LDA #$F0
        0x29, 0x3C, // AND #$3C
        0x09, 0x80, // ORA #$80
        0x49, 0xFF  // EOR #$FF
    });
    cpu.reset(memory);
    auto logical8 = cpu.registers();
    logical8.a = 0xAB00;
    cpu.setRegisters(logical8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA immediate preserves the accumulator high byte in 8-bit mode");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND immediate applies an 8-bit mask and preserves accumulator high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0 &&
          (cpu.registers().p & 0x80U) != 0,
          "ORA immediate sets bits and updates the negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR immediate toggles bits and preserves accumulator high byte");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,       // LDA #$0FF0
        0x29, 0x0F, 0x00,       // AND #$000F
        0x09, 0x00, 0x80,       // ORA #$8000
        0x49, 0xFF, 0xFF        // EOR #$FFFF
    });
    cpu.reset(memory);
    auto logical16 = cpu.registers();
    logical16.emulation = false;
    logical16.p = 0x00;
    cpu.setRegisters(logical16);
    check(cpu.step(memory, error), "REP enables 16-bit accumulator for logical tests");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0000 &&
          (cpu.registers().p & 0x02U) != 0,
          "16-bit AND immediate uses both operand bytes and sets zero");
    check(cpu.step(memory, error) && cpu.registers().a == 0x8000 &&
          (cpu.registers().p & 0x80U) != 0,
          "16-bit ORA immediate updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7FFF &&
          (cpu.registers().p & 0x80U) == 0,
          "16-bit EOR immediate updates the 16-bit negative flag");
    loadProgram(memory, {
        0xA9, 0x80, // LDA #$80
        0xC9, 0x7F, // CMP #$7F
        0xC9, 0xFF, // CMP #$FF
        0xC9, 0x80, // CMP #$80
        0xA2, 0x20, // LDX #$20
        0xE0, 0x21, // CPX #$21
        0xA0, 0x00, // LDY #$00
        0xC0, 0x00  // CPY #$00
    });
    cpu.reset(memory);
    check(cpu.step(memory, error), "LDA prepares 8-bit CMP");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U) != 0 &&
          (cpu.registers().p & 0x80U) == 0,
          "CMP sets carry when accumulator is greater and clears negative");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U) == 0 &&
          (cpu.registers().p & 0x80U) != 0,
          "CMP clears carry and sets negative when accumulator is smaller");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP sets carry and zero for equal operands");
    check(cpu.step(memory, error), "LDX prepares 8-bit CPX");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U) == 0 &&
          (cpu.registers().p & 0x80U) != 0,
          "CPX compares index register and updates carry and negative");
    check(cpu.step(memory, error), "LDY prepares 8-bit CPY");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPY sets carry and zero for equal index values");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xA9, 0x00, 0x80,       // LDA #$8000
        0xC9, 0xFF, 0x7F,       // CMP #$7FFF
        0xA2, 0x34, 0x12,       // LDX #$1234
        0xE0, 0x35, 0x12,       // CPX #$1235
        0xA0, 0x56, 0x34,       // LDY #$3456
        0xC0, 0x56, 0x34        // CPY #$3456
    });
    cpu.reset(memory);
    auto compare16 = cpu.registers();
    compare16.emulation = false;
    compare16.p = 0x00;
    cpu.setRegisters(compare16);
    check(cpu.step(memory, error), "REP enables 16-bit comparison widths");
    check(cpu.step(memory, error), "16-bit LDA prepares CMP");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U) != 0 &&
          (cpu.registers().p & 0x80U) == 0,
          "16-bit CMP compares both bytes and sets carry with positive result");
    check(cpu.step(memory, error), "16-bit LDX prepares CPX");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x01U) == 0 &&
          (cpu.registers().p & 0x80U) != 0,
          "16-bit CPX compares both index bytes");
    check(cpu.step(memory, error), "16-bit LDY prepares CPY");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "16-bit CPY sets carry and zero for equal operands");
    std::cout << "All 65C816 tests passed.\n";
    loadProgram(memory, {
        0xA9, 0xF0,       // LDA #$F0
        0x89, 0x0F,       // BIT #$0F: zero
        0x89, 0x80        // BIT #$80: nonzero
    });
    cpu.reset(memory);
    auto bit8 = cpu.registers();
    bit8.p = static_cast<std::uint8_t>(bit8.p | 0xC0U);
    cpu.setRegisters(bit8);
    check(cpu.step(memory, error), "LDA prepares 8-bit BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x02U) != 0 &&
          (cpu.registers().p & 0xC0U) == 0xC0U,
          "8-bit BIT immediate sets zero and preserves negative and overflow");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x02U) == 0 &&
          (cpu.registers().p & 0xC0U) == 0xC0U,
          "8-bit BIT immediate clears zero when masked bits are nonzero");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0x01, 0x80,       // LDA #$8001
        0x89, 0x00, 0x80,       // BIT #$8000: nonzero
        0x89, 0xFE, 0x7F        // BIT #$7FFE: zero
    });
    cpu.reset(memory);
    auto bit16 = cpu.registers();
    bit16.emulation = false;
    bit16.p = 0x00;
    cpu.setRegisters(bit16);
    check(cpu.step(memory, error), "REP enables 16-bit BIT width");
    check(cpu.step(memory, error), "16-bit LDA prepares BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x02U) == 0,
          "16-bit BIT immediate tests both accumulator bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x02U) != 0,
          "16-bit BIT immediate sets zero when no masked bits overlap");

    return 0;
}
