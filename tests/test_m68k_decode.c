#include "m68k_analyze.h"
#include "m68k_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static NgProgramRom make_rom_at(const unsigned char *bytes, uint32_t size, uint32_t addr) {
    NgProgramRom rom;
    rom.size = addr + size;
    rom.data = (uint8_t *)calloc(rom.size ? rom.size : 1u, 1);
    if (rom.data) {
        memcpy(rom.data + addr, bytes, size);
    }
    return rom;
}

static int decode_one(const unsigned char *bytes, uint32_t size,
                      uint32_t addr, NgM68kInstr *instr) {
    NgProgramRom rom = make_rom_at(bytes, size, addr);
    CHECK(rom.data != NULL);
    int ok = ng_m68k_decode(&rom, addr, instr);
    ng_program_rom_free(&rom);
    return ok;
}

int main(void) {
    NgM68kInstr instr;

    {
        const unsigned char bytes[] = { 0x41, 0xFA, 0x01, 0x24 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x0007CCu, &instr));
        CHECK(instr.mnemonic == NG_M68K_LEA);
        CHECK(instr.byte_length == 4);
        CHECK(instr.reg == 0);
        CHECK(instr.target == 0x000008F4u);
    }

    {
        const unsigned char bytes[] = { 0x23, 0xC8, 0x00, 0x10, 0x6E, 0xA8 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 6);
        CHECK(instr.form == NG_M68K_FORM_AREG_TO_ABS);
        CHECK(instr.reg == 0);
        CHECK(instr.absolute_addr == 0x00106EA8u);
    }

    {
        const unsigned char bytes[] = { 0x23, 0xCE, 0x00, 0x10, 0x00, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 6);
        CHECK(instr.form == NG_M68K_FORM_AREG_TO_ABS);
        CHECK(instr.reg == 6);
        CHECK(instr.absolute_addr == 0x00100080u);
    }

    {
        const unsigned char bytes[] = { 0x08, 0xB9, 0x00, 0x07, 0x00, 0x10, 0xFD, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BCLR);
        CHECK(instr.byte_length == 8);
        CHECK(instr.immediate == 7);
        CHECK(instr.absolute_addr == 0x0010FD80u);
    }

    {
        const unsigned char bytes[] = { 0x33, 0xFC, 0x00, 0x07, 0x00, 0x3C, 0x00, 0x0C };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 8);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_ABS);
        CHECK(instr.immediate == 7);
        CHECK(instr.absolute_addr == 0x003C000Cu);
    }

    {
        const unsigned char bytes[] = { 0x13, 0xFC, 0x00, 0x80, 0x00, 0x10, 0xFD, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 8);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_ABS);
        CHECK(instr.immediate == 0x80u);
        CHECK(instr.absolute_addr == 0x0010FD80u);
    }

    {
        const unsigned char bytes[] = { 0x13, 0xC2, 0x00, 0x10, 0x00, 0x18 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_DREG_TO_ABS);
        CHECK(instr.reg == 2);
        CHECK(instr.absolute_addr == 0x00100018u);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x7C, 0xF8, 0xFF };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI_TO_SR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.immediate == 0xF8FFu);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x2E, 0x00, 0x0F, 0x0F, 0x7A };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_AREG_DISP);
        CHECK(instr.reg == 6);
        CHECK(instr.immediate == 0x0Fu);
        CHECK(instr.displacement == 0x0F7A);
    }

    {
        const unsigned char bytes[] = { 0x10, 0x39, 0x00, 0x10, 0xFD, 0xAE };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 6);
        CHECK(instr.form == NG_M68K_FORM_ABS_TO_DREG);
        CHECK(instr.reg == 0);
        CHECK(instr.absolute_addr == 0x0010FDAEu);
    }

    {
        const unsigned char bytes[] = { 0xD0, 0x40 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADD);
        CHECK(instr.byte_length == 2);
        CHECK(instr.form == NG_M68K_FORM_DREG_TO_DREG);
        CHECK(instr.src_reg == 0);
        CHECK(instr.reg == 0);
    }

    {
        const unsigned char bytes[] = { 0x20, 0x7B, 0x00, 0x04 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x0007F6u, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEA);
        CHECK(instr.byte_length == 4);
        CHECK(instr.form == NG_M68K_FORM_PC_INDEX_TO_AREG);
        CHECK(instr.reg == 0);
        CHECK(instr.src_reg == 0);
        CHECK(instr.displacement == 4);
        CHECK(instr.target == 0x0007FCu);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0xD0 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_JMP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.form == NG_M68K_FORM_AREG_INDIRECT);
        CHECK(instr.reg == 0);
    }

    {
        const unsigned char load_bytes[] = { 0x20, 0x7B, 0x00, 0x04 };
        const unsigned char jump_bytes[] = { 0x4E, 0xD0 };
        NgM68kInstr load;
        NgM68kInstr jump;
        NgM68kJumpTablePattern pattern;
        CHECK(decode_one(load_bytes, sizeof(load_bytes), 0x0007F6u, &load));
        CHECK(decode_one(jump_bytes, sizeof(jump_bytes), 0x0007FAu, &jump));
        CHECK(ng_m68k_match_pc_index_jump_table(&load, &jump, &pattern));
        CHECK(pattern.table_addr == 0x0007FCu);
        CHECK(pattern.index_reg == 0);
        CHECK(pattern.target_reg == 0);
        CHECK(pattern.entry_size == 4);

        jump.reg = 1;
        CHECK(!ng_m68k_match_pc_index_jump_table(&load, &jump, &pattern));
        jump.reg = 0;
        jump.mnemonic = NG_M68K_RTS;
        CHECK(!ng_m68k_match_pc_index_jump_table(&load, &jump, &pattern));
    }

    {
        const unsigned char bytes[] = { 0x70, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEQ);
        CHECK(instr.byte_length == 2);
        CHECK(instr.reg == 0);
        CHECK((int32_t)instr.immediate == -128);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0xB9, 0x00, 0x02, 0x78, 0x3A };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_JSR);
        CHECK(instr.byte_length == 6);
        CHECK(instr.target == 0x0002783Au);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0xFA, 0x00, 0x2A };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000832u, &instr));
        CHECK(instr.mnemonic == NG_M68K_JMP);
        CHECK(instr.byte_length == 4);
        CHECK(instr.form == NG_M68K_FORM_PC_RELATIVE);
        CHECK(instr.displacement == 0x2A);
        CHECK(instr.target == 0x00085Eu);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0xBA, 0x01, 0x42 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000836u, &instr));
        CHECK(instr.mnemonic == NG_M68K_JSR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.form == NG_M68K_FORM_PC_RELATIVE);
        CHECK(instr.displacement == 0x0142);
        CHECK(instr.target == 0x00097Au);
    }

    {
        const unsigned char bytes[] = { 0x20, 0x39, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000812u, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_ABS_TO_DREG);
        CHECK(instr.reg == 0);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x14, 0x3C, 0x00, 0x7F };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000812u, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x7Fu);
    }

    {
        const unsigned char bytes[] = { 0x0C, 0x02, 0x00, 0x7F };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000812u, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x7Fu);
    }

    {
        const unsigned char bytes[] = { 0x42, 0x79, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x42, 0x39, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x42, 0x03 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 3);
    }

    {
        const unsigned char bytes[] = { 0x42, 0xB9, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x4A, 0x39, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000996u, &instr));
        CHECK(instr.mnemonic == NG_M68K_TST);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x4A, 0xB9, 0x00, 0x10, 0xFE, 0x80 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000996u, &instr));
        CHECK(instr.mnemonic == NG_M68K_TST);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
    }

    {
        const unsigned char bytes[] = { 0x61, 0x00, 0x00, 0x0E };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000852u, &instr));
        CHECK(instr.mnemonic == NG_M68K_BSR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.target == 0x000862u);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x75 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_RTS);
        CHECK(instr.byte_length == 2);
    }

    return 0;
}
