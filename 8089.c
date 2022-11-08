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

#include <stdint.h>
#include <stdio.h>

#include "8089.h"

static const enum i89_regs mmregs[4] = { GA, GB, GC, PP };
static const enum i89_regs pppregs[8] = { GA, GB, GC, BAD_REG, TP, BAD_REG, BAD_REG, BAD_REG };

#define wb      ((insn & 0x0018) >> 3)
#define dd	wb
#define rrr     ((insn & 0x00e0) >> 5)
#define ppp     rrr
#define bbb     rrr
#define aa      ((insn & 0x0006) >> 1)
#define w       ((insn & 0x0001) >> 0)

#define opcode  ((insn & 0xfc00) >> 10)
#define mm      ((insn & 0x0300) >> 8)

#define I89_HAVE_PRINT
#define I89_HAVE_CHECK

#if defined(I89_HAVE_CHECK) || defined(I89_HAVE_PRINT)

/*
 * Register names.
 */

static const char *regn[] = { "ga", "gb", "gc", "bc", "tp", "ix", "cc", "mc", /**/ "pp" };

/*
 * Instruction description.
 * Used for validation and disassembly printout.
 */

#define RRR	1 << 5
#define BBB	2 << 5
#define PPP	3 << 5
#define WB	1 << 3
#define DD	2 << 3
#define AA	1 << 1
#define W	1 << 0

struct op {
	uint8_t flags;
	uint8_t extra;
	const char *mnem;
};

struct op ops[] = {
[ 0] = { 0xe0          , 0x00, NULL },	 	/* nop, sintr, xfer, wid */
[ 2] = { PPP           , 0x11, "lpdi" },	/* Load Pointer PPP Immediate 4 Bytes */
[ 8] = { RRR| WB|     W, 0x00, "add" },		/* ADD Immediate to Register */
[ 9] = { RRR| WB|     W, 0x00, "or" },		/* OR Register with Immediate */
[10] = { RRR| WB|     W, 0x00, "and" },		/* AND Regisier with Immediate */
[11] = { RRR           , 0x00, "not" },		/* Complement Register */
[12] = { RRR| WB|     W, 0x00, "mov" },		/* Load Register RRR Immediate (Byte), Sign Extend */
[14] = { RRR           , 0x00, "inc" },		/* Increment Register */
[15] = { RRR           , 0x00, "dec" },		/* Decrement Register */
[16] = { RRR| DD       , 0x00, "jnz" },		/* Jump on Non-Zero Register */
[17] = { RRR| DD       , 0x00, "jz" },		/* Jump on Zero Register */
[18] = {              0, 0x20, "hlt" },		/* Halt Channel Execution */
[19] = {      WB| AA| W, 0x00, "mov" },		/* Move Immediate to Addressed Location */
[32] = { RRR|     AA| W, 0x00, "mov" },		/* Load Register RRR from Addressed Location */
[33] = { RRR|     AA| W, 0x00, "mov" },		/* Store Contents of Register RRR in Addressed Location */
[34] = { PPP|     AA   , 0x01, "lpd " },	/* Load Pointer PPP from Addressed Location */
[35] = { PPP|     AA   , 0x01, "movp" },	/* Restore Pointer */
[36] = {          AA| W, 0x00, NULL },		/* Move from Source to Destination - Source */
[37] = {          AA   , 0x18, "tsl" },		/* Test and Set Lock */
[38] = { PPP|     AA   , 0x01, "movp" },	/* Store Contents of Pointer PPP in Addressed Location */
[39] = {      DD| AA   , 0x81, "call" },	/* Call Unconditional */
[40] = { RRR|     AA| W, 0x00, "add" },		/* ADD Memory to Register */
[41] = { RRR|     AA| W, 0x00, "or" },		/* OR Register with Memory */
[42] = { RRR|     AA| W, 0x00, "and" },		/* AND Register with Memory */
[43] = { RRR|     AA| W, 0x00, "not" },		/* Complement Memory, Place in Register */
[44] = {      DD| AA   , 0x00, "jmce" },	/* Mask/Compare and Jump on Equal */
[45] = {      DD| AA   , 0x00, "jmcne" },	/* Mask/Compare and Jump on Non-Equal */
[46] = { BBB| DD| AA   , 0x00, "jnbt" },	/* Test Bit and Jump if Not True */
[47] = { BBB| DD| AA   , 0x00, "jbt" },		/* Test Bit and Jump if True */
[48] = {      WB| AA| W, 0x00, "add" },		/* ADD Immediate to Memory */
[49] = {      WB| AA| W, 0x00, "or" },		/* OR Memory with Immediate */
[50] = {      WB| AA| W, 0x00, "and" },		/* AND Memory with Immediate */
[51] = {          AA| W, 0x00, "mov" },		/* Move from Source to Destination - Destination */
[52] = { RRR|     AA| W, 0x00, "add" },		/* Add Register to Memory */
[53] = { RRR|     AA| W, 0x00, "or" },		/* OR Memory with Register */
[54] = { RRR|     AA| W, 0x00, "and" },		/* AND Memory with Register */
[55] = {          AA| W, 0x00, "not" },		/* Complement Memory */
[56] = {      DD| AA| W, 0x00, "jnz" },		/* Jump on Non-Zero Memory */
[57] = {      DD| AA| W, 0x00, "jz" },		/* Jump on Zero Memory */
[58] = {          AA| W, 0x00, "inc" },		/* Increment Addressed Location */
[59] = {          AA| W, 0x00, "dec" },		/* Decrement Addressed Location */
[61] = { BBB|     AA   , 0x00, "setb" },	/* Set the Selected Bit */
[62] = { BBB|     AA   , 0x00, "clr" },		/* Clear the Selected Bit */
};

#endif /* I89_HAVE_CHECK || I89_HAVE_PRINT */

#if defined(I89_HAVE_CHECK)

static int
validate (uint16_t insn, uint32_t value)
{
	struct op *op = &ops[opcode];
	uint8_t extra = insn & 0xff;

	if (op->flags == 0 && op->extra == 0) {
		fprintf (stderr, "Bad opcode %x %d\n", insn, opcode);
		return -1;
	}

	if ((op->flags & 0x06) != AA && (insn & 0x0300)) {
		fprintf (stderr, "Bad MM\n");
		return -1;
	}

	if ((op->flags & 0xe0) == PPP && pppregs[ppp] == BAD_REG) {
		fprintf (stderr, "Bad PPP %d\n", ppp);
		return -1;
	}

	if ((op->flags & 0x18) == WB && (2 < wb || wb < 1)) {
		fprintf (stderr, "Bad WB\n");
		return -1;
	}

	if ((op->flags & 0x18) == DD && (2 < dd || dd < 1)) {
		fprintf (stderr, "Bad DD\n");
		return -1;
	}

	extra &= ~(0xe0 * !!(op->flags & 0xe0));
	extra &= ~(0x18 * !!(op->flags & 0x18));
	extra &= ~(0x06 * !!(op->flags & 0x06));
	extra &= ~(0x01 * !!(op->flags & 0x01));
	if (extra != op->extra) {
		fprintf (stderr, "Wrong hi bits\n");
		return -1;
	}

	return 0;
}

#else /* !I89_HAVE_CHECK */

static inline int
validate (uint16_t insn, uint32_t value)
{
	return 0;
}

#endif /* !I89_HAVE_CHECK */

#if defined(I89_HAVE_PRINT)

#define PRINT_ADDR if (flags & I89_PRINT_ADDR) printf
#define PRINT_DATA if (flags & I89_PRINT_DATA) column += printf
#define PRINT_INSN if (flags & I89_PRINT_INSN) printf

/*
 * Pretty printer. Does some tricky things so that the listing ends up in
 * the format similar to Intel's ASM89 and i89's asi89.
 */

static int
print (uint16_t insn, int8_t offset, uint32_t value, uint8_t sdisp)
{
	struct op *op = &ops[opcode];
	int s = 0;

	/* Opcode 0: Special instruction */
	if ((insn & 0xff00) == 0x0000) {
		if (insn == 0x0000) {
			printf ("nop");
		} else if (insn == 0x0040) {
			printf ("sintr");
		} else if (insn == 0x0060) {
			printf ("xfer");
		} else if (insn & 0x0080) {
			printf ("wid %d,%d",
				insn & 0x0040 ? 16 : 8,
				insn & 0x0020 ? 16 : 8);
		} else {
			fprintf (stderr, "Bad special: 0x%04x\n", insn);
			return -1;
		}
		return 0;
	}

	/* Regular instruction mnemonic */
	if (op->mnem) {
		if ((op->flags & 0x18) == DD && dd == 2)
			putchar ('l');
		printf ("%s", op->mnem);
		if ((op->flags & 0x1) == W && w == 0)
			putchar ('b');
		if ((op->flags & 0x18) == WB)
			putchar ('i');
	}

	/* Arguments */
	#define S (s++ ? ',' : ' ')
	if (opcode == 36)
		s++;
	if ((op->flags & 0xe0) == PPP)
		printf ("%c%s", S, regn[pppregs[ppp]]);
	if ((opcode & 0x3c) == 0x28 && (op->flags & 0xe0) == RRR)
		printf ("%c%s", S, regn[rrr]);
	if ((op->flags & 0x06) == AA) {
		printf ("%c[%s", S, regn[mmregs[mm]]);
		switch (aa) {
		case 0:
			printf ("]");
			break;
		case 1:
			printf ("].%d", offset);
			break;
		case 2:
			printf ("+ix]");
			break;
		case 3:
			printf ("+ix+]");
			break;
		}
	}
	if ((opcode & 0x3c) != 0x28 && (op->flags & 0xe0) == RRR)
		printf ("%c%s", S, regn[rrr]);
	if ((op->flags & 0xe0) == BBB)
		printf ("%c%d", S, bbb);
	if ((op->flags & 0x18) == DD)
		printf ("%c[TP].%d", S, (int16_t)value);
	if ((op->flags & 0x18) == WB) {
		switch (wb) {
		case 1:
			printf ("%c0x%02x", S, value & 0xff);
			break;
		case 2:
			printf ("%c0x%04x", S, value & 0xffff);
			break;
		}
	}
	if (opcode == 2)
		printf ("%c0x%08x", S, value);
	if (opcode == 37)
		printf ("%c0x%02x,[TP].%d", S, value & 0xff, sdisp);
	#undef S

	return 0;
}

#else /* !I89_HAVE_PRINT */

#define PRINT_ADDR if (0) printf
#define PRINT_DATA if (0) printf
#define PRINT_INSN if (0) printf

static inline int
print (uint16_t insn, int8_t offset, uint32_t value, uint8_t sdisp)
{
	return 0;
}

#endif /* !I89_HAVE_PRINT */

/*
 * Memory access.
 */

static uint32_t
in8 (struct i89 *iop, uint32_t addr, int tag)
{
	if (tag)
		return iop->in8 (iop, addr);
	else
		return iop->read8 (iop, addr);
}

static uint32_t
in16 (struct i89 *iop, uint32_t addr, int tag)
{
	if (tag) {
		if (iop->in16)
			return iop->in16 (iop, addr);
		return iop->in8 (iop, addr) | (iop->in8 (iop, addr + 1) << 8);
	} else {
		if (iop->read16)
			return iop->read16 (iop, addr);
		return iop->read8 (iop, addr) | (iop->read8 (iop, addr + 1) << 8);
	}
}

static uint32_t
in (struct i89 *iop, uint32_t addr, int tag, unsigned wide)
{
	addr &= tag ? 0xffff : 0xfffff;
	if (wide == 0)
		return in8 (iop, addr, tag);
	if (addr % 2)
		return in8 (iop, addr, tag) | (in (iop, addr + 1, tag, wide - 1) << 8);
	if (wide == 1)
		return in16 (iop, addr, tag);
	return in16 (iop, addr, tag) | (in (iop, addr + 2, tag, wide - 2) << 16);
}

static void
out8 (struct i89 *iop, uint32_t addr, uint8_t value, int tag)
{
	if (tag)
		iop->out8 (iop, addr, value);
	else
		iop->write8 (iop, addr, value);
}

static void
out16 (struct i89 *iop, uint32_t addr, uint16_t value, int tag)
{
	if (tag) {
		if (iop->out16) {
			iop->out16 (iop, addr, value);
		} else {
			iop->out8 (iop, addr, value);
			iop->out8 (iop, addr + 1, value >> 8);
		}
	} else {
		if (iop->write16) {
			iop->write16 (iop, addr, value);
			return;
		} else {
			iop->write8 (iop, addr, value);
			iop->write8 (iop, addr + 1, value >> 8);
		}
	}
}

static void
out (struct i89 *iop, uint32_t addr, uint32_t value, int tag, unsigned wide)
{
	addr &= tag ? 0xffff : 0xfffff;
	if (wide == 0) {
		out8 (iop, addr, value, tag);
		return;
	}
	if ((addr % 2) || !out16) {
		out8 (iop, addr, value, tag);
		out (iop, addr + 1, value >> 8, tag, wide - 1);
		return;
	}
	if (wide == 1) {
		out16 (iop, addr, value, tag);
		return;
	}
	out16 (iop, addr, value, tag);
	out (iop, addr + 2, value >> 16, tag, wide - 2);
}

/*
 * Helper macros.
 */

#define CHAN	(iop->chan[ch])
#define TAG(r)	((CHAN.tags >> (r)) & 1)
#define MASK(b)	((CHAN.regs[MC] ^ (b)) & (CHAN.regs[MC] >> 8) & 0xff)
#define preg	CHAN.regs[mmregs[mm]]

/*
 * Memory access macros.
 */

#define rd	in(iop, preg+offset, TAG(mmregs[mm]), w)
#define rd20	in(iop, preg+offset, TAG(mmregs[mm]), 2)
#define rd32	in(iop, preg+offset, TAG(mmregs[mm]), 3)
#define wr(v)	out(iop, preg+offset, v, TAG(mmregs[mm]), w)
#define wr20(v)	out(iop, preg+offset, v, TAG(mmregs[mm]), 2)

/*
 * DMA.
 */

static int
dma (struct i89 *iop, int ch)
{
	uint16_t cc = CHAN.regs[CC];
	int gs_inc = !!(cc & 0x4000);
	int gd_inc = !!(cc & 0x8000);
	int wid = CHAN.wid;
	uint8_t masked = 0;
	int src, dst;
	uint16_t val;
	int term;

	/* S */
	if (cc & 0x0400) {
		src = GB;
		dst = GA;
	} else {
		src = GA;
		dst = GB;
	}

	//printf ("\n");
	//printf ("========== XFER =======\n");
	//printf ("%s->", gs_inc ? "(GS)+" : "GS");
	//printf ("%s ", gd_inc ? "(GD)+" : "GD");
	//printf ("src=%s dst=%s ", regn[src], regn[dst]);
	//printf ("\n");
	//printf ("tbc=TP+%d\n", tp_add);

	/* TR: translate not supported */
	if (cc & 0x2000) {
		fprintf (stderr, "tx unimpl\n");
		return -1;
	}

	while (1) {
		/* TX External Terminate */
		if (cc & 0x0060) {
			if (!CHAN.xfer) {
				term = cc >> 5;
				break;
			}
		}

		/* TBC Byte Counte Termination*/
		if (cc & 0x0018) {
			if (CHAN.regs[BC] == 0) {
				term = cc >> 7;
				break;
			}
			if (CHAN.regs[BC] == 1)
				wid = 0;
		}

		switch (wid) {
		case 0:
			/* wid 8,8 */
			val = in8 (iop, CHAN.regs[src], TAG(src));
			masked = !MASK(val);
			CHAN.regs[src] += gs_inc;
			out8 (iop, CHAN.regs[dst], val, TAG(dst));
			CHAN.regs[dst] += gd_inc;
			CHAN.regs[BC]--;
			break;
		case 1:
			/* wid 8,16 */
			val = in8 (iop, CHAN.regs[src], TAG(src));
			masked = !MASK(val);
			CHAN.regs[src] += gs_inc;
			val |= in8 (iop, CHAN.regs[src], TAG(src)) << 8;
			masked |= !MASK(val >> 8);
			CHAN.regs[src] += gs_inc;
			out16 (iop, CHAN.regs[dst], val, TAG(dst));
			CHAN.regs[dst] += 2 * gd_inc;
			CHAN.regs[BC] -= 2;
			break;
		case 2:
			/* wid 16,8 */
			val = in16 (iop, CHAN.regs[src], TAG(src));
			masked = !MASK(val);
			masked |= !MASK(val >> 8);
			CHAN.regs[src] += 2 * gs_inc;
			out8 (iop, CHAN.regs[dst], val, TAG(dst));
			CHAN.regs[dst] += gd_inc;
			out8 (iop, CHAN.regs[dst], val >> 8, TAG(dst));
			CHAN.regs[dst] += gd_inc;
			CHAN.regs[BC] -= 2;
			break;
		case 3:
			/* wid 16,16 */
			val = in16 (iop, CHAN.regs[src], TAG(src));
			masked = !MASK(val);
			masked |= !MASK(val >> 8);
			CHAN.regs[src] += 2 * gs_inc;
			out16 (iop, CHAN.regs[dst], val, TAG(dst));
			CHAN.regs[dst] += 2 * gd_inc;
			CHAN.regs[BC] -= 2;
			break;
		}

#if 0
		if ((cc & 0x0007) != 0x0006) {
			/* TSH: Mask/compare termination */
			if (masked != !(cc & 0x0600)) {
				term = cc & 0x0003;
				break;
			}
		}
#endif

		if (cc & 0x0080) {
			/* TS: Single Transfer mode. */
			term = 0;
			break;
		}
	}

	CHAN.xfer = 0;

	switch (term & 3) {
	case 3: CHAN.regs[TP] += 4;
	case 2: CHAN.regs[TP] += 4;
	}

	return 0;
}

/*
 * Various common instruction operations.
 */

#define FETCH	(in(iop, CHAN.regs[TP]++, 0, 0))
#define FETCH16	(in(iop, (CHAN.regs[TP]+=2)-2, 0, 1))
#define JUMP	(CHAN.regs[TP] += (int16_t)value)
#define REG	(CHAN.regs[rrr])
#define BIT	(1 << bbb)
#define TAG_IO	(CHAN.tags |= (1 << rrr))
#define TAG_MEM	(CHAN.tags &= ~(1 << rrr))
#define REG20	((REG & 0xffff) | (REG & 0xf0000) << 4 | (TAG(rrr) << 19))

static inline uint32_t
segoff (uint32_t value)
{
	return (value & 0xffff) | ((value >> 16) << 4);
}

/*
 * This fetches an instruction and its argument.
 * It optionally validates, prints and executes it.
 *
 * Processing MOV M,M is somewhat peculiar, because it's in fact two
 * instructions: one for load, another for store. Handling it involves
 * recursion, passing along the value being transferred and context for
 * pretty printing.
 */


static int
do_insn (struct i89 *iop, int ch, enum i89_flags flags, uint32_t value, int column)
{
	int8_t offset, sdisp;
	uint16_t insn;

	PRINT_ADDR ("%05x: ", CHAN.regs[TP]);

	/* Fetch the instruction. */
	insn = FETCH16;
	PRINT_DATA ("%04x ", insn);

	/* Displacement/offset */
	switch (aa) {
	case 0:
		offset = 0;
		break;
	case 1:
		offset = FETCH;
		PRINT_DATA ("%02x ", offset);
		break;
	case 2:
		offset = CHAN.regs[IX];
		break;
	case 3:
		offset = CHAN.regs[IX]++;
		break;
	}

	/* Immediate value */
	switch (wb) {
	case 0:
		/* None. Second part of mov m,m takes this from previous
		 * half instruction, passed as an argument. */
		break;
	case 1:
		/* Immediate byte. */
		value = FETCH;
		PRINT_DATA ("%02x ", value);

		/* Sign extend */
		value = (int8_t)value;
		break;
	case 2:
		/* Immediate word (or more). */
		value = FETCH16;
		if ((insn & 0xff00) == 0x0800) {
			/* lpdi takes two more immediate bytes */
			value |= FETCH16 << 16;
			PRINT_DATA ("%08x ", value);
		} else {
			PRINT_DATA ("%04x ", value);
			/* Sign extend */
			value = (int16_t)value;
		}
		break;
	case 3:
		/* Used by tsl instruction only. */
		value = FETCH;
		PRINT_DATA ("%02x ", value);
		sdisp = FETCH;
		PRINT_DATA ("%02x ", sdisp);
		sdisp = (int16_t)sdisp;
		break;
	}

	/* Sanity checks */
        if (flags & I89_CHECK) {
		if (validate (insn, value))
			return -1;
		if ((flags & _I89_STORE) && (opcode != 51)) {
			fprintf (stderr, "mov/store must follow mov/load\n");
			return -1;
		}
	}

	/* mov m,m (load part) */
	if (opcode == 36) {
		if (do_insn(iop, ch, _I89_STORE | flags & ~I89_PRINT_ADDR, rd, column))
			return -1;
		column = 0;
	}

        if (flags & I89_PRINT_INSN) {
		if (column && (flags & I89_PRINT_DATA)) {
			for (; column < 20; column++)
				putchar (' ');
		}
		if (print (insn, offset, value, sdisp))
			return -1;
		/* mov m,m (store part) */
		if (opcode != 51)
			putchar ('\n');
	}

	if ((flags & I89_EXEC) == 0)
		return 0;

	switch (opcode) {

	case  2: REG = segoff (value); TAG_MEM;	break;	/* lpdi p,i */
	case  8: REG += value;			break;	/* addi r,i */
	case  9: REG |= value;			break;	/* ori r,i */
	case 10: REG &= value;			break;	/* andi r,i */
	case 11: REG = ~REG;			break;	/* not r */
	case 12: REG = value; TAG_IO;		break;	/* movi r,i */
	case 14: REG++;				break;	/* inc r */
	case 15: REG--;				break;	/* dec r */
	case 16: if (REG) JUMP;			break;	/* jnz r */
	case 17: if (REG == 0) JUMP;		break;	/* jz r */
	case 18: return 1;				/* hlt */
	case 19: wr(value);			break;	/* mov m,i */
	case 32: REG = rd; TAG_IO;		break;	/* mov r,m */
	case 33: wr(REG);			break;	/* mov m,r */
	case 34: REG = segoff (rd32); TAG_MEM;	break;	/* lpd p,m */
	case 35:					/* movp p,m */
		REG = rd20;
		if (REG & (1 << 19))
			TAG_IO;
		else
			TAG_MEM;
		REG = (REG & 0xffff) | ((REG & 0xf00000) >> 4);
		break;
	case 36: /* handled above */		break;	/* movp p,m */
	case 38: wr20(REG20);			break;	/* movp m,p */
	case 39: wr20(REG20); JUMP;		break;	/* call */
	case 40: REG += rd;			break;	/* add r,m */
	case 41: REG |= rd;			break;	/* or r,m */
	case 42: REG &= rd;			break;	/* and r,m */
	case 43: REG = ~rd;			break;	/* not r,m */
	case 44: if (MASK(rd) == 0) JUMP;	break;	/* jmce */
	case 45: if (MASK(rd) != 0) JUMP;	break;	/* jmcne */
	case 46: if (!(rd & BIT)) JUMP;		break;	/* jnbt */
	case 47: if (rd & BIT) JUMP;		break;	/* jbt */
	case 48: wr(rd + value);		break;	/* add m,i */
	case 49: wr(rd | value);		break;	/* or m,i */
	case 50: wr(rd & value);		break;	/* and m,i */
	case 51: wr(value);			break;	/* mov m,m (store part) */
	case 52: wr(rd + REG);			break;	/* add m,r */
	case 53: wr(rd | REG);			break;	/* or m,r */
	case 54: wr(rd & REG);			break;	/* and m,r */
	case 55: wr(~rd);			break;	/* not m */
	case 56: if (rd) JUMP;			break;	/* jnz m */
	case 57: if (rd == 0) JUMP;		break;	/* jz m */
	case 58: wr(rd + 1);			break;	/* inc m */
	case 59: wr(rd - 1);			break;	/* dec m */
	case 61: wr(rd | 1 << bbb);		break;	/* setb */
	case 62: wr(rd & ~ BIT);		break;	/* clr */

	case  0:
		/* Special instruction */
		if (insn == 0x0000) {
			/* nop */
			break;
		} else if (insn == 0x0040) {
			if (iop->sintr)
				iop->sintr (iop);
			break;
		} else if (insn == 0x0060) {
			CHAN.xfer = 1;
			/* Transfer begins at the end of next insn. */
			return 0;
		} else if (insn & 0x0080) {
			CHAN.wid = (insn & 0x0060) >> 5;
			break;
		}
		/* Fallthrough. */
	default:
		fprintf (stderr, "Unknown: %d\n", opcode);
		return -1;
	}

	if (CHAN.xfer)
		return dma (iop, ch);

	return 0;
}

int
i89_insn (struct i89 *iop, enum i89_flags flags)
{
	do_insn (iop, 0, flags, 0, 0);
}

static void
dump_chan (struct i89 *iop, int ch)
{
	int i;

	printf ("ch%d: ", ch);
	for (i = 0; i < NUM_REGS; i++) {
		printf ("%s=", regn[i]);
		switch (i) {
		case GA:
		case GB:
		case GC:
		case BC:
		case TP:
		case PP:
			if (CHAN.tags & (1 << i))
				printf ("IO:0x%04x", CHAN.regs[i] & 0xffff);
			else
				printf ("MEM:0x%05x", CHAN.regs[i] & 0xfffff);
			break;
		default:
			printf ("0x%04x", CHAN.regs[i] & 0xffff);

		}
		printf (i == 3 || i == NUM_REGS-1 ? "\n     " : " ");
	}
	printf ("src=%dbit ", (CHAN.wid & 2) ? 16 : 8);
	printf ("dst=%dbit\n", (CHAN.wid & 1) ? 16 : 8);
}

void
i89_dump (struct i89 *iop)
{
	int ch = 0;
	int i;

	dump_chan (iop, 0);
	dump_chan (iop, 1);
}

static uint32_t
memptr (struct i89 *iop, uint32_t addr)
{
	return segoff(in (iop, addr, 0, 3));
}

void
i89_attn (struct i89 *iop, int ch)
{
	uint32_t scb;
	uint32_t pb0, pb1;

	if (iop->cb == 0) {
		in8 (iop, 0xffff6, 0); // sys bus
		scb = memptr (iop, 0xffff8);

		in8 (iop, scb, 0); // soc
		iop->cb = memptr (iop, scb + 2);
	}

	in8 (iop, iop->cb + 8 * ch + 0, 0); // ccw0
	in8 (iop, iop->cb + 8 * ch + 1, 0); // busy0
	CHAN.regs[PP] = memptr (iop, iop->cb + 0 + 2);
	CHAN.regs[TP] = memptr (iop, CHAN.regs[PP]);
}
