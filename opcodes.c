#include "cpu.h"

/* 0x00 */
void nop(void)
{
	return;
}

/* LD BC,nn */
void op0x01(void)
{
	BC = fetch_16bit_data();
}

/* LD BC,A */
void op0x02(void)
{
	BC = get_high_register(AF);
}

/* INC BC */
void op0x03(void)
{
	BC++;
}


void op0x04(void)
{
	
}

void op0x05(void)
{
	
}

/* LD B,n */
void op0x06(void)
{
	set_high_register(BC, fetch_8bit_data());
}

/* RLCA A */
void op0x07(void)
{
	
	if (rotate_left(get_highest_bits(AF))
		set_cflag();
	
	if (get_highest_bits(AF) == 0)
		set_zflag();
	
	reset_nflag();
	reset_hflag();
}

void op0x08(void)
{
	
}

/* ADD HL,BC */
void op0x09(void)
{
	
	int tmp = HL + BC;
	if (tmp > 65535)
		set_cflag();
	
	HL = tmp;
	tmp = (HL & 0x0FFF) + (BC & 0x0FFF)
	if (tmp > 4095)
		set_hflag();
		
	reset_nflag;
	
}

/* LD A,BC */
void op0x0A(void)
{
	set_high_register(AF, BC);
}

void op0x0B(void)
{
	
}

void op0x0C(void)
{
	
}

void op0x0D(void)
{
	
}

/* LD C,n */
void op0x0E(void)
{
	set_low_register(BC, fetch_8bit_data());
}

void op0x0F(void)
{
	
}

void op0x10(void)
{
	
}

void op0x11(void)
{
	
}

void op0x12(void)
{
	
}

/* INC DE */
void op0x13(void)
{
	DE++;
}

/* INC HL */
void op0x23(void)
{
	HL++;
}

/* INC SP */
void op0x33(void)
{
	SP++;
}