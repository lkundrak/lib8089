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

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include "8089.h"

uint8_t mem[0x100000];
int end;

static uint8_t
read8 (struct i89 *iop, uint32_t addr)
{
	uint8_t value;

	if (addr < end) {
		value = mem[addr];
	} else {
		fprintf (stderr, "Short read\n");
		value = 0xff;
	}
	return mem[addr];
}

int
main (int argc, char *argv[])
{
	struct i89 iop = { 0, };
	enum i89_flags flags;
	int br;
	int fd;

	iop.read8 = read8;
	end = iop.chan[0].regs[TP];

	switch (argc) {
	case 1:
		fd = STDIN_FILENO;
		break;
	case 2:
		fd = open (argv[1], O_RDONLY);
		if (fd == -1) {
			perror (argv[1]);
			return 1;
		}
		break;
	case 10:
		fprintf (stderr, "Are you stupid?\n");
	default:
		fprintf (stderr, "Usage: %s [<iop.bin>]\n", argv[0]);
		return 1;
	}

	do {
		br = read (fd, &mem[end], sizeof(mem) - end);
		if (br == -1) {
			perror (argv[1]);
			return 1;
		}
		end += br;
	} while (br);

	flags = I89_CHECK;
	flags |= I89_PRINT_INSN;
	flags |= I89_PRINT_ADDR | I89_PRINT_DATA;
	while (iop.chan[0].regs[TP] < end) {
		if (i89_insn (&iop, flags))
			return 1;
	}

	return 0;
}
