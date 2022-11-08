#include <stdint.h>
#include <stdio.h>

#include "8089.h"

#define PP_OFF 0x0ee9
#define PP_SEG 0x0000
#define PP_ADDR SEGOFF(PP_SEG, PP_OFF)

#define TP_OFF 0x1eba
#define TP_SEG 0xfe00
#define TP_ADDR SEGOFF(TP_SEG, TP_OFF)

#define DATA_OFF 0x06ca
#define DATA_SEG 0x0000
//#define DATA_LEN 512
#define DATA_LEN 1024
#define DATA_ADDR SEGOFF(DATA_SEG, DATA_OFF)

#define LE16(n) ((n) & 0xff), ((n) >> 8)
#define SEGOFF(s,o) (((s) << 4) + (o))

// 16K
uint8_t sbuf[0x4000] = { 0, };

uint8_t mem[0x100000] = {

[0x400] = 0x03, // ccw
[0x401] = 0x00, // busy
[0x402] =
LE16(PP_OFF),
LE16(PP_SEG),

[0x410] = 0x01, // soc
[0x412] = LE16(0x0000),
[0x414] = LE16(0x0040),

[PP_ADDR] =
LE16(TP_OFF),	// 00: Offset
LE16(TP_SEG),	// 02: Segment
0x21,		// 04: Opcode
0xff,		// 05: Status
LE16(0x1234),	// 06: Cylinder
0x56,		// 08: Drive and head
0x78,		// 09: Start sector (incremented as i/o happens)
LE16(DATA_LEN),	// 0a: Byte count
LE16(DATA_OFF),	// 0c: Buffer Offset
LE16(DATA_SEG),	// 0e: Buffer Segment
2,		// 10: Sector count (decremented as i/o happens)
		// 12: tmp: word transfer size
		// 14: tmp: word cylinder save
		// 16: tmp: byte[3] tp save

[TP_ADDR] =
0x51, 0x30, 0xd0, 0xff,			// 0000:		movi	gc,0ffd0h
0xaa, 0xbb, 0x04, 0x20,			// 0004:		jnbt	[pp].4h_opcode,5,cmd_no_bit5

0x0a, 0x4e, 0x06, 0x80,			// 0008:		movbi	[gc].6h,80h // ?
0x02, 0x93, 0x08, 0x02, 0xce, 0x02,	// 000c:		movb	[gc].2h,[pp].8h_drive_head
0xea, 0xba, 0x06, 0xfc,			// 0012: loop_0:	jnbt	[gc].6h,7,loop_0

0x0a, 0x4e, 0x06, 0x20,			// 0016:		movbi	[gc].6h,20h
0x13, 0x4f, 0x14, 0x00, 0x00,		// 001a:		movi	[pp].14h_cylinder_save,0h
0x0a, 0xbe, 0x06, 0xfc,			// 001f: loop_1:	jbt	[gc].6h,0,loop_1
0x12, 0xba, 0x04, 0xe2, 0x00,		// 0023:		ljnbt	[gc].4h,0,ret_81

0x0a, 0xcb, 0x04, 0x0f,			// 0028: cmd_no_bit5:	andbi	[pp].4h_opcode,0fh
0x12, 0xe7, 0x04, 0xb1, 0x00,		// 002c:		ljzb	[pp].4h_opcode,ret_00

0x02, 0x93, 0x08, 0x02, 0xce, 0x02,	// 0031:		movb	[gc].2h,[pp].8h_drive_head
0xea, 0xba, 0x06, 0xfc,			// 0037: loop_2:	jnbt	[gc].6h,7,loop_2

0x02, 0x93, 0x14, 0x00, 0xce,		// 003b:		movb	[gc],[pp].14h_cylinder_save
0x02, 0x93, 0x15, 0x00, 0xce,		// 0040:		movb	[gc],[pp].15h
0x02, 0x93, 0x06, 0x02, 0xce, 0x04,	// 0045:		movb	[gc].4h,[pp].6h_cylinder_lo
0x02, 0x93, 0x07, 0x02, 0xce, 0x04,	// 004b:		movb	[gc].4h,[pp].7h_cylinder_lo
0x0a, 0x4e, 0x06, 0x10,			// 0051:		movbi	[gc].6h,10h
0x03, 0x93, 0x06, 0x03, 0xcf, 0x14,	// 0055:		mov	[pp].14h_cylinder_save,[pp].6h_cylinder
0x0a, 0xbe, 0x06, 0xfc,			// 005b: loop_3:	jbt	[gc].6h,0,loop_3
0x2a, 0xba, 0x04, 0xfc,			// 005f: loop_4:	jnbt	[gc].4h,1,loop_4
0x0a, 0xe7, 0x10, 0x7b,			// 0063:		jzb	[pp].10h_sector_count,ret_00
0x0a, 0xbf, 0x04, 0x0e,			// 0067:		jbt	[pp].4h_opcode,0,read_op
0x03, 0x8b, 0x0c,			// 006b:		lpd	ga,[pp].0ch_data_buffer
0x31, 0x30, 0x00, 0x00,			// 006e:		movi	gb,0h
0x63, 0x83, 0x0a,			// 0072:		mov	bc,[pp].0ah_byte_count
0x8b, 0x9f, 0x16, 0x70,			// 0075:		call	[pp].16h_call_ret_tp,mmxfer

0x31, 0x30, 0x00, 0x00,			// 0079: read_op:	movi	gb,0h
0xf1, 0x30, 0x80, 0xfe,			// 007d:		movi	mc,0fe80h
0x11, 0x30, 0xd0, 0xff,			// 0081:		movi	ga,0ffd0h
0x13, 0x4f, 0x12, 0x00, 0x02,		// 0085:		movi	[pp].12h_transfer_len,512
0x0a, 0xbb, 0x04, 0x12,			// 008a:		jnbt	[pp].4h_opcode,0,write_op
0xd1, 0x30, 0x28, 0x8a,			// 008e:		movi	cc,8a28h // ga->gb++  -> ffd0.8->0++
0xa0, 0x00,				// 0092:		wid	8,16
0x6a, 0xbb, 0x04, 0x17,			// 0094:		jnbt	[pp].4h_opcode,3,do_one_xfer
0x13, 0x4f, 0x12, 0x05, 0x02,		// 0098:		movi	[pp].12h_transfer_len,517
0x88, 0x20, 0x0f,			// 009d:		jmp	do_one_xfer

0xd1, 0x30, 0x28, 0x56,			// 00a0: write_op:	movi	cc,5628h // gb++->ga  -> 0++->ffd0.8
0xc0, 0x00,				// 00a4:		wid	16,8
0x4a, 0xbb, 0x04, 0x05,			// 00a6:		jnbt	[pp].4h_opcode,2,do_one_xfer
0x13, 0x4f, 0x12, 0x04, 0x00,		// 00aa:		movi	[pp].12h_transfer_len,4

0x63, 0x83, 0x12,			// 00af: do_one_xfer:	mov	bc,[pp].12h_transfer_len
0x02, 0x93, 0x09, 0x00, 0xce,		// 00b2:		movb	[gc],[pp].9h_sector
0x60, 0x00,				// 00b7:		xfer
0x02, 0x93, 0x04, 0x02, 0xce, 0x06,	// 00b9:		movb	[gc].6h,[pp].4h_opcode
0x0a, 0xb6, 0x06, 0x33,			// 00bf:		jmcne	[gc].6h,ret_err
0x02, 0xef, 0x10,			// 00c3:		decb	[pp].10h_sector_count
0x0a, 0xe7, 0x10, 0x06,			// 00c6:		jzb	[pp].10h_sector_count,xfers_done
0x02, 0xeb, 0x09,			// 00ca:		incb	[pp].9h_sector
0x88, 0x20, 0xdf,			// 00cd:		jmp	do_one_xfer

0x0a, 0xbb, 0x04, 0x0e,			// 00d0: xfers_done:	jnbt	[pp].4h_opcode,0,ret_00
0x23, 0x8b, 0x0c,			// 00d4:		lpd	gb,[pp].0ch_data_buffer
0x11, 0x30, 0x00, 0x00,			// 00d7:		movi	ga,0h
0x63, 0x83, 0x0a,			// 00db:		mov	bc,[pp].0ah_byte_count
0x8b, 0x9f, 0x16, 0x07,			// 00de:		call	[pp].16h_call_ret_tp,mmxfer
0x0a, 0x4f, 0x05, 0x00,			// 00e2: ret_00:	movbi	[pp].5h_status,0h
0x88, 0x20, 0x26,			// 00e6:		jmp	ret

0xe0, 0x00,				// 00e9: mmxfer:	wid	16,16
0xd1, 0x30, 0x08, 0xc2,			// 00eb:		movi	cc,0c208h  // ga++->gb++  16->16
0x60, 0x00,				// 00ef:		xfer
0x00, 0x00,				// 00f1:		nop
0x83, 0x8f, 0x16,			// 00f3:		movp	tp,[pp].16h_call_ret_tp

0x02, 0x92, 0x06, 0x02, 0xcf, 0x05,	// 00f6: ret_err:	movb	[pp].5h_status,[gc].6h
0x0a, 0xcb, 0x05, 0x7e,			// 00fc:		andbi	[pp].5h_status,7eh
0xe2, 0xf7, 0x05,			// 0100:		setb	[pp].5h_status,7
0x0a, 0x4e, 0x06, 0x00,			// 0103:		movbi	[gc].6h,0h
0x88, 0x20, 0x05,			// 0107:		jmp	ret

0x13, 0x4f, 0x05, 0x81, 0x00,		// 010a: ret_81:	movi	[pp].5h_status,81h
0x40, 0x00,				// 010f: ret:		sintr
0x20, 0x48,				// 0111:		hlt

[0xffff6] = 0x01,
[0xffff8] =
LE16(0x0410),
LE16(0x0000),
};

int stop = 0;

static const char *
aname (uint32_t addr)
{
	switch (addr) {
	case 0x400: return "CCW";
	case 0x401: return "BUSY";
	case 0x402: return "PP Offset.lo";
	case 0x403: return "PP Offset.hi";
	case 0x404: return "PP Segment.lo";
	case 0x405: return "PP Segment.hi";

	case 0x410: return "SOC";
	case 0x412: return "CB Offset.lo";
	case 0x413: return "CB Offset.hi";
	case 0x414: return "CB Segment.lo";
	case 0x415: return "CB Segment.hi";

	case PP_ADDR + 0x00: return "Offset.lo";
	case PP_ADDR + 0x01: return "Offset.hi";
	case PP_ADDR + 0x02: return "Segment.lo";
	case PP_ADDR + 0x03: return "Segment.hi";
	case PP_ADDR + 0x04: return "Opcode";
	case PP_ADDR + 0x05: return "Status";
	case PP_ADDR + 0x06: return "Cylinder.lo";
	case PP_ADDR + 0x07: return "Cylinder.hi";
	case PP_ADDR + 0x08: return "Drive and head";
	case PP_ADDR + 0x09: return "Start sector";
	case PP_ADDR + 0x0a: return "Byte count.lo";
	case PP_ADDR + 0x0b: return "Byte count.hi";
	case PP_ADDR + 0x0c: return "Buffer Offset.lo";
	case PP_ADDR + 0x0d: return "Buffer Offset.hi";
	case PP_ADDR + 0x0e: return "Buffer Segment.lo";
	case PP_ADDR + 0x0f: return "Buffer Segment.hi";
	case PP_ADDR + 0x10: return "Sector count";
	case PP_ADDR + 0x12: return "Tx size.lo";
	case PP_ADDR + 0x13: return "Tx size.hi";
	case PP_ADDR + 0x14: return "Old Cylinder save.lo";
	case PP_ADDR + 0x15: return "Old Cylinder save.hi";
	case PP_ADDR + 0x16: return "Scratch (ret).0";
	case PP_ADDR + 0x17: return "Scratch (ret).1";
	case PP_ADDR + 0x18: return "Scratch (ret).2";
	default: return NULL;
	}
}

static uint8_t
read8 (struct i89 *iop, uint32_t addr)
{
	uint8_t value = mem[addr];
	const char *name;

	name = aname (addr);
	if (name == NULL) {
		if (addr >= TP_ADDR)
			return value;
		name = "Unknown";
		stop = 1;
	}

	//printf ("%s 0x%05x -> 0x%02x [%s]\n", __func__, addr, value, name);
	return value;
}

static void
write8 (struct i89 *iop, uint32_t addr, uint8_t value)
{
	const char *name;

	if (addr >= DATA_ADDR && addr < DATA_ADDR + DATA_LEN)
		name = "Data";
	else
		name = aname (addr);
	if (name == NULL) {
		name = "Unknown";
		stop = 1;
	}

	//printf ("%s 0x%05x <- 0x%02x [%s]\n", __func__, addr, value, name);
	mem[addr] = value;
}

uint8_t data_buf[2] = { 0, };
int data_ptr = 0;
uint16_t drive_head = 0;
uint8_t status = 0;
uint8_t seek_status = 0;
uint16_t cylinder;

static uint8_t
in8 (struct i89 *iop, uint16_t addr)
{
	uint8_t value = 0xff;

	switch (addr) {
	case 0xffd0:
		// data read
		value = 0x5a;
		if (!iop->chan[0].xfer)
			stop = 1;
		break;
	case 0xffd2:
		stop = 1;
		break;
	case 0xffd4:
		value = seek_status;
		break;
	case 0xffd6:
		value = status;
		break;
	default:
		if (addr < sizeof(sbuf))
			value = sbuf[addr];
		else
			stop = 1;
	};

	//printf ("%s 0x%04x -> 0x%02x\n", __func__, addr, value);
	return value;
}

static void
out8 (struct i89 *iop, uint16_t addr, uint8_t value)
{
	uint8_t reg;

	//printf ("%s 0x%04x <- 0x%02x\n", __func__, addr, value);

	switch (addr) {
	case 0xffd0:
		data_ptr %= sizeof(data_buf);
		data_buf[data_ptr++] = value;
		break;
	case 0xffd2:
		//printf ("HEAD AND DRIVE: %x\n", value);
		drive_head = value;
		status |= 0x80; // selected
		break;
	case 0xffd4:
		cylinder >>= 8;
		cylinder |= value << 8;
		break;
	case 0xffd6:
		switch (value) {
		case 0x01:
			//printf ("START AT SECTOR: %x\n", data_buf[0]);
			break;
		case 0x10:
			// seek to cylinder
			//printf ("SEEK TO CYLINDER: %x\n", cylinder);
			data_ptr = 0;
			status &= ~0x01; // not busy
			seek_status |= 0x02; // seek done
			break;
		case 0x20:
			// if command bit 5 is on
			//printf ("SOME SORT OF SELECT\n");
			seek_status |= 0x01; // head/drive selected?
			break;
		case 0x80:
			// if command bit 5 is on
			//printf ("SOME SORT OF RESET\n");
			status = 0;
			seek_status = 0;
			cylinder = 0;
			data_ptr = 0;
			break;
		default:
			stop = 1;
		}
		break;
	default:
		if (addr < sizeof(sbuf))
			sbuf[addr] = value;
		else
			stop = 1;
	};
}


int
main (int argc, char *argv[])
{
	struct i89 iop = { 0, };
	enum i89_flags flags;
	int br;

	iop.read8 = read8;
	iop.write8 = write8;
	iop.in8 = in8;
	iop.out8 = out8;

	if (argc > 1) {
		fprintf (stderr, "Are you stupid?\n");
		return 1;
	}

	flags = I89_CHECK;
	flags |= I89_PRINT_INSN;
	flags |= I89_PRINT_ADDR | I89_PRINT_DATA;
	flags |= I89_EXEC;

	i89_attn (&iop, 0);
	while (1) {
		i89_dump (&iop);	
		if (stop)
			return 0;
		putchar ('\n');
		if (i89_insn (&iop, flags))
			stop = 1;
		putchar ('\n');
	}

	return 0;
}
