#include "cpu.h"

uint8_t get_highest_bits(uint16_t reg)
{
	uint8_t highreg = reg >> 8;
	return highreg;
}

uint8_t get_lowest_bits(uint16_t reg)
{
	uint8_t lowreg = reg & 0x00FF;
	return lowreg;
}

void set_high_register(uint16_t reg, uint8_t value)
{

}

void set_low_register(uint16_t reg, uint8_t value)
{

}

uint8_t fetch_8bit_data(void)
{

}

uint16_t fetch_16bit_data(void)
{

}

void fetch_opcode(void)
{

}

int rotate_left(uint8_t reg)
{

}

void set_zflag(void)
{
	AF |= 128;
}

void set_nflag(void)
{
	AF |= 64;
}

void set_hflag(void)
{
	AF |= 32;
}

void set_cflag(void)
{
	AF |= 16;
}

void reset_zflag(void)
{
	AF &= 127;
}

void reset_nflag(void)
{
	AF &= 181;
}

void reset_hflag(void)
{
	AF &= 223;
}

void reset_cflag(void)
{
	AF &= 239;
}

void init_cpu(void)
{
	AF = 0x01B0;
	BC = 0x0013;
	DE = 0x00D8;
	HL = 0x014D;
	SP = 0xFFFE;

	PC = 0x100;
}

/* 0x00 */
static void nop(void)
{
	return;
}

/* LD BC,nn */
static void op0x01(void)
{
	BC = fetch_16bit_data();
}

/* LD BC,A */
static void op0x02(void)
{
	BC = get_highest_bits(AF);
}

/* INC BC */
static void op0x03(void)
{
	BC++;
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
	set_high_register(BC, fetch_8bit_data());
}

/* RLCA A */
static void op0x07(void)
{
	if (rotate_left(get_highest_bits(AF)))
		set_cflag();

	if (get_highest_bits(AF) == 0)
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
	int tmp = HL + BC;
	if (tmp > 65535)
		set_cflag();

	HL = tmp;
	tmp = (HL & 0x0FFF) + (BC & 0x0FFF);
	if (tmp > 4095)
		set_hflag();

	reset_nflag();

}

/* LD A,BC */
static void op0x0A(void)
{
	set_high_register(AF, BC);
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
	set_low_register(BC, fetch_8bit_data());
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
	DE++;
}

/* INC HL */
static void op0x23(void)
{
	HL++;
}

/* INC SP */
static void op0x33(void)
{
	SP++;
}
