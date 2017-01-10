#include <stdint.h>

#include "memory.h"

#define ZFLAG 0x80
#define NFLAG 0x40
#define HFLAG 0x20
#define CFLAG 0x10

static void init_optable(void);

struct reg {
	uint8_t high;
	uint8_t low;
} AF, BC, DE, HL;

uint16_t PC;
uint16_t SP;

void (*optable[512])(void);

uint8_t fetch_8bit_data(void)
{
	uint8_t data;

	data = read_memory(PC);
	PC++;

	return data;
}

uint16_t fetch_16bit_data(void)
{
	uint16_t data;

	data = read_memory(PC) + (read_memory(PC + 1) << 8);
	PC = PC + 2;

	return data;
}

void fetch_opcode(void)
{
	uint8_t opcode;

	opcode = read_memory(PC);
	PC++;
}

static void set_flag(uint8_t flag)
{
	AF.low |= flag;
}

static void reset_flag(uint8_t flag)
{
	AF.low &= (0xFF - flag);
}

static uint8_t get_flag(uint8_t flag)
{
	return (AF.low & flag) ? 1 : 0;
}

void push_stack(uint8_t low, uint8_t high)
{
	write_memory(SP, high);
	write_memory(SP - 1, low);

	SP -= 2;
}

uint16_t pop_stack(void)
{
	uint16_t value;
	value = read_memory(SP);
	value += (read_memory(SP + 1) << 8);

	SP += 2;

	return value;
}

static void handle_mbc(uint16_t addr, uint8_t value)
{
	switch (mode) {
	case ROM:
		break;
	case MBC1:
		write_mbc1(addr, value);
		break;
	default:
		return;
	}
}

void init_cpu(void)
{
	AF.high = 0x01;
	AF.low = 0xB0;
	BC.high = 0x00;
	BC.low = 0x13;
	DE.high = 0x00;
	DE.low = 0xD8;
	HL.high = 0x01;
	HL.low = 0x4D;
	SP = 0xFFFE;

	PC = 0x100;

	init_optable();
}

/* NOP */
static void op0x00(void)
{
	return;
}

/* LD BC,nn */
static void op0x01(void)
{
	uint16_t temp = fetch_16bit_data();
	BC.high = temp >> 8;
	BC.low = temp;
}

/* LD BC,A */
static void op0x02(void)
{
	BC.high = 0;
	BC.low = AF.high;
}

/* INC BC */
static void op0x03(void)
{
	BC.low++;

	if (BC.low == 0)
		BC.high++;

}

/* INC B */
static void op0x04(void)
{
	if ((BC.high & 0x0F) == 15)
		set_flag(HFLAG);

	BC.high++;
	if (BC.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC B */
static void op0x05(void)
{
	if ((BC.high & 0x0F) != 0)
		set_flag(HFLAG);

	BC.high--;
	if (BC.high == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD B,n */
static void op0x06(void)
{
	BC.high = fetch_8bit_data();
}

/* RLCA */
static void op0x07(void)
{
	if (AF.high > 127){
		set_flag(CFLAG);
	}

	AF.high <<= 1;
	AF.high += get_flag(CFLAG);

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
}

/* LD nn,SP */
static void op0x08(void)
{
	uint16_t addr = fetch_16bit_data();
	write_memory(addr, SP & 0x00FF);
	write_memory(addr+1, SP >> 8);
}

/* ADD HL,BC */
static void op0x09(void)
{
	int tmp = HL.high + BC.high;
	tmp <<= 8;
	tmp += HL.low + BC.low;
	if (tmp > 65535)
		set_flag(CFLAG);

	HL.high = tmp >> 8;
	HL.low = tmp;
	tmp = HL.high + BC.high;
	tmp &= 0x0F;
	tmp <<= 8;
	tmp += HL.low + BC.low;
	if (tmp > 4095)
		set_flag(HFLAG);

	reset_flag(NFLAG);
}

/* LD A,BC */
static void op0x0A(void)
{
	AF.high = BC.low;
}

/* DEC BC */
static void op0x0B(void)
{
	if (BC.low == 0)
		BC.high--;

	BC.low--;

}

/* INC C */
static void op0x0C(void)
{
	if ((BC.low & 0x0F) == 15)
		set_flag(HFLAG);

	BC.low++;
	if (BC.low == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC C */
static void op0x0D(void)
{
	if ((BC.low & 0x0F) != 0)
		set_flag(HFLAG);

	BC.low--;
	if (BC.low == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD C,n */
static void op0x0E(void)
{
	BC.low = fetch_8bit_data();
}

/* RRCA */
static void op0x0F(void)
{
	if (AF.high & 1)
		set_flag(CFLAG);

	AF.high >>= 1;
	AF.high += get_flag(CFLAG) * 128;

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(HFLAG);
	reset_flag(NFLAG);
}

static void op0x10(void)
{

}

/* LD DE,nn */
static void op0x11(void)
{
	uint16_t temp = fetch_16bit_data();
	DE.high = temp >> 8;
	DE.low = temp;
}

/* LD DE,A */
static void op0x12(void)
{
	DE.high = 0;
	DE.low = AF.high;
}

/* INC DE */
static void op0x13(void)
{
	DE.low++;
	if (DE.low == 0)
		DE.high++;

}

/* INC D */
static void op0x14(void)
{
	if ((DE.high & 0x0F) == 15)
		set_flag(HFLAG);

	DE.high++;
	if (DE.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC D */
static void op0x15(void)
{
	if ((DE.high & 0x0F) != 0)
		set_flag(HFLAG);

	DE.high--;
	if (DE.high == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD D,n */
static void op0x16(void)
{
	DE.high = fetch_8bit_data();
}

/* RLA */
static void op0x17(void)
{
	uint8_t tmp = get_flag(CFLAG);
	if (AF.high > 127){
		set_flag(CFLAG);
	}

	AF.high <<= 1;
	AF.high += tmp;

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
}

/* JR n */
static void op0x18(void)
{
	PC += fetch_8bit_data();
}

/* ADD HL,DE */
static void op0x19(void)
{
	int tmp = HL.high + DE.high;
	tmp <<= 8;
	tmp += HL.low + DE.low;
	if (tmp > 65535)
		set_flag(CFLAG);

	HL.high = tmp >> 8;
	HL.low = tmp;
	tmp = HL.high + DE.high;
	tmp &= 0x0F;
	tmp <<= 8;
	tmp += HL.low + DE.low;
	if (tmp > 4095)
		set_flag(HFLAG);

	reset_flag(NFLAG);
}

/* LD A,DE */
static void op0x1A(void)
{
	AF.high = DE.low;
}

/* DEC DE */
static void op0x1B(void)
{
	if (DE.low == 0)
		DE.high--;

	DE.low--;
}

/* INC E */
static void op0x1C(void)
{
	if ((DE.low & 0x0F) == 15)
		set_flag(HFLAG);

	DE.low++;
	if (DE.low == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC E */
static void op0x1D(void)
{
	if ((DE.low & 0x0F) != 0)
		set_flag(HFLAG);

	DE.low--;
	if (DE.low == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD E,n */
static void op0x1E(void)
{
	DE.low = fetch_8bit_data();
}

/* RRA */
static void op0x1F(void)
{
	uint8_t tmp = get_flag(CFLAG);
	if (AF.high & 1)
		set_flag(CFLAG);

	AF.high >>= 1;
	AF.high += tmp * 128;

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(HFLAG);
	reset_flag(NFLAG);
}

/* INC HL */
static void op0x23(void)
{
	HL.low++;
	if (HL.low == 0)
		HL.high++;

}

/* INC SP */
static void op0x33(void)
{
	SP++;
}

static void init_optable(void)
{
	optable[0x00] = op0x00;
	optable[0x01] = op0x01;
	optable[0x02] = op0x02;
	optable[0x03] = op0x03;
	optable[0x04] = op0x04;
	optable[0x05] = op0x05;
	optable[0x06] = op0x06;
	optable[0x07] = op0x07;
	optable[0x08] = op0x08;
	optable[0x09] = op0x09;
	optable[0x0A] = op0x0A;
	optable[0x0B] = op0x0B;
	optable[0x0C] = op0x0C;
	optable[0x0D] = op0x0D;
	optable[0x0E] = op0x0E;
	optable[0x0F] = op0x0F;
	optable[0x10] = op0x10;
	optable[0x11] = op0x11;
	optable[0x12] = op0x12;
	optable[0x13] = op0x13;
	optable[0x14] = op0x14;
	optable[0x15] = op0x15;
	optable[0x16] = op0x16;
	optable[0x17] = op0x17;
	optable[0x18] = op0x18;
	optable[0x19] = op0x19;
	optable[0x1A] = op0x1A;
	optable[0x1B] = op0x1B;
	optable[0x1C] = op0x1C;
	optable[0x1D] = op0x1D;
	optable[0x1E] = op0x1E;
	optable[0x1F] = op0x1F;
	optable[0x23] = op0x23;
	optable[0x33] = op0x33;
}
