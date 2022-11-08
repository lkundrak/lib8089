/*
 * Intel 8089 I/O processor emulator and disassembler.
 * Copyright (C) 2022  Lubomir Rintel <lkundrak@v3.sk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

enum i89_regs { GA, GB, GC, BC, TP, IX, CC, MC, PP, NUM_REGS, BAD_REG };

enum i89_flags {
	I89_CHECK	= 0x01,
	I89_PRINT_ADDR	= 0x02,
	I89_PRINT_DATA	= 0x04,
	I89_PRINT_INSN	= 0x08,
	I89_EXEC	= 0x10,
	_I89_STORE	= 0x80,
};

struct i89 {
	uint32_t cb;
	struct {
		uint32_t regs[NUM_REGS];
		unsigned tags:NUM_REGS;
		unsigned wid:2;
		unsigned xfer:1;
	} chan[2];

	void (*sintr)(struct i89 *iop);

	uint8_t (*read8)(struct i89 *iop, uint32_t addr);
	uint16_t (*read16)(struct i89 *iop, uint32_t addr);
	void (*write8)(struct i89 *iop, uint32_t addr, uint8_t value);
	void (*write16)(struct i89 *iop, uint32_t addr, uint16_t value);

	uint8_t (*in8)(struct i89 *iop, uint16_t addr);
	uint16_t (*in16)(struct i89 *iop, uint16_t addr);
	void (*out8)(struct i89 *iop, uint16_t addr, uint8_t value);
	void (*out16)(struct i89 *iop, uint16_t addr, uint16_t value);
};

void i89_dump (struct i89 *iop);
void i89_attn (struct i89 *iop, int ch);
int i89_insn (struct i89 *iop, enum i89_flags flags);
