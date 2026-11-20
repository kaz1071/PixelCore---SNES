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
    loadProgram(memory, {
        0xCF, 0x00, 0x10, 0x7E,       // CMP $7E1000
        0xDF, 0xFF, 0x0F, 0x7E        // CMP $7E0FFF,X
    });
    memory.write8(0x7E1000, 0x50);
    memory.write8(0x7E1001, 0x00);
    cpu.reset(memory);
    auto compareLong8 = cpu.registers();
    compareLong8.a = 0xAB50;
    compareLong8.x = 1;
    compareLong8.db = 0x12;
    cpu.setRegisters(compareLong8);
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute long compares independently of the data bank");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute long,X adds X to the full 24-bit address");

    loadProgram(memory, {
        0xC2, 0x20,                   // REP #$20: 16-bit accumulator
        0xCF, 0x00, 0x10, 0x7E,       // CMP $7E1000
        0xDF, 0xFE, 0x0F, 0x7E        // CMP $7E0FFE,X
    });
    memory.write8(0x7E1000, 0x34);
    memory.write8(0x7E1001, 0x12);
    cpu.reset(memory);
    auto compareLong16 = cpu.registers();
    compareLong16.emulation = false;
    compareLong16.p = 0x00;
    compareLong16.a = 0x1234;
    compareLong16.x = 2;
    compareLong16.db = 0x12;
    cpu.setRegisters(compareLong16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute-long compare operations");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute long compares both bytes at the 24-bit address");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute long,X compares both bytes at the indexed 24-bit address");

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


    loadProgram(memory, {
        0xA9, 0xF0,       // LDA #$F0
        0x2C, 0x00, 0x10, // BIT $1000
        0x2C, 0x00, 0x10  // BIT $1000 again
    });
    memory.write8(0x001000, 0xC0);
    cpu.reset(memory);
    auto bitAbs8 = cpu.registers();
    bitAbs8.p = static_cast<std::uint8_t>(bitAbs8.p | 0x01U);
    cpu.setRegisters(bitAbs8);
    check(cpu.step(memory, error), "LDA prepares 8-bit absolute BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC3U) == 0xC1U,
          "8-bit BIT absolute sets N and V from memory and clears Z");
    memory.write8(0x001000, 0x0F);
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC2U) == 0x02U,
          "8-bit BIT absolute sets Z when A and memory do not overlap");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0x00, 0x80,       // LDA #$8000
        0x2C, 0x00, 0x10,       // BIT $1000
        0x2C, 0x00, 0x10        // BIT $1000 again
    });
    memory.write8(0x001000, 0x00);
    memory.write8(0x001001, 0x80);
    cpu.reset(memory);
    auto bitAbs16 = cpu.registers();
    bitAbs16.emulation = false;
    bitAbs16.p = 0x00;
    cpu.setRegisters(bitAbs16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute BIT");
    check(cpu.step(memory, error), "16-bit LDA prepares absolute BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC2U) == 0x80U,
          "16-bit BIT absolute reads both bytes and sets negative");
    memory.write8(0x001000, 0x00);
    memory.write8(0x001001, 0x40);
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC2U) == 0x42U,
          "16-bit BIT absolute reads both bytes and sets overflow and zero");
    loadProgram(memory, {
        0xA2, 0x04,       // LDX #$04
        0xA9, 0xF0,       // LDA #$F0
        0x3C, 0x00, 0x10  // BIT $1000,X
    });
    memory.write8(0x001004, 0xC0);
    cpu.reset(memory);
    check(cpu.step(memory, error), "LDX prepares indexed absolute BIT");
    check(cpu.step(memory, error), "LDA prepares indexed absolute BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC2U) == 0xC0U,
          "8-bit BIT absolute,X reads from the indexed address and updates N and V");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xA2, 0x04, 0x00,       // LDX #$0004
        0xA9, 0x00, 0x80,       // LDA #$8000
        0x3C, 0x00, 0x10        // BIT $1000,X
    });
    memory.write8(0x001004, 0x00);
    memory.write8(0x001005, 0x80);
    cpu.reset(memory);
    auto bitIndexed16 = cpu.registers();
    bitIndexed16.emulation = false;
    bitIndexed16.p = 0x00;
    cpu.setRegisters(bitIndexed16);
    check(cpu.step(memory, error), "REP enables 16-bit indexed BIT");
    check(cpu.step(memory, error), "16-bit LDX prepares indexed BIT");
    check(cpu.step(memory, error), "16-bit LDA prepares indexed BIT");
    check(cpu.step(memory, error) && (cpu.registers().p & 0xC2U) == 0x80U,
          "16-bit BIT absolute,X reads both bytes at the indexed address");
    loadProgram(memory, {
        0xA9, 0xF0,             // LDA #$F0
        0x2D, 0x00, 0x10,       // AND $1000
        0x0D, 0x01, 0x10,       // ORA $1001
        0x5D, 0x00, 0x10        // EOR $1000,X
    });
    memory.write8(0x7E1000, 0x3C);
    memory.write8(0x7E1001, 0x80);
    cpu.reset(memory);
    auto logicalAbsolute8 = cpu.registers();
    logicalAbsolute8.a = 0xAB00;
    logicalAbsolute8.x = 1;
    logicalAbsolute8.db = 0x7E;
    cpu.setRegisters(logicalAbsolute8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit absolute logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND absolute reads from the data bank and preserves accumulator high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0 &&
          (cpu.registers().p & 0x80U) != 0,
          "ORA absolute updates the 8-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "EOR absolute,X uses the indexed data-bank address");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,       // LDA #$0FF0
        0x2D, 0x02, 0x10,       // AND $1002
        0x1D, 0x02, 0x10,       // ORA $1002,X
        0x4D, 0x06, 0x10        // EOR $1006
    });
    memory.write8(0x7E1002, 0xF0);
    memory.write8(0x7E1003, 0x00);
    memory.write8(0x7E1004, 0x00);
    memory.write8(0x7E1005, 0x80);
    memory.write8(0x7E1006, 0xFF);
    memory.write8(0x7E1007, 0xFF);
    cpu.reset(memory);
    auto logicalAbsolute16 = cpu.registers();
    logicalAbsolute16.emulation = false;
    logicalAbsolute16.p = 0x00;
    logicalAbsolute16.x = 2;
    logicalAbsolute16.db = 0x7E;
    cpu.setRegisters(logicalAbsolute16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares absolute logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND absolute reads both bytes and applies a 16-bit mask");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0 &&
          (cpu.registers().p & 0x80U) != 0,
          "ORA absolute,X reads both bytes at the indexed address and sets negative");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F &&
          (cpu.registers().p & 0x80U) == 0,
          "EOR absolute reads both bytes and updates the 16-bit negative flag");

    loadProgram(memory, {
        0xA9, 0xF0,       // LDA #$F0
        0x25, 0x20,       // AND $20
        0x05, 0x21,       // ORA $21
        0x45, 0x22        // EOR $22
    });
    memory.write8(0x001020, 0x3C);
    memory.write8(0x001021, 0x80);
    memory.write8(0x001022, 0xFF);
    cpu.reset(memory);
    auto directPage8 = cpu.registers();
    directPage8.a = 0xAB00;
    directPage8.d = 0x1000;
    cpu.setRegisters(directPage8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit direct-page logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND direct page adds the direct-page register and preserves A high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0,
          "ORA direct page reads the direct-page operand");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR direct page updates the accumulator");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,       // LDA #$0FF0
        0x25, 0x10,             // AND $10
        0x05, 0x12,             // ORA $12
        0x45, 0x14              // EOR $14
    });
    memory.write8(0x001010, 0xF0);
    memory.write8(0x001011, 0x00);
    memory.write8(0x001012, 0x00);
    memory.write8(0x001013, 0x80);
    memory.write8(0x001014, 0xFF);
    memory.write8(0x001015, 0xFF);
    cpu.reset(memory);
    auto directPage16 = cpu.registers();
    directPage16.emulation = false;
    directPage16.p = 0x00;
    directPage16.d = 0x1000;
    cpu.setRegisters(directPage16);
    check(cpu.step(memory, error), "REP enables 16-bit direct-page logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares direct-page logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND direct page reads both operand bytes");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0,
          "ORA direct page updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F,
          "EOR direct page reads both bytes and updates negative");

    loadProgram(memory, {
        0xA9, 0xF0,       // LDA #$F0
        0x35, 0x20,       // AND $20,X
        0x15, 0x21,       // ORA $21,X
        0x55, 0x22        // EOR $22,X
    });
    memory.write8(0x001023, 0x3C);
    memory.write8(0x001024, 0x80);
    memory.write8(0x001025, 0xFF);
    cpu.reset(memory);
    auto directPageIndexed8 = cpu.registers();
    directPageIndexed8.a = 0xAB00;
    directPageIndexed8.d = 0x1000;
    directPageIndexed8.x = 3;
    cpu.setRegisters(directPageIndexed8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit direct-page indexed logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND direct page,X adds D and X and preserves A high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0,
          "ORA direct page,X reads the indexed direct-page operand");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR direct page,X updates the accumulator");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,       // LDA #$0FF0
        0x35, 0x10,             // AND $10,X
        0x15, 0x12,             // ORA $12,X
        0x55, 0x14              // EOR $14,X
    });
    memory.write8(0x001012, 0xF0);
    memory.write8(0x001013, 0x00);
    memory.write8(0x001014, 0x00);
    memory.write8(0x001015, 0x80);
    memory.write8(0x001016, 0xFF);
    memory.write8(0x001017, 0xFF);
    cpu.reset(memory);
    auto directPageIndexed16 = cpu.registers();
    directPageIndexed16.emulation = false;
    directPageIndexed16.p = 0x00;
    directPageIndexed16.d = 0x1000;
    directPageIndexed16.x = 2;
    cpu.setRegisters(directPageIndexed16);
    check(cpu.step(memory, error), "REP enables 16-bit direct-page indexed logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares direct-page indexed logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND direct page,X reads both indexed operand bytes");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0,
          "ORA direct page,X updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F,
          "EOR direct page,X reads both bytes and updates negative");

    loadProgram(memory, {
        0xA9, 0x5A,       // LDA #$5A
        0x85, 0x20,       // STA $20
        0xA9, 0x00,       // LDA #$00
        0xA5, 0x20,       // LDA $20
        0xA2, 0xA5,       // LDX #$A5
        0x86, 0x21,       // STX $21
        0xA0, 0x3C,       // LDY #$3C
        0x84, 0x22,       // STY $22
        0xA6, 0x21,       // LDX $21
        0xA4, 0x22        // LDY $22
    });
    cpu.reset(memory);
    auto directPageLoads8 = cpu.registers();
    directPageLoads8.a = 0xAB00;
    directPageLoads8.d = 0x1000;
    cpu.setRegisters(directPageLoads8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB5A,
          "LDA immediate prepares 8-bit direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001020) == 0x5A,
          "STA direct page writes the accumulator");
    check(cpu.step(memory, error), "LDA immediate clears accumulator before direct-page load");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB5A,
          "LDA direct page loads data and preserves A high byte");
    check(cpu.step(memory, error) && cpu.registers().x == 0xA5,
          "LDX immediate prepares direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001021) == 0xA5,
          "STX direct page writes X");
    check(cpu.step(memory, error) && cpu.registers().y == 0x3C,
          "LDY immediate prepares direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001022) == 0x3C,
          "STY direct page writes Y");
    check(cpu.step(memory, error) && cpu.registers().x == 0xA5,
          "LDX direct page loads X");
    check(cpu.step(memory, error) && cpu.registers().y == 0x3C,
          "LDY direct page loads Y");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xA9, 0x34, 0x12,       // LDA #$1234
        0x85, 0x30,             // STA $30
        0xA2, 0x78, 0x56,       // LDX #$5678
        0x86, 0x32,             // STX $32
        0xA0, 0xBC, 0x9A,       // LDY #$9ABC
        0x84, 0x34,             // STY $34
        0xA9, 0x00, 0x00,       // LDA #$0000
        0xA2, 0x00, 0x00,       // LDX #$0000
        0xA0, 0x00, 0x00,       // LDY #$0000
        0xA5, 0x30,             // LDA $30
        0xA6, 0x32,             // LDX $32
        0xA4, 0x34              // LDY $34
    });
    cpu.reset(memory);
    auto directPageLoads16 = cpu.registers();
    directPageLoads16.emulation = false;
    directPageLoads16.p = 0x00;
    directPageLoads16.d = 0x1000;
    cpu.setRegisters(directPageLoads16);
    check(cpu.step(memory, error), "REP enables 16-bit direct-page load and store widths");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA prepares direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001030) == 0x34 &&
          memory.read8(0x001031) == 0x12,
          "16-bit STA direct page writes little-endian accumulator");
    check(cpu.step(memory, error) && cpu.registers().x == 0x5678,
          "16-bit LDX prepares direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001032) == 0x78 &&
          memory.read8(0x001033) == 0x56,
          "16-bit STX direct page writes both bytes");
    check(cpu.step(memory, error) && cpu.registers().y == 0x9ABC,
          "16-bit LDY prepares direct-page store");
    check(cpu.step(memory, error) && memory.read8(0x001034) == 0xBC &&
          memory.read8(0x001035) == 0x9A,
          "16-bit STY direct page writes both bytes");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0000,
          "16-bit LDA clears accumulator before direct-page reload");
    check(cpu.step(memory, error) && cpu.registers().x == 0x0000,
          "16-bit LDX clears X before direct-page reload");
    check(cpu.step(memory, error) && cpu.registers().y == 0x0000,
          "16-bit LDY clears Y before direct-page reload");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA direct page reads both bytes");
    check(cpu.step(memory, error) && cpu.registers().x == 0x5678,
          "16-bit LDX direct page reads both bytes");
    check(cpu.step(memory, error) && cpu.registers().y == 0x9ABC,
          "16-bit LDY direct page reads both bytes");

    loadProgram(memory, {
        0x95, 0x20,       // STA $20,X
        0x96, 0x30,       // STX $30,Y
        0x94, 0x40,       // STY $40,X
        0xA9, 0x00,       // LDA #$00
        0xB5, 0x20,       // LDA $20,X
        0xB6, 0x30,       // LDX $30,Y
        0xB4, 0x40        // LDY $40,X
    });
    cpu.reset(memory);
    auto indexedDirect8 = cpu.registers();
    indexedDirect8.a = 0xAB5A;
    indexedDirect8.d = 0x1000;
    indexedDirect8.x = 3;
    indexedDirect8.y = 4;
    cpu.setRegisters(indexedDirect8);
    check(cpu.step(memory, error) && memory.read8(0x001023) == 0x5A,
          "STA direct page,X stores at D plus operand plus X");
    check(cpu.step(memory, error) && memory.read8(0x001034) == 0x03,
          "STX direct page,Y stores at D plus operand plus Y");
    check(cpu.step(memory, error) && memory.read8(0x001043) == 0x04,
          "STY direct page,X stores at D plus operand plus X");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB00,
          "LDA immediate prepares indexed direct-page reload");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB5A,
          "LDA direct page,X loads and preserves A high byte");
    check(cpu.step(memory, error) && cpu.registers().x == 0x03,
          "LDX direct page,Y loads from Y-indexed direct page");
    auto restoreX = cpu.registers();
    restoreX.x = 3;
    cpu.setRegisters(restoreX);
    check(cpu.step(memory, error) && cpu.registers().y == 0x04,
          "LDY direct page,X loads from X-indexed direct page");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0x95, 0x20,             // STA $20,X
        0x96, 0x30,             // STX $30,Y
        0x94, 0x40,             // STY $40,X
        0xA9, 0x00, 0x00,       // LDA #$0000
        0xB5, 0x20              // LDA $20,X
    });
    cpu.reset(memory);
    auto indexedDirect16 = cpu.registers();
    indexedDirect16.emulation = false;
    indexedDirect16.p = 0x00;
    indexedDirect16.d = 0x1000;
    indexedDirect16.x = 2;
    indexedDirect16.y = 4;
    indexedDirect16.a = 0x1234;
    cpu.setRegisters(indexedDirect16);
    check(cpu.step(memory, error), "REP enables 16-bit indexed direct-page operations");
    check(cpu.step(memory, error) && memory.read8(0x001022) == 0x34 &&
          memory.read8(0x001023) == 0x12,
          "16-bit STA direct page,X writes little-endian accumulator");
    check(cpu.step(memory, error) && memory.read8(0x001034) == 0x02 &&
          memory.read8(0x001035) == 0x00,
          "16-bit STX direct page,Y writes both bytes");
    check(cpu.step(memory, error) && memory.read8(0x001042) == 0x04 &&
          memory.read8(0x001043) == 0x00,
          "16-bit STY direct page,X writes both bytes");
    check(cpu.step(memory, error), "16-bit LDA immediate clears accumulator");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA direct page,X reads both indexed bytes");

    loadProgram(memory, {
        0xA9, 0xF0,             // LDA #$F0
        0x39, 0x00, 0x10,       // AND $1000,Y
        0x19, 0x02, 0x10,       // ORA $1002,Y
        0x59, 0x04, 0x10        // EOR $1004,Y
    });
    memory.write8(0x7E1001, 0x3C);
    memory.write8(0x7E1003, 0x80);
    memory.write8(0x7E1005, 0xFF);
    cpu.reset(memory);
    auto logicalAbsoluteY8 = cpu.registers();
    logicalAbsoluteY8.a = 0xAB00;
    logicalAbsoluteY8.y = 1;
    logicalAbsoluteY8.db = 0x7E;
    cpu.setRegisters(logicalAbsoluteY8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit absolute,Y logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND absolute,Y uses Y and preserves accumulator high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0,
          "ORA absolute,Y reads from the indexed data-bank address");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR absolute,Y updates the 8-bit accumulator");

    loadProgram(memory, {
        0xC2, 0x20,             // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,       // LDA #$0FF0
        0x39, 0x00, 0x10,       // AND $1000,Y
        0x19, 0x02, 0x10,       // ORA $1002,Y
        0x59, 0x04, 0x10        // EOR $1004,Y
    });
    memory.write8(0x7E1002, 0xF0);
    memory.write8(0x7E1003, 0x00);
    memory.write8(0x7E1004, 0x00);
    memory.write8(0x7E1005, 0x80);
    memory.write8(0x7E1006, 0xFF);
    memory.write8(0x7E1007, 0xFF);
    cpu.reset(memory);
    auto logicalAbsoluteY16 = cpu.registers();
    logicalAbsoluteY16.emulation = false;
    logicalAbsoluteY16.p = 0x00;
    logicalAbsoluteY16.y = 2;
    logicalAbsoluteY16.db = 0x7E;
    cpu.setRegisters(logicalAbsoluteY16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute,Y logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares absolute,Y logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND absolute,Y reads both bytes at the Y-indexed address");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0,
          "ORA absolute,Y updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F &&
          (cpu.registers().p & 0x80U) == 0,
          "EOR absolute,Y reads both bytes and updates negative");

    loadProgram(memory, {
        0xA9, 0xF0,                   // LDA #$F0
        0x2F, 0x00, 0x10, 0x7E,       // AND $7E1000
        0x0F, 0x01, 0x10, 0x7E,       // ORA $7E1001
        0x4F, 0x02, 0x10, 0x7E        // EOR $7E1002
    });
    memory.write8(0x7E1000, 0x3C);
    memory.write8(0x7E1001, 0x80);
    memory.write8(0x7E1002, 0xFF);
    cpu.reset(memory);
    auto logicalLong8 = cpu.registers();
    logicalLong8.a = 0xAB00;
    logicalLong8.db = 0x12;
    cpu.setRegisters(logicalLong8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit absolute-long logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND absolute long uses the full 24-bit address and preserves A high byte");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0,
          "ORA absolute long reads independently of the data bank");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR absolute long updates the 8-bit accumulator");

    loadProgram(memory, {
        0xC2, 0x20,                   // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,             // LDA #$0FF0
        0x2F, 0x00, 0x10, 0x7E,       // AND $7E1000
        0x0F, 0x02, 0x10, 0x7E,       // ORA $7E1002
        0x4F, 0x04, 0x10, 0x7E        // EOR $7E1004
    });
    memory.write8(0x7E1000, 0xF0);
    memory.write8(0x7E1001, 0x00);
    memory.write8(0x7E1002, 0x00);
    memory.write8(0x7E1003, 0x80);
    memory.write8(0x7E1004, 0xFF);
    memory.write8(0x7E1005, 0xFF);
    cpu.reset(memory);
    auto logicalLong16 = cpu.registers();
    logicalLong16.emulation = false;
    logicalLong16.p = 0x00;
    logicalLong16.db = 0x12;
    cpu.setRegisters(logicalLong16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute-long logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares absolute-long logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND absolute long reads both bytes at the 24-bit address");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0,
          "ORA absolute long updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F &&
          (cpu.registers().p & 0x80U) == 0,
          "EOR absolute long reads both bytes and updates negative");

    loadProgram(memory, {
        0xA9, 0xF0,                   // LDA #$F0
        0x3F, 0x00, 0x10, 0x7E,       // AND $7E1000,X
        0x1F, 0x01, 0x10, 0x7E,       // ORA $7E1001,X
        0x5F, 0x02, 0x10, 0x7E        // EOR $7E1002,X
    });
    memory.write8(0x7E1001, 0x3C);
    memory.write8(0x7E1002, 0x80);
    memory.write8(0x7E1003, 0xFF);
    cpu.reset(memory);
    auto logicalLongX8 = cpu.registers();
    logicalLongX8.a = 0xAB00;
    logicalLongX8.x = 1;
    logicalLongX8.db = 0x12;
    cpu.setRegisters(logicalLongX8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABF0,
          "LDA prepares 8-bit absolute-long,X logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB30,
          "AND absolute long,X adds X to the full 24-bit address");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABB0,
          "ORA absolute long,X reads the indexed address independently of DB");
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB4F,
          "EOR absolute long,X updates the 8-bit accumulator");

    loadProgram(memory, {
        0xC2, 0x20,                   // REP #$20: 16-bit accumulator
        0xA9, 0xF0, 0x0F,             // LDA #$0FF0
        0x3F, 0x00, 0x10, 0x7E,       // AND $7E1000,X
        0x1F, 0x02, 0x10, 0x7E,       // ORA $7E1002,X
        0x5F, 0x04, 0x10, 0x7E        // EOR $7E1004,X
    });
    memory.write8(0x7E1002, 0xF0);
    memory.write8(0x7E1003, 0x00);
    memory.write8(0x7E1004, 0x00);
    memory.write8(0x7E1005, 0x80);
    memory.write8(0x7E1006, 0xFF);
    memory.write8(0x7E1007, 0xFF);
    cpu.reset(memory);
    auto logicalLongX16 = cpu.registers();
    logicalLongX16.emulation = false;
    logicalLongX16.p = 0x00;
    logicalLongX16.x = 2;
    logicalLongX16.db = 0x12;
    cpu.setRegisters(logicalLongX16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute-long,X logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x0FF0,
          "16-bit LDA prepares absolute-long,X logical operations");
    check(cpu.step(memory, error) && cpu.registers().a == 0x00F0,
          "AND absolute long,X reads both bytes at the indexed 24-bit address");
    check(cpu.step(memory, error) && cpu.registers().a == 0x80F0,
          "ORA absolute long,X updates the 16-bit negative flag");
    check(cpu.step(memory, error) && cpu.registers().a == 0x7F0F &&
          (cpu.registers().p & 0x80U) == 0,
          "EOR absolute long,X reads both bytes and updates negative");

    loadProgram(memory, {
        0xAF, 0x00, 0x10, 0x7E,       // LDA $7E1000
        0x8F, 0x02, 0x10, 0x7E,       // STA $7E1002
        0xBF, 0x00, 0x10, 0x7E,       // LDA $7E1000,X
        0x9F, 0x04, 0x10, 0x7E        // STA $7E1004,X
    });
    memory.write8(0x7E1000, 0x5A);
    memory.write8(0x7E1001, 0xA5);
    memory.write8(0x7E1003, 0x3C);
    cpu.reset(memory);
    auto absoluteLongLoads8 = cpu.registers();
    absoluteLongLoads8.a = 0xAB00;
    absoluteLongLoads8.x = 1;
    absoluteLongLoads8.db = 0x12;
    cpu.setRegisters(absoluteLongLoads8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB5A,
          "LDA absolute long loads from the full 24-bit address and preserves A high byte");
    check(cpu.step(memory, error) && memory.read8(0x7E1002) == 0x5A,
          "STA absolute long stores the 8-bit accumulator independently of DB");
    check(cpu.step(memory, error) && cpu.registers().a == 0xABA5,
          "LDA absolute long,X adds X to the 24-bit address");
    check(cpu.step(memory, error) && memory.read8(0x7E1005) == 0xA5,
          "STA absolute long,X stores at the indexed 24-bit address");

    loadProgram(memory, {
        0xC2, 0x20,                   // REP #$20: 16-bit accumulator
        0xAF, 0x00, 0x10, 0x7E,       // LDA $7E1000
        0x8F, 0x02, 0x10, 0x7E,       // STA $7E1002
        0xBF, 0x00, 0x10, 0x7E,       // LDA $7E1000,X
        0x9F, 0x04, 0x10, 0x7E        // STA $7E1004,X
    });
    memory.write8(0x7E1000, 0x34);
    memory.write8(0x7E1001, 0x12);
    memory.write8(0x7E1002, 0x00);
    memory.write8(0x7E1003, 0x00);
    memory.write8(0x7E1004, 0x00);
    memory.write8(0x7E1005, 0x00);
    cpu.reset(memory);
    auto absoluteLongLoads16 = cpu.registers();
    absoluteLongLoads16.emulation = false;
    absoluteLongLoads16.p = 0x00;
    absoluteLongLoads16.x = 2;
    absoluteLongLoads16.db = 0x12;
    cpu.setRegisters(absoluteLongLoads16);
    check(cpu.step(memory, error), "REP enables 16-bit absolute-long accumulator transfers");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA absolute long reads both bytes at the 24-bit address");
    check(cpu.step(memory, error) && memory.read8(0x7E1002) == 0x34 &&
          memory.read8(0x7E1003) == 0x12,
          "16-bit STA absolute long writes both bytes little-endian");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA absolute long,X reads both indexed bytes");
    check(cpu.step(memory, error) && memory.read8(0x7E1006) == 0x34 &&
          memory.read8(0x7E1007) == 0x12,
          "16-bit STA absolute long,X writes both indexed bytes little-endian");

    loadProgram(memory, {
        0xBD, 0x00, 0x10,       // LDA $1000,X
        0xB9, 0x00, 0x10,       // LDA $1000,Y
        0x9D, 0x10, 0x10,       // STA $1010,X
        0x99, 0x10, 0x10        // STA $1010,Y
    });
    memory.write8(0x7E1002, 0x5A);
    memory.write8(0x7E1004, 0xA5);
    cpu.reset(memory);
    auto absoluteIndexed8 = cpu.registers();
    absoluteIndexed8.a = 0xAB00;
    absoluteIndexed8.x = 2;
    absoluteIndexed8.y = 4;
    absoluteIndexed8.db = 0x7E;
    cpu.setRegisters(absoluteIndexed8);
    check(cpu.step(memory, error) && cpu.registers().a == 0xAB5A,
          "LDA absolute,X loads from the X-indexed data-bank address");
    auto setYIndex = cpu.registers();
    setYIndex.y = 4;
    cpu.setRegisters(setYIndex);
    check(cpu.step(memory, error) && cpu.registers().a == 0xABA5,
          "LDA absolute,Y loads from the Y-indexed data-bank address");
    check(cpu.step(memory, error) && memory.read8(0x7E1012) == 0xA5,
          "STA absolute,X stores at the X-indexed address");
    check(cpu.step(memory, error) && memory.read8(0x7E1014) == 0xA5,
          "STA absolute,Y stores at the Y-indexed address");

    loadProgram(memory, {
        0xC2, 0x30,                   // REP #$30: 16-bit accumulator and indexes
        0xBD, 0x00, 0x10,             // LDA $1000,X
        0xBE, 0x10, 0x10,             // LDX $1010,Y
        0xBC, 0x20, 0x10,             // LDY $1020,X
        0x9E, 0x30, 0x10,             // STX $1030,Y
        0x9C, 0x40, 0x10              // STY $1040,X
    });
    memory.write8(0x7E1002, 0x34);
    memory.write8(0x7E1003, 0x12);
    memory.write8(0x7E1014, 0x78);
    memory.write8(0x7E1015, 0x56);
    memory.write8(0x7E1022, 0xBC);
    memory.write8(0x7E1023, 0x9A);
    cpu.reset(memory);
    auto absoluteIndexed16 = cpu.registers();
    absoluteIndexed16.emulation = false;
    absoluteIndexed16.p = 0x00;
    absoluteIndexed16.x = 2;
    absoluteIndexed16.y = 4;
    absoluteIndexed16.db = 0x7E;
    cpu.setRegisters(absoluteIndexed16);
    check(cpu.step(memory, error), "REP enables 16-bit indexed absolute transfers");
    check(cpu.step(memory, error) && cpu.registers().a == 0x1234,
          "16-bit LDA absolute,X reads both indexed bytes");
    check(cpu.step(memory, error) && cpu.registers().x == 0x5678,
          "16-bit LDX absolute,Y reads both indexed bytes");
    auto restoreIndex16 = cpu.registers();
    restoreIndex16.x = 2;
    cpu.setRegisters(restoreIndex16);
    check(cpu.step(memory, error) && cpu.registers().y == 0x9ABC,
          "16-bit LDY absolute,X reads both indexed bytes");
    auto storeIndexes16 = cpu.registers();
    storeIndexes16.x = 2;
    storeIndexes16.y = 4;
    cpu.setRegisters(storeIndexes16);
    check(cpu.step(memory, error) && memory.read8(0x7E1034) == 0x02 &&
          memory.read8(0x7E1035) == 0x00,
          "16-bit STX absolute,Y writes the X register little-endian");
    check(cpu.step(memory, error) && memory.read8(0x7E1042) == 0x04 &&
          memory.read8(0x7E1043) == 0x00,
          "16-bit STY absolute,X writes the Y register little-endian");

    loadProgram(memory, {
        0xA9, 0x50,             // LDA #$50
        0xC5, 0x20,             // CMP $20
        0xD5, 0x1F,             // CMP $1F,X
        0xA2, 0x40,             // LDX #$40
        0xE4, 0x22,             // CPX $22
        0xA0, 0x60,             // LDY #$60
        0xC4, 0x23              // CPY $23
    });
    memory.write8(0x001020, 0x50);
    memory.write8(0x001022, 0x40);
    memory.write8(0x001023, 0x60);
    cpu.reset(memory);
    auto directCompare8 = cpu.registers();
    directCompare8.d = 0x1000;
    directCompare8.x = 1;
    cpu.setRegisters(directCompare8);
    check(cpu.step(memory, error), "LDA prepares 8-bit direct-page compare tests");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP direct page sets carry and zero for equal values");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP direct page,X uses D plus operand plus X");
    check(cpu.step(memory, error) && cpu.registers().x == 0x40,
          "LDX prepares CPX direct page");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPX direct page compares the 8-bit X register");
    check(cpu.step(memory, error) && cpu.registers().y == 0x60,
          "LDY prepares CPY direct page");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPY direct page compares the 8-bit Y register");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xC5, 0x20,             // CMP $20
        0xE4, 0x22,             // CPX $22
        0xC4, 0x24,             // CPY $24
        0xD5, 0x1E              // CMP $1E,X
    });
    memory.write8(0x001020, 0x34);
    memory.write8(0x001021, 0x12);
    memory.write8(0x001022, 0x78);
    memory.write8(0x001023, 0x56);
    memory.write8(0x001024, 0xBC);
    memory.write8(0x001025, 0x9A);
    cpu.reset(memory);
    auto directCompare16 = cpu.registers();
    directCompare16.emulation = false;
    directCompare16.p = 0x00;
    directCompare16.a = 0x1234;
    directCompare16.x = 0x5678;
    directCompare16.y = 0x9ABC;
    directCompare16.d = 0x1000;
    cpu.setRegisters(directCompare16);
    check(cpu.step(memory, error), "REP enables 16-bit direct-page compare operations");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP direct page compares both accumulator bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPX direct page compares both X register bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPY direct page compares both Y register bytes");
    auto directCompareIndex16 = cpu.registers();
    directCompareIndex16.x = 2;
    cpu.setRegisters(directCompareIndex16);
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP direct page,X compares both bytes at the indexed address");

    loadProgram(memory, {
        0xA9, 0x50,             // LDA #$50
        0xCD, 0x00, 0x10,       // CMP $1000
        0xDD, 0xFF, 0x0F,       // CMP $0FFF,X
        0xD9, 0xFE, 0x0F,       // CMP $0FFE,Y
        0xA2, 0x40,             // LDX #$40
        0xEC, 0x02, 0x10,       // CPX $1002
        0xA0, 0x60,             // LDY #$60
        0xCC, 0x03, 0x10        // CPY $1003
    });
    memory.write8(0x7E1000, 0x50);
    memory.write8(0x7E1002, 0x40);
    memory.write8(0x7E1003, 0x60);
    cpu.reset(memory);
    auto compare8 = cpu.registers();
    compare8.db = 0x7E;
    compare8.x = 1;
    compare8.y = 2;
    cpu.setRegisters(compare8);
    check(cpu.step(memory, error), "LDA prepares 8-bit memory compare tests");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute sets carry and zero when values are equal");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute,X compares from the X-indexed data-bank address");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute,Y compares from the Y-indexed data-bank address");
    check(cpu.step(memory, error) && cpu.registers().x == 0x40,
          "LDX prepares CPX absolute");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPX absolute compares the 8-bit X register");
    check(cpu.step(memory, error) && cpu.registers().y == 0x60,
          "LDY prepares CPY absolute");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPY absolute compares the 8-bit Y register");

    loadProgram(memory, {
        0xC2, 0x30,             // REP #$30: 16-bit accumulator and indexes
        0xCD, 0x00, 0x10,       // CMP $1000
        0xEC, 0x02, 0x10,       // CPX $1002
        0xCC, 0x04, 0x10,       // CPY $1004
        0xDD, 0x88, 0xB9,       // CMP $B988,X -> $1000 with X=$5678
        0xD9, 0x44, 0x75        // CMP $7544,Y -> $1000 with Y=$9ABC
    });
    memory.write8(0x7E1000, 0x34);
    memory.write8(0x7E1001, 0x12);
    memory.write8(0x7E1002, 0x78);
    memory.write8(0x7E1003, 0x56);
    memory.write8(0x7E1004, 0xBC);
    memory.write8(0x7E1005, 0x9A);
    cpu.reset(memory);
    auto memoryCompare16 = cpu.registers();
    memoryCompare16.emulation = false;
    memoryCompare16.p = 0x00;
    memoryCompare16.a = 0x1234;
    memoryCompare16.x = 0x5678;
    memoryCompare16.y = 0x9ABC;
    memoryCompare16.db = 0x7E;
    cpu.setRegisters(memoryCompare16);
    check(cpu.step(memory, error), "REP enables 16-bit memory compare operations");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute compares both accumulator bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPX absolute compares both X register bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CPY absolute compares both Y register bytes");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute,X compares both bytes at the indexed address");
    check(cpu.step(memory, error) && (cpu.registers().p & 0x03U) == 0x03U,
          "CMP absolute,Y compares both bytes at the indexed address");

    std::cout << "All 65C816 tests passed.\\n";
    return 0;
}
