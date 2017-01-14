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

void execute_opcode(uint8_t opcode)
{
	optable[opcode]();
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

/* HALT */
static void op0x10(void)
{
	
}

/* LD DE,nn */
static void op0x11(void)
{
	uint16_t tmp = fetch_16bit_data();
	DE.high = tmp >> 8;
	DE.low = tmp;
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

/* JR NZ,n */
static void op0x20(void)
{
	if (!get_flag(ZFLAG))
		PC += fetch_8bit_data();
}

/* LD HL,nn */
static void op0x21(void)
{
	uint16_t tmp = fetch_16bit_data();
	HL.high = tmp >> 8;
	HL.low = tmp;
}

/* LDI (HL),A */
static void op0x22(void)
{
	uint16_t tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, AF.high);
	op0x23();
}

/* INC HL */
static void op0x23(void)
{
	HL.low++;
	if (HL.low == 0)
		HL.high++;

}

/* INC H */
static void op0x24(void)
{
	if ((HL.high & 0x0F) == 15)
		set_flag(HFLAG);

	HL.high++;
	if (HL.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC H */
static void op0x25(void)
{
	if ((HL.high & 0x0F) != 0)
		set_flag(HFLAG);

	HL.high--;
	if (HL.high == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD H,n */
static void op0x26(void)
{
	HL.high = fetch_8bit_data();
}

/* DAA */
static void op0x27(void)
{
	
}

/* JR Z,n */
static void op0x28(void)
{
	if (get_flag(ZFLAG))
		PC += fetch_8bit_data();
}

/*  */
static void op0x29(void)
{
	
}

/*  */
static void op0x2A(void)
{
	
}

/*  */
static void op0x2B(void)
{
	
}

/* INC L */
static void op0x2C(void)
{
	if ((HL.low & 0x0F) == 15)
		set_flag(HFLAG);

	HL.low++;
	if (HL.low == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
}

/* DEC L */
static void op0x2D(void)
{
	if ((HL.low & 0x0F) != 0)
		set_flag(HFLAG);

	HL.low--;
	if (HL.low == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
}

/* LD L,n */
static void op0x2E(void)
{
	HL.low = fetch_8bit_data();
}

/* */
static void op0x2F(void)
{

}

/* JR NC,n */
static void op0x30(void)
{
	if (!get_flag(CFLAG))
		PC += fetch_8bit_data();
}

/* LD SP,nn */
static void op0x31(void)
{
	SP = fetch_16bit_data();
}

/* */
static void op0x32(void)
{

}

/* INC SP */
static void op0x33(void)
{
	SP++;
}

/* */
static void op0x34(void)
{

}

/* */
static void op0x35(void)
{

}

/* */
static void op0x36(void)
{

}

/* */
static void op0x37(void)
{

}

/* JR C,n */
static void op0x38(void)
{
	if (get_flag(CFLAG))
		PC += fetch_8bit_data();
}

/* */
static void op0x39(void)
{

}

/* */
static void op0x3A(void)
{

}

/* */
static void op0x3B(void)
{

}

/* */
static void op0x3C(void)
{

}

/* */
static void op0x3D(void)
{

}

/* */
static void op0x3E(void)
{

}

/* */
static void op0x3F(void)
{

}

/* */
static void op0x40(void)
{

}

/* */
static void op0x41(void)
{

}

/* */
static void op0x42(void)
{

}

/* */
static void op0x43(void)
{

}

/* */
static void op0x44(void)
{

}

/* */
static void op0x45(void)
{

}

/* */
static void op0x46(void)
{

}

/* */
static void op0x47(void)
{

}

/* */
static void op0x48(void)
{

}

/* */
static void op0x49(void)
{

}

/* */
static void op0x4A(void)
{

}

/* */
static void op0x4B(void)
{

}

/* */
static void op0x4C(void)
{

}

/* */
static void op0x4D(void)
{

}

/* */
static void op0x4E(void)
{

}

/* */
static void op0x4F(void)
{

}

/* */
static void op0x50(void)
{

}

/* */
static void op0x51(void)
{

}

/* */
static void op0x52(void)
{

}

/* */
static void op0x53(void)
{

}

/* */
static void op0x54(void)
{

}

/* */
static void op0x55(void)
{

}

/* */
static void op0x56(void)
{

}

/* */
static void op0x57(void)
{

}

/* */
static void op0x58(void)
{

}

/* */
static void op0x59(void)
{

}

/* */
static void op0x5A(void)
{

}

/* */
static void op0x5B(void)
{

}

/* */
static void op0x5C(void)
{

}

/* */
static void op0x5D(void)
{

}

/* */
static void op0x5E(void)
{

}

/* */
static void op0x5F(void)
{

}

/* */
static void op0x60(void)
{

}

/* */
static void op0x61(void)
{

}

/* */
static void op0x62(void)
{

}

/* */
static void op0x63(void)
{

}

/* */
static void op0x64(void)
{

}

/* */
static void op0x65(void)
{

}

/* */
static void op0x66(void)
{

}

/* */
static void op0x67(void)
{

}

/* */
static void op0x68(void)
{

}

/* */
static void op0x69(void)
{

}

/* */
static void op0x6A(void)
{

}

/* */
static void op0x6B(void)
{

}

/* */
static void op0x6C(void)
{

}

/* */
static void op0x6D(void)
{

}

/* */
static void op0x6E(void)
{

}

/* */
static void op0x6F(void)
{

}

/* */
static void op0x70(void)
{

}

/* */
static void op0x71(void)
{

}

/* */
static void op0x72(void)
{

}

/* */
static void op0x73(void)
{

}

/* */
static void op0x74(void)
{

}

/* */
static void op0x75(void)
{

}

/* */
static void op0x76(void)
{

}

/* */
static void op0x77(void)
{

}

/* */
static void op0x78(void)
{

}

/* */
static void op0x79(void)
{

}

/* */
static void op0x7A(void)
{

}

/* */
static void op0x7B(void)
{

}

/* */
static void op0x7C(void)
{

}

/* */
static void op0x7D(void)
{

}

/* */
static void op0x7E(void)
{

}

/* */
static void op0x7F(void)
{

}

/* */
static void op0x80(void)
{

}

/* */
static void op0x81(void)
{

}

/* */
static void op0x82(void)
{

}

/* */
static void op0x83(void)
{

}

/* */
static void op0x84(void)
{

}

/* */
static void op0x85(void)
{

}

/* */
static void op0x86(void)
{

}

/* */
static void op0x87(void)
{

}

/* */
static void op0x88(void)
{

}

/* */
static void op0x89(void)
{

}

/* */
static void op0x8A(void)
{

}

/* */
static void op0x8B(void)
{

}

/* */
static void op0x8C(void)
{

}

/* */
static void op0x8D(void)
{

}

/* */
static void op0x8E(void)
{

}

/* */
static void op0x8F(void)
{

}

/* */
static void op0x90(void)
{

}

/* */
static void op0x91(void)
{

}

/* */
static void op0x92(void)
{

}

/* */
static void op0x93(void)
{

}

/* */
static void op0x94(void)
{

}

/* */
static void op0x95(void)
{

}

/* */
static void op0x96(void)
{

}

/* */
static void op0x97(void)
{

}

/* */
static void op0x98(void)
{

}

/* */
static void op0x99(void)
{

}

/* */
static void op0x9A(void)
{

}

/* */
static void op0x9B(void)
{

}

/* */
static void op0x9C(void)
{

}

/* */
static void op0x9D(void)
{

}

/* */
static void op0x9E(void)
{

}

/* */
static void op0x9F(void)
{

}

/* */
static void op0xA0(void)
{

}

/* */
static void op0xA1(void)
{

}

/* */
static void op0xA2(void)
{

}

/* */
static void op0xA3(void)
{

}

/* */
static void op0xA4(void)
{

}

/* */
static void op0xA5(void)
{

}

/* */
static void op0xA6(void)
{

}

/* */
static void op0xA7(void)
{

}

/* */
static void op0xA8(void)
{

}

/* */
static void op0xA9(void)
{

}

/* */
static void op0xAA(void)
{

}

/* */
static void op0xAB(void)
{

}

/* */
static void op0xAC(void)
{

}

/* */
static void op0xAD(void)
{

}

/* */
static void op0xAE(void)
{

}

/* */
static void op0xAF(void)
{

}

/* */
static void op0xB0(void)
{

}

/* */
static void op0xB1(void)
{

}

/* */
static void op0xB2(void)
{

}

/* */
static void op0xB3(void)
{

}

/* */
static void op0xB4(void)
{

}

/* */
static void op0xB5(void)
{

}

/* */
static void op0xB6(void)
{

}

/* */
static void op0xB7(void)
{

}

/* */
static void op0xB8(void)
{

}

/* */
static void op0xB9(void)
{

}

/* */
static void op0xBA(void)
{

}

/* */
static void op0xBB(void)
{

}

/* */
static void op0xBC(void)
{

}

/* */
static void op0xBD(void)
{

}

/* */
static void op0xBE(void)
{

}

/* */
static void op0xBF(void)
{

}

/* */
static void op0xC0(void)
{

}

/* */
static void op0xC1(void)
{

}

/* */
static void op0xC2(void)
{

}

/* */
static void op0xC3(void)
{

}

/* */
static void op0xC4(void)
{

}

/* */
static void op0xC5(void)
{

}

/* */
static void op0xC6(void)
{

}

/* */
static void op0xC7(void)
{

}

/* */
static void op0xC8(void)
{

}

/* */
static void op0xC9(void)
{

}

/* */
static void op0xCA(void)
{

}

/* */
static void op0xCB(void)
{

}

/* */
static void op0xCC(void)
{

}

/* */
static void op0xCD(void)
{

}

/* */
static void op0xCE(void)
{

}

/* */
static void op0xCF(void)
{

}

/* */
static void op0xD0(void)
{

}

/* */
static void op0xD1(void)
{

}

/* */
static void op0xD2(void)
{

}

/* */
static void op0xD3(void)
{

}

/* */
static void op0xD4(void)
{

}

/* */
static void op0xD5(void)
{

}

/* */
static void op0xD6(void)
{

}

/* */
static void op0xD7(void)
{

}

/* */
static void op0xD8(void)
{

}

/* */
static void op0xD9(void)
{

}

/* */
static void op0xDA(void)
{

}

/* */
static void op0xDB(void)
{

}

/* */
static void op0xDC(void)
{

}

/* */
static void op0xDD(void)
{

}

/* */
static void op0xDE(void)
{

}

/* */
static void op0xDF(void)
{

}

/* */
static void op0xE0(void)
{

}

/* */
static void op0xE1(void)
{

}

/* */
static void op0xE2(void)
{

}

/* */
static void op0xE3(void)
{

}

/* */
static void op0xE4(void)
{

}

/* */
static void op0xE5(void)
{

}

/* */
static void op0xE6(void)
{

}

/* */
static void op0xE7(void)
{

}

/* */
static void op0xE8(void)
{

}

/* */
static void op0xE9(void)
{

}

/* */
static void op0xEA(void)
{

}

/* */
static void op0xEB(void)
{

}

/* */
static void op0xEC(void)
{

}

/* */
static void op0xED(void)
{

}

/* */
static void op0xEE(void)
{

}

/* */
static void op0xEF(void)
{

}

/* */
static void op0xF0(void)
{

}

/* */
static void op0xF1(void)
{

}

/* */
static void op0xF2(void)
{

}

/* */
static void op0xF3(void)
{

}

/* */
static void op0xF4(void)
{

}

/* */
static void op0xF5(void)
{

}

/* */
static void op0xF6(void)
{

}

/* */
static void op0xF7(void)
{

}

/* */
static void op0xF8(void)
{

}

/* */
static void op0xF9(void)
{

}

/* */
static void op0xFA(void)
{

}

/* */
static void op0xFB(void)
{

}

/* */
static void op0xFC(void)
{

}

/* */
static void op0xFD(void)
{

}

/* */
static void op0xFE(void)
{

}

/* */
static void op0xFF(void)
{

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
	optable[0x20] = op0x20;
	optable[0x21] = op0x21;
	optable[0x22] = op0x22;
	optable[0x23] = op0x23;
	optable[0x24] = op0x24;
	optable[0x25] = op0x25;
	optable[0x26] = op0x26;
	optable[0x27] = op0x27;
	optable[0x28] = op0x28;
	optable[0x29] = op0x29;
	optable[0x2A] = op0x2A;
	optable[0x2B] = op0x2B;
	optable[0x2C] = op0x2C;
	optable[0x2D] = op0x2D;
	optable[0x2E] = op0x2E;
	optable[0x2F] = op0x2F;
	optable[0x30] = op0x30;
	optable[0x31] = op0x31;
	optable[0x32] = op0x32;
	optable[0x33] = op0x33;
	optable[0x34] = op0x34;
	optable[0x35] = op0x35;
	optable[0x36] = op0x36;
	optable[0x37] = op0x37;
	optable[0x38] = op0x38;
	optable[0x39] = op0x39;
	optable[0x3A] = op0x3A;
	optable[0x3B] = op0x3B;
	optable[0x3C] = op0x3C;
	optable[0x3D] = op0x3D;
	optable[0x3E] = op0x3E;
	optable[0x3F] = op0x3F;
	optable[0x40] = op0x40;
	optable[0x41] = op0x41;
	optable[0x42] = op0x42;
	optable[0x43] = op0x43;
	optable[0x44] = op0x44;
	optable[0x45] = op0x45;
	optable[0x46] = op0x46;
	optable[0x47] = op0x47;
	optable[0x48] = op0x48;
	optable[0x49] = op0x49;
	optable[0x4A] = op0x4A;
	optable[0x4B] = op0x4B;
	optable[0x4C] = op0x4C;
	optable[0x4D] = op0x4D;
	optable[0x4E] = op0x4E;
	optable[0x4F] = op0x4F;
	optable[0x50] = op0x50;
	optable[0x51] = op0x51;
	optable[0x52] = op0x52;
	optable[0x53] = op0x53;
	optable[0x54] = op0x54;
	optable[0x55] = op0x55;
	optable[0x56] = op0x56;
	optable[0x57] = op0x57;
	optable[0x58] = op0x58;
	optable[0x59] = op0x59;
	optable[0x5A] = op0x5A;
	optable[0x5B] = op0x5B;
	optable[0x5C] = op0x5C;
	optable[0x5D] = op0x5D;
	optable[0x5E] = op0x5E;
	optable[0x5F] = op0x5F;
	optable[0x60] = op0x60;
	optable[0x61] = op0x61;
	optable[0x62] = op0x62;
	optable[0x63] = op0x63;
	optable[0x64] = op0x64;
	optable[0x65] = op0x65;
	optable[0x66] = op0x66;
	optable[0x67] = op0x67;
	optable[0x68] = op0x68;
	optable[0x69] = op0x69;
	optable[0x6A] = op0x6A;
	optable[0x6B] = op0x6B;
	optable[0x6C] = op0x6C;
	optable[0x6D] = op0x6D;
	optable[0x6E] = op0x6E;
	optable[0x6F] = op0x6F;
	optable[0x70] = op0x70;
	optable[0x71] = op0x71;
	optable[0x72] = op0x72;
	optable[0x73] = op0x73;
	optable[0x74] = op0x74;
	optable[0x75] = op0x75;
	optable[0x76] = op0x76;
	optable[0x77] = op0x77;
	optable[0x78] = op0x78;
	optable[0x79] = op0x79;
	optable[0x7A] = op0x7A;
	optable[0x7B] = op0x7B;
	optable[0x7C] = op0x7C;
	optable[0x7D] = op0x7D;
	optable[0x7E] = op0x7E;
	optable[0x7F] = op0x7F;
	optable[0x80] = op0x80;
	optable[0x81] = op0x81;
	optable[0x82] = op0x82;
	optable[0x83] = op0x83;
	optable[0x84] = op0x84;
	optable[0x85] = op0x85;
	optable[0x86] = op0x86;
	optable[0x87] = op0x87;
	optable[0x88] = op0x88;
	optable[0x89] = op0x89;
	optable[0x8A] = op0x8A;
	optable[0x8B] = op0x8B;
	optable[0x8C] = op0x8C;
	optable[0x8D] = op0x8D;
	optable[0x8E] = op0x8E;
	optable[0x8F] = op0x8F;
	optable[0x90] = op0x90;
	optable[0x91] = op0x91;
	optable[0x92] = op0x92;
	optable[0x93] = op0x93;
	optable[0x94] = op0x94;
	optable[0x95] = op0x95;
	optable[0x96] = op0x96;
	optable[0x97] = op0x97;
	optable[0x98] = op0x98;
	optable[0x99] = op0x99;
	optable[0x9A] = op0x9A;
	optable[0x9B] = op0x9B;
	optable[0x9C] = op0x9C;
	optable[0x9D] = op0x9D;
	optable[0x9E] = op0x9E;
	optable[0x9F] = op0x9F;
	optable[0xA0] = op0xA0;
	optable[0xA1] = op0xA1;
	optable[0xA2] = op0xA2;
	optable[0xA3] = op0xA3;
	optable[0xA4] = op0xA4;
	optable[0xA5] = op0xA5;
	optable[0xA6] = op0xA6;
	optable[0xA7] = op0xA7;
	optable[0xA8] = op0xA8;
	optable[0xA9] = op0xA9;
	optable[0xAA] = op0xAA;
	optable[0xAB] = op0xAB;
	optable[0xAC] = op0xAC;
	optable[0xAD] = op0xAD;
	optable[0xAE] = op0xAE;
	optable[0xAF] = op0xAF;
	optable[0xB0] = op0xB0;
	optable[0xB1] = op0xB1;
	optable[0xB2] = op0xB2;
	optable[0xB3] = op0xB3;
	optable[0xB4] = op0xB4;
	optable[0xB5] = op0xB5;
	optable[0xB6] = op0xB6;
	optable[0xB7] = op0xB7;
	optable[0xB8] = op0xB8;
	optable[0xB9] = op0xB9;
	optable[0xBA] = op0xBA;
	optable[0xBB] = op0xBB;
	optable[0xBC] = op0xBC;
	optable[0xBD] = op0xBD;
	optable[0xBE] = op0xBE;
	optable[0xBF] = op0xBF;
	optable[0xC0] = op0xC0;
	optable[0xC1] = op0xC1;
	optable[0xC2] = op0xC2;
	optable[0xC3] = op0xC3;
	optable[0xC4] = op0xC4;
	optable[0xC5] = op0xC5;
	optable[0xC6] = op0xC6;
	optable[0xC7] = op0xC7;
	optable[0xC8] = op0xC8;
	optable[0xC9] = op0xC9;
	optable[0xCA] = op0xCA;
	optable[0xCB] = op0xCB;
	optable[0xCC] = op0xCC;
	optable[0xCD] = op0xCD;
	optable[0xCE] = op0xCE;
	optable[0xCF] = op0xCF;
	optable[0xD0] = op0xD0;
	optable[0xD1] = op0xD1;
	optable[0xD2] = op0xD2;
	optable[0xD3] = op0xD3;
	optable[0xD4] = op0xD4;
	optable[0xD5] = op0xD5;
	optable[0xD6] = op0xD6;
	optable[0xD7] = op0xD7;
	optable[0xD8] = op0xD8;
	optable[0xD9] = op0xD9;
	optable[0xDA] = op0xDA;
	optable[0xDB] = op0xDB;
	optable[0xDC] = op0xDC;
	optable[0xDD] = op0xDD;
	optable[0xDE] = op0xDE;
	optable[0xDF] = op0xDF;
	optable[0xE0] = op0xE0;
	optable[0xE1] = op0xE1;
	optable[0xE2] = op0xE2;
	optable[0xE3] = op0xE3;
	optable[0xE4] = op0xE4;
	optable[0xE5] = op0xE5;
	optable[0xE6] = op0xE6;
	optable[0xE7] = op0xE7;
	optable[0xE8] = op0xE8;
	optable[0xE9] = op0xE9;
	optable[0xEA] = op0xEA;
	optable[0xEB] = op0xEB;
	optable[0xEC] = op0xEC;
	optable[0xED] = op0xED;
	optable[0xEE] = op0xEE;
	optable[0xEF] = op0xEF;
	optable[0xF0] = op0xF0;
	optable[0xF1] = op0xF1;
	optable[0xF2] = op0xF2;
	optable[0xF3] = op0xF3;
	optable[0xF4] = op0xF4;
	optable[0xF5] = op0xF5;
	optable[0xF6] = op0xF6;
	optable[0xF7] = op0xF7;
	optable[0xF8] = op0xF8;
	optable[0xF9] = op0xF9;
	optable[0xFA] = op0xFA;
	optable[0xFB] = op0xFB;
	optable[0xFC] = op0xFC;
	optable[0xFD] = op0xFD;
	optable[0xFE] = op0xFE;
	optable[0xFF] = op0xFF;
}
