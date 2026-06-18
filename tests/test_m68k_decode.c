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
        const unsigned char bytes[] = { 0x47, 0xE8, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_LEA);
        CHECK(instr.byte_length == 4);
        CHECK(instr.reg == 3);
        CHECK(instr.src.mode == NG_M68K_EA_ADISP);
        CHECK(instr.src.reg == 0);
        CHECK(instr.src.displacement == 0x10);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "LEA ($10,A0),A3") == 0);
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
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BCLR);
        CHECK(instr.byte_length == 8);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.dst.absolute_addr == 0x0010FD80u);
        CHECK(instr.absolute_addr == 0x0010FD80u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BCLR #7,$10FD80") == 0);
    }

    {
        const unsigned char bytes[] = { 0x08, 0x39, 0x00, 0x07, 0x00, 0x10, 0xFD, 0x80 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BTST);
        CHECK(instr.byte_length == 8);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.absolute_addr == 0x0010FD80u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BTST #7,$10FD80") == 0);
    }

    {
        const unsigned char bytes[] = { 0x08, 0x42, 0x00, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BCHG);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 4);
        CHECK(instr.immediate == 1);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BCHG #1,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x01, 0xC2 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BSET);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_DREG_TO_DREG);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BSET D0,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x08, 0xD8, 0x00, 0x00 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BSET);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BSET #0,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x03, 0xD8 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_BSET);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "BSET D1,(A0)+") == 0);
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
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI_TO_SR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 0xF8FFu);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ANDI #$F8FF,SR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x00, 0x3C, 0x00, 0x04 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ORI_TO_CCR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x04u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ORI #$04,CCR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x00, 0x7C, 0x01, 0x00 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ORI_TO_SR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 0x0100u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ORI #$0100,SR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x3C, 0x00, 0x1F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI_TO_CCR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x1Fu);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ANDI #$1F,CCR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x0A, 0x3C, 0x00, 0x04 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EORI_TO_CCR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x04u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EORI #$04,CCR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x0A, 0x7C, 0x01, 0x00 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EORI_TO_SR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 0x0100u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EORI #$0100,SR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x2E, 0x00, 0x0F, 0x0F, 0x7A };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_AREG_DISP);
        CHECK(instr.reg == 6);
        CHECK(instr.immediate == 0x0Fu);
        CHECK(instr.displacement == 0x0F7A);
        CHECK(instr.dst.mode == NG_M68K_EA_ADISP);
        CHECK(instr.dst.reg == 6);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ANDI.B #$F,($F7A,A6)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x42, 0x0F, 0x0F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x0F0Fu);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ANDI.W #$F0F,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x00, 0x42, 0x00, 0xF0 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ORI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x00F0u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ORI.W #$F0,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x06, 0x42, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADDI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x0010u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADDI.W #$10,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x02, 0x18, 0x00, 0x0F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ANDI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x0Fu);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ANDI.B #$F,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x0A, 0x18, 0x00, 0x0F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EORI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x0Fu);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EORI.B #$F,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x04, 0x18, 0x00, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUBI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.immediate == 0x01u);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUBI.B #$1,(A0)+") == 0);
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
        const unsigned char bytes[] = { 0x18, 0x2E, 0x0F, 0x7A };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_AREG_DISP);
        CHECK(instr.reg == 4);
        CHECK(instr.src_reg == 6);
        CHECK(instr.displacement == 0x0F7A);
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
        const unsigned char bytes[] = { 0xD0, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADD);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADD.B D1,D0") == 0);
    }

    {
        const unsigned char bytes[] = { 0x94, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUB);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUB.B D1,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0xB0, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMP.B D1,D0") == 0);
    }

    {
        const unsigned char bytes[] = { 0xB0, 0x39, 0x00, 0x10, 0xFE, 0x80 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMP);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.src.absolute_addr == 0x0010FE80u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMP.B $10FE80,D0") == 0);
    }

    {
        const unsigned char bytes[] = { 0xD4, 0xFC, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADDA);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x10u);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADDA.W #$10,A2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x95, 0xFC, 0x00, 0x00, 0x00, 0x08 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUBA);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x08u);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUBA.L #$8,A2") == 0);
    }

    {
        const unsigned char bytes[] = { 0xB5, 0xFC, 0x00, 0x00, 0x01, 0x08 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPA);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x108u);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMPA.L #$108,A2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x86, 0x07 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_OR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "OR.B D7,D3") == 0);
    }

    {
        const unsigned char bytes[] = { 0xC6, 0x07 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_AND);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "AND.B D7,D3") == 0);
    }

    {
        const unsigned char bytes[] = { 0xBF, 0x03 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EOR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EOR.B D7,D3") == 0);
    }

    {
        const unsigned char bytes[] = { 0x8F, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_OR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "OR.B D7,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0xCF, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_AND);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "AND.B D7,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0xBF, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EOR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EOR.B D7,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0xDF, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADD);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADD.B D7,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x9F, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUB);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 7);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUB.B D7,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0xCE, 0xFC, 0x00, 0x04 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MULU);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 4u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MULU.W #$4,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0xCF, 0xFC, 0xFF, 0xFE };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MULS);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0xFFFEu);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MULS.W #$FFFE,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0xC1, 0x41 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EXG);
        CHECK(instr.byte_length == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EXG D0,D1") == 0);
    }

    {
        const unsigned char bytes[] = { 0x8E, 0xFC, 0x00, 0x04 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_DIVU);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 4u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "DIVU.W #$4,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0x8F, 0xFC, 0xFF, 0xFE };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_DIVS);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0xFFFEu);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "DIVS.W #$FFFE,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0xE3, 0x4F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_LSL);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 1u);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "LSL.W #1,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0xE2, 0x6F };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_LSR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 7);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "LSR.W D1,D7") == 0);
    }

    {
        const unsigned char bytes[] = { 0xE3, 0xF9, 0x00, 0x00, 0x01, 0x8A };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_LSL);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 1u);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.dst.absolute_addr == 0x0000018Au);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "LSL.W $00018A") == 0);
    }

    {
        const unsigned char bytes[] = { 0xD1, 0x01 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADDX);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_DREG_TO_DREG);
        CHECK(instr.src_reg == 1);
        CHECK(instr.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADDX.B D1,D0") == 0);
    }

    {
        const unsigned char bytes[] = { 0x93, 0x40 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUBX);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUBX.W D0,D1") == 0);
    }

    {
        const unsigned char bytes[] = { 0xC3, 0x00 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ABCD);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ABCD.B D0,D1") == 0);
    }

    {
        const unsigned char bytes[] = { 0x89, 0x09 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SBCD);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_APRE);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_APRE);
        CHECK(instr.dst.reg == 4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SBCD.B -(A1),-(A4)") == 0);
    }

    {
        const unsigned char bytes[] = { 0xDD, 0x4D };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADDX);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_APRE);
        CHECK(instr.src.reg == 5);
        CHECK(instr.dst.mode == NG_M68K_EA_APRE);
        CHECK(instr.dst.reg == 6);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADDX.W -(A5),-(A6)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x3A, 0x06 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_DREG_TO_DREG);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 6);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 5);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.W D6,D5") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0xD4, 0x00, 0x03 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEM);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 4);
        CHECK(instr.immediate == 0x0003u);
        CHECK(instr.dst.mode == NG_M68K_EA_AIND);
        CHECK(instr.dst.reg == 4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEM.L #$0003,(A4)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4C, 0xD4, 0x00, 0x03 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEM);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 4);
        CHECK(instr.immediate == 0x0003u);
        CHECK(instr.src.mode == NG_M68K_EA_AIND);
        CHECK(instr.src.reg == 4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEM.L (A4),#$0003") == 0);
    }

    {
        const unsigned char bytes[] = { 0x44, 0xFC, 0x00, 0x1B };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE_CCR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x001Bu);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE #$1B,CCR") == 0);
    }

    {
        const unsigned char bytes[] = { 0x40, 0xF9, 0x00, 0x00, 0x01, 0x88 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE_SR);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 2);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.dst.absolute_addr == 0x00000188u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE SR,$000188") == 0);
    }

    {
        const unsigned char bytes[] = { 0x30, 0x85 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 5);
        CHECK(instr.dst.mode == NG_M68K_EA_AIND);
        CHECK(instr.dst.reg == 0);
    }

    {
        const unsigned char bytes[] = { 0x12, 0xD8 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_APOST);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.B (A0)+,(A1)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x10, 0xC1 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.B D1,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x20, 0xC1 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.L D1,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x20, 0x7C, 0x12, 0x34, 0x56, 0x78 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEA);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x12345678u);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEA.L #$12345678,A0") == 0);
    }

    {
        const unsigned char bytes[] = { 0x22, 0x48 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEA);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_AREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEA.L A0,A1") == 0);
    }

    {
        const unsigned char bytes[] = { 0x32, 0xC0 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.W D0,(A1)+") == 0);
    }

    {
        const unsigned char bytes[] = {
            0x21, 0xBC, 0x12, 0x34, 0x56, 0x78, 0xA8, 0x0C
        };
        char text[80];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE);
        CHECK(instr.byte_length == 8);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x12345678u);
        CHECK(instr.dst.mode == NG_M68K_EA_AINDEX);
        CHECK(instr.dst.reg == 0);
        CHECK(instr.dst.index_is_addr == 1);
        CHECK(instr.dst.index_reg == 2);
        CHECK(instr.dst.index_is_long == 1);
        CHECK(instr.dst.displacement == 0x0C);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE.L #$12345678,($C,A0,A2.L)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x59, 0x04 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUBQ);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 4);
        CHECK(instr.immediate == 4);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUBQ.B #4,D4") == 0);
    }

    {
        const unsigned char bytes[] = { 0x54, 0x42 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_ADDQ);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 2);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "ADDQ.W #2,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x51, 0x98 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SUBQ);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.immediate == 8);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SUBQ.L #8,(A0)+") == 0);
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
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_JMP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.form == NG_M68K_FORM_AREG_INDIRECT);
        CHECK(instr.reg == 0);
        CHECK(instr.src.mode == NG_M68K_EA_AIND);
        CHECK(instr.src.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "JMP (A0)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x90 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_JSR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.form == NG_M68K_FORM_AREG_INDIRECT);
        CHECK(instr.reg == 0);
        CHECK(instr.src.mode == NG_M68K_EA_AIND);
        CHECK(instr.src.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "JSR (A0)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0xEA, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_JMP);
        CHECK(instr.byte_length == 4);
        CHECK(instr.src.mode == NG_M68K_EA_ADISP);
        CHECK(instr.src.reg == 2);
        CHECK(instr.src.displacement == 0x10);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "JMP ($10,A2)") == 0);
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
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0x000812u, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_IMM_TO_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.immediate == 0x7Fu);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMPI.B #$7F,D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x0C, 0x58, 0x00, 0x5A };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPI);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.immediate == 0x005Au);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMPI.W #$5A,(A0)+") == 0);
    }

    {
        const unsigned char bytes[] = {
            0x0C, 0xB0, 0x12, 0x34, 0x56, 0x78, 0xA8, 0x0C
        };
        char text[80];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPI);
        CHECK(instr.byte_length == 8);
        CHECK(instr.size == 4);
        CHECK(instr.immediate == 0x12345678u);
        CHECK(instr.dst.mode == NG_M68K_EA_AINDEX);
        CHECK(instr.dst.reg == 0);
        CHECK(instr.dst.index_is_addr == 1);
        CHECK(instr.dst.index_reg == 2);
        CHECK(instr.dst.index_is_long == 1);
        CHECK(instr.dst.displacement == 0x0C);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMPI.L #$12345678,($C,A0,A2.L)") == 0);
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
        const unsigned char bytes[] = { 0x42, 0x68, 0x00, 0x44 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.dst.mode == NG_M68K_EA_ADISP);
        CHECK(instr.dst.reg == 0);
        CHECK(instr.dst.displacement == 0x44);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CLR.W ($44,A0)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x42, 0x98 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0x00081Cu, &instr));
        CHECK(instr.mnemonic == NG_M68K_CLR);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CLR.L (A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x40, 0x39, 0x00, 0x00, 0x01, 0x8C };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_NEGX);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0000018Cu);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "NEGX.B $00018C") == 0);
    }

    {
        const unsigned char bytes[] = { 0x41, 0xBC, 0x00, 0x0A };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CHK);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_IMM);
        CHECK(instr.src.immediate == 0x000Au);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CHK.W #$A,D0") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4A, 0xF9, 0x00, 0x00, 0x01, 0x8D };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_TAS);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.dst.absolute_addr == 0x0000018Du);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "TAS $00018D") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0x39, 0x00, 0x00, 0x01, 0x94 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_NBCD);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_ABS_L);
        CHECK(instr.dst.absolute_addr == 0x00000194u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "NBCD.B $000194") == 0);
    }

    {
        const unsigned char bytes[] = { 0xBD, 0x4D };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_CMPM);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_APOST);
        CHECK(instr.src.reg == 5);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 6);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "CMPM.W (A5)+,(A6)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x65 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE_USP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_AREG);
        CHECK(instr.src.reg == 5);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE A5,USP") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x6C };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVE_USP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.dst.mode == NG_M68K_EA_AREG);
        CHECK(instr.dst.reg == 4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVE USP,A4") == 0);
    }

    {
        const unsigned char bytes[] = { 0x01, 0x88, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEP);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 0);
        CHECK(instr.dst.mode == NG_M68K_EA_ADISP);
        CHECK(instr.dst.reg == 0);
        CHECK(instr.dst.displacement == 0x10);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEP.W D0,($10,A0)") == 0);
    }

    {
        const unsigned char bytes[] = { 0x03, 0x49, 0xFF, 0xFC };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_MOVEP);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_ADISP);
        CHECK(instr.src.reg == 1);
        CHECK(instr.src.displacement == -4);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "MOVEP.L ($FFFC,A1),D1") == 0);
    }

    {
        const unsigned char bytes[] = { 0x44, 0x42 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_NEG);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "NEG.W D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x46, 0x18 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_NOT);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 1);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "NOT.B (A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x46, 0x79, 0x00, 0x10, 0xFE, 0x80 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_NOT);
        CHECK(instr.byte_length == 6);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_ABS);
        CHECK(instr.absolute_addr == 0x0010FE80u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "NOT.W $10FE80") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0x83 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EXT);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 3);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EXT.W D3") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0xC3 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_EXT);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 3);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 3);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "EXT.L D3") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0x42 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SWAP);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.reg == 2);
        CHECK(instr.dst.mode == NG_M68K_EA_DREG);
        CHECK(instr.dst.reg == 2);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "SWAP D2") == 0);
    }

    {
        const unsigned char bytes[] = { 0x48, 0x68, 0x00, 0x10 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_PEA);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 4);
        CHECK(instr.src.mode == NG_M68K_EA_ADISP);
        CHECK(instr.src.reg == 0);
        CHECK(instr.src.displacement == 0x10);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "PEA ($10,A0)") == 0);
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
        const unsigned char bytes[] = { 0x4A, 0x2E, 0x0F, 0x7A };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000996u, &instr));
        CHECK(instr.mnemonic == NG_M68K_TST);
        CHECK(instr.byte_length == 4);
        CHECK(instr.size == 1);
        CHECK(instr.form == NG_M68K_FORM_AREG_DISP);
        CHECK(instr.reg == 6);
        CHECK(instr.displacement == 0x0F7A);
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
        const unsigned char bytes[] = { 0x4A, 0x81 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_TST);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 4);
        CHECK(instr.form == NG_M68K_FORM_DREG);
        CHECK(instr.src.mode == NG_M68K_EA_DREG);
        CHECK(instr.src.reg == 1);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "TST.L D1") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4A, 0x58 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_TST);
        CHECK(instr.byte_length == 2);
        CHECK(instr.size == 2);
        CHECK(instr.src.mode == NG_M68K_EA_APOST);
        CHECK(instr.src.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "TST.W (A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x61, 0x00, 0x00, 0x0E };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000852u, &instr));
        CHECK(instr.mnemonic == NG_M68K_BSR);
        CHECK(instr.byte_length == 4);
        CHECK(instr.target == 0x000862u);
    }

    {
        const unsigned char bytes[] = { 0x6B, 0x08 };
        CHECK(decode_one(bytes, sizeof(bytes), 0x000100u, &instr));
        CHECK(instr.mnemonic == NG_M68K_BCC);
        CHECK(instr.byte_length == 2);
        CHECK(instr.condition == 11u);
        CHECK(instr.target == 0x00010Au);
    }

    {
        const unsigned char bytes[] = { 0x5B, 0xD8 };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_SCC);
        CHECK(instr.byte_length == 2);
        CHECK(instr.condition == 11u);
        CHECK(instr.dst.mode == NG_M68K_EA_APOST);
        CHECK(instr.dst.reg == 0);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "Scc.B (A0)+") == 0);
    }

    {
        const unsigned char bytes[] = { 0x51, 0xCF, 0xFF, 0xEE };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0x000110u, &instr));
        CHECK(instr.mnemonic == NG_M68K_DBCC);
        CHECK(instr.byte_length == 4);
        CHECK(instr.condition == 1u);
        CHECK(instr.reg == 7);
        CHECK(instr.displacement == -18);
        CHECK(instr.target == 0x000100u);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "DBcc.1 D7,$000100") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x55, 0xFF, 0xFC };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_LINK);
        CHECK(instr.byte_length == 4);
        CHECK(instr.reg == 5);
        CHECK(instr.displacement == -4);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "LINK A5,#-4") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x5D };
        char text[64];
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_UNLK);
        CHECK(instr.byte_length == 2);
        CHECK(instr.reg == 5);
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        CHECK(strcmp(text, "UNLK A5") == 0);
    }

    {
        const unsigned char bytes[] = { 0x4E, 0x75 };
        CHECK(decode_one(bytes, sizeof(bytes), 0, &instr));
        CHECK(instr.mnemonic == NG_M68K_RTS);
        CHECK(instr.byte_length == 2);
    }

    return 0;
}
