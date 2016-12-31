#include <stdint.h>

#include "memory.h"

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

	data = memory[PC];
	PC++;

	return data;
}

uint16_t fetch_16bit_data(void)
{
	uint16_t data;

	data = memory[PC] + (memory[PC+1] << 8);
	PC = PC + 2;

	return data;
}

void fetch_opcode(void)
{
	uint8_t opcode;

	opcode = memory[PC];
	PC++;
}

void set_zflag(void)
{
	AF.low |= 128;
}

void set_nflag(void)
{
	AF.low |= 64;
}

void set_hflag(void)
{
	AF.low |= 32;
}

void set_cflag(void)
{
	AF.low |= 16;
}

void reset_zflag(void)
{
	AF.low &= 127;
}

void reset_nflag(void)
{
	AF.low &= 181;
}

void reset_hflag(void)
{
	AF.low &= 223;
}

void reset_cflag(void)
{
	AF.low &= 239;
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


static void op0x04(void)
{

}

static void op0x05(void)
{

}

/* LD B,n */
static void op0x06(void)
{
	BC.high = fetch_8bit_data();
}

/* RLCA A */
static void op0x07(void)
{
	if (AF.high > 127)
		set_cflag();

	AF.high <<= 1;

	if (AF.high == 0)
		set_zflag();

	reset_nflag();
	reset_hflag();
}

static void op0x08(void)
{

}

/* ADD HL,BC */
static void op0x09(void)
{
	int tmp = HL.high + BC.high;
	tmp <<= 8;
	tmp += HL.low + BC.low;
	if (tmp > 65535)
		set_cflag();

	HL.high = tmp >> 8;
	HL.low = tmp;
	tmp = HL.high + BC.high;
	tmp &= 0x0F;
	tmp <<= 8;
	tmp += HL.low + BC.low;
	if (tmp > 4095)
		set_hflag();

	reset_nflag();

}

/* LD A,BC */
static void op0x0A(void)
{
	AF.high = BC.low;
}

static void op0x0B(void)
{

}

static void op0x0C(void)
{

}

static void op0x0D(void)
{

}

/* LD C,n */
static void op0x0E(void)
{
	BC.low = fetch_8bit_data();
}

static void op0x0F(void)
{

}

static void op0x10(void)
{

}

static void op0x11(void)
{

}

static void op0x12(void)
{

}

/* INC DE */
static void op0x13(void)
{
	DE.low++;
	if (DE.low == 0)
		DE.high++;

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
}
