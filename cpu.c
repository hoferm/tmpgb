#include "gameboy.h"

#include "interrupt.h"
#include "memory.h"
#include "opnames.h"

#define ZFLAG 0x80
#define NFLAG 0x40
#define HFLAG 0x20
#define CFLAG 0x10

static void init_optable(void);
static void init_cb_optable(void);

void (*optable[256])(void);
void (*cb_optable[256])(void);

static struct cpu_reg {
	u8 high;
	u8 low;
} AF, BC, DE, HL;

static u16 PC;
static u16 SP;

static u64 opcount = 0;

static int clock_count = 0;

static int disable_interrupt = 0;

void tick(void)
{
	clock_count += 4;
}

int cpu_cycle(void)
{
	return clock_count;
}

void push_stack(u8 low, u8 high)
{
	SP--;
	write_memory(SP, high);
	SP--;
	write_memory(SP, low);
}

u16 pop_stack(void)
{
	u16 value;
	value = read_memory(SP);
	value += (read_memory(SP + 1) << 8);

	SP += 2;

	log_msg("Stack: %.4X\n", value);
	return value;
}

u8 fetch_8bit_data(void)
{
	u8 data;

	data = read_memory(PC);
	PC++;

	log_msg("Byte value: %.2X\n", data);

	return data;
}

u16 fetch_16bit_data(void)
{
	u16 data;

	data = read_memory(PC) + (read_memory(PC + 1) << 8);
	PC = PC + 2;

	log_msg("2 Byte value: %.4X\n", data);

	return data;
}

void execute_opcode(u8 opcode)
{
	int interrupt = execute_interrupt();
	if (interrupt) {
		push_stack(PC, PC >> 8);
		PC = interrupt;
	}
	if (opcode > 0)
		log_msg("%d: %s (%.2X): PC: %.4X, SP: %.4X\n", opcount, op_names[opcode], opcode, PC, SP);

	optable[opcode]();

	if (disable_interrupt)
		set_ime(0);
	opcount++;
}

void fetch_opcode(void)
{
	u8 opcode;

	opcode = read_memory(PC);
	PC++;

	execute_opcode(opcode);
}

static void set_flag(u8 flag)
{
	AF.low |= flag;
}

static void reset_flag(u8 flag)
{
	AF.low &= (0xFF - flag);
}

static u8 get_flag(u8 flag)
{
	return (AF.low & flag) ? 1 : 0;
}

static u8 inc_8bit(u8 reg)
{
	if ((reg & 0x0F) == 15)
		set_flag(HFLAG);

	reg++;
	if (reg == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	return reg;
}

static u8 dec_8bit(u8 reg)
{
	if ((reg & 0x0F) == 0)
		set_flag(HFLAG);

	reg--;
	if (reg == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);
	return reg;
}

static void add(u8 val, int with_carry)
{
	u16 res;

	if (with_carry)
		val += get_flag(CFLAG);

	res = AF.high + val;

	if (res == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);

	if ((AF.high ^ val ^ res) & 0x10)
		set_flag(HFLAG);

	if (res > 255)
		set_flag(CFLAG);

	AF.high = res;
}

static void add_HL(u16 val)
{
	int tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low + val;
	if (tmp > 65535)
		set_flag(CFLAG);

	HL.high = tmp >> 8;
	HL.low = tmp;
	tmp = HL.high + (val >> 8);
	tmp &= 0x0F;
	tmp <<= 8;
	tmp += HL.low + (val & 0x00FF);
	if (tmp > 4095)
		set_flag(HFLAG);

	reset_flag(NFLAG);
}

static void sub(u8 val, int with_carry)
{
	u8 res;

	if (with_carry)
		val += get_flag(CFLAG);

	res = AF.high - val;

	if (res == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);

	if ((AF.high & 0xF) >= (val & 0xF))
		set_flag(HFLAG);

	if (AF.high >= val)
		set_flag(CFLAG);
}

static void and(u8 val)
{
	AF.high &= val;

	if (AF.high == 0)
		set_flag(ZFLAG);

	set_flag(HFLAG);
	reset_flag(NFLAG);
	reset_flag(CFLAG);
}

static void xor(u8 val)
{
	AF.high ^= val;

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);
}

static void or(u8 val)
{
	AF.high |= val;

	if (AF.high == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);
}

static void cmp(u8 val)
{
	u8 tmp = AF.high - val;

	if (tmp == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);

	if ((AF.high & 0xF) < (val & 0xF))
		set_flag(HFLAG);

	if (AF.high < val)
		set_flag(CFLAG);
}

static void rst(u8 offset)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;

	push_stack(low, high);

	PC = 0x0000 + offset;
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
	init_cb_optable();
}

/* NOP */
static void op0x00(void)
{
	return;
}

/* LD BC,nn */
static void op0x01(void)
{
	u16 temp = fetch_16bit_data();
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
	BC.high = inc_8bit(BC.high);
}

/* DEC B */
static void op0x05(void)
{
	BC.high = dec_8bit(BC.high);
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
	u16 addr = fetch_16bit_data();
	write_memory(addr, SP & 0x00FF);
	write_memory(addr+1, SP >> 8);
}

/* ADD HL,BC */
static void op0x09(void)
{
	u16 tmp = BC.high;
	tmp <<= 8;
	add_HL(tmp + BC.low);
}

/* LD A,(BC) */
static void op0x0A(void)
{
	u16 tmp = BC.high;
	tmp <<= 8;
	tmp += BC.low;
	AF.high = read_memory(tmp);
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
	BC.low = inc_8bit(BC.low);
}

/* DEC C */
static void op0x0D(void)
{
	BC.low = dec_8bit(BC.low);
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

/* STOP */
static void op0x10(void)
{

}

/* LD DE,nn */
static void op0x11(void)
{
	u16 tmp = fetch_16bit_data();
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
	DE.high = inc_8bit(DE.high);
}

/* DEC D */
static void op0x15(void)
{
	DE.high = dec_8bit(DE.high);
}

/* LD D,n */
static void op0x16(void)
{
	DE.high = fetch_8bit_data();
}

/* RLA */
static void op0x17(void)
{
	u8 tmp = get_flag(CFLAG);
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
	u16 tmp = DE.high;
	tmp <<= 8;
	add_HL(tmp + DE.low);
}

/* LD A,(DE) */
static void op0x1A(void)
{
	u16 tmp = DE.high;
	tmp <<= 8;
	tmp += DE.low;
	AF.high = read_memory(tmp);
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
	DE.low = inc_8bit(DE.low);
}

/* DEC E */
static void op0x1D(void)
{
	DE.low = dec_8bit(DE.low);
}

/* LD E,n */
static void op0x1E(void)
{
	DE.low = fetch_8bit_data();
}

/* RRA */
static void op0x1F(void)
{
	u8 tmp = get_flag(CFLAG);
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
	u16 tmp = fetch_16bit_data();
	HL.high = tmp >> 8;
	HL.low = tmp;
}

/* LDI (HL),A */
static void op0x22(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, AF.high);
	HL.low++;
	if (HL.low == 0)
		HL.high++;
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
	HL.high = inc_8bit(HL.high);
}

/* DEC H */
static void op0x25(void)
{
	HL.high = dec_8bit(HL.high);
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

/* ADD HL,HL */
static void op0x29(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	add_HL(tmp + HL.low);
}

/* LDI A,(HL) */
static void op0x2A(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	AF.high = read_memory(tmp);
	op0x23();
}

/* DEC HL */
static void op0x2B(void)
{
	if (HL.low == 0)
		HL.high--;

	HL.low--;
}

/* INC L */
static void op0x2C(void)
{
	HL.low = inc_8bit(HL.low);
}

/* DEC L */
static void op0x2D(void)
{
	HL.low = dec_8bit(HL.low);
}

/* LD L,n */
static void op0x2E(void)
{
	HL.low = fetch_8bit_data();
}

/* CPL */
static void op0x2F(void)
{
	AF.high = ~AF.high;
	set_flag(NFLAG);
	set_flag(HFLAG);
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

/* LDD (HL),A */
static void op0x32(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, AF.high);

	if (HL.low == 0)
		HL.high--;

	HL.low--;
}

/* INC SP */
static void op0x33(void)
{
	SP++;
}

/* INC (HL) */
static void op0x34(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, inc_8bit(read_memory(tmp)));
}

/* DEC (HL) */
static void op0x35(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;

	write_memory(tmp, dec_8bit(read_memory(tmp)));

}

/* LD (HL),n */
static void op0x36(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, fetch_8bit_data());
}

/* SCF */
static void op0x37(void)
{
	set_flag(CFLAG);
	reset_flag(NFLAG);
	reset_flag(HFLAG);
}

/* JR C,n */
static void op0x38(void)
{
	if (get_flag(CFLAG))
		PC += fetch_8bit_data();
}

/* ADD HL,SP */
static void op0x39(void)
{
	add_HL(SP);
}

/* LDD A,(HL) */
static void op0x3A(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	AF.high = read_memory(tmp);
	op0x2B();
}

/* DEC SP */
static void op0x3B(void)
{
	SP--;
}

/* INC A */
static void op0x3C(void)
{
	AF.high = inc_8bit(AF.high);
}

/* DEC A */
static void op0x3D(void)
{
	AF.high = dec_8bit(AF.high);
}

/* LD A,n */
static void op0x3E(void)
{
	AF.high = fetch_8bit_data();
}

/* CCF */
static void op0x3F(void)
{
	if (get_flag(CFLAG))
		reset_flag(CFLAG);
	else
		set_flag(CFLAG);
}

/* LD B,B */
static void op0x40(void)
{
	BC.high = BC.high;
}

/* LD B,C */
static void op0x41(void)
{
	BC.high = BC.low;
}

/* LD B,D */
static void op0x42(void)
{
	BC.high = DE.high;
}

/* LD B,E */
static void op0x43(void)
{
	BC.high = DE.low;
}

/* LD B,H */
static void op0x44(void)
{
	BC.high = HL.high;
}

/* LD B,L */
static void op0x45(void)
{
	BC.high = HL.low;
}

/* LD B,(HL)*/
static void op0x46(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	BC.high = read_memory(tmp);
}

/* LD B,A */
static void op0x47(void)
{
	BC.high = AF.high;
}

/* LD C,B */
static void op0x48(void)
{
	BC.low = BC.high;
}

/* LD C,C */
static void op0x49(void)
{
	BC.low = BC.low;
}

/* LD C,D */
static void op0x4A(void)
{
	BC.low = DE.high;
}

/* LD C,E */
static void op0x4B(void)
{
	BC.low = DE.low;
}

/* LD C,H */
static void op0x4C(void)
{
	BC.low = HL.high;
}

/* LD C,L */
static void op0x4D(void)
{
	BC.low = HL.low;
}

/* LD C,(HL) */
static void op0x4E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	BC.low = read_memory(tmp);
}

/* LD C,A */
static void op0x4F(void)
{
	BC.low = AF.high;
}

/* LD D,B */
static void op0x50(void)
{
	DE.high = BC.high;
}

/* LD D,C */
static void op0x51(void)
{
	DE.high = BC.low;
}

/* LD D,D */
static void op0x52(void)
{
	DE.high = DE.high;
}

/* LD D,E */
static void op0x53(void)
{
	DE.high = DE.low;
}

/* LD D,H */
static void op0x54(void)
{
	DE.high = HL.high;
}

/* LD D,L */
static void op0x55(void)
{
	DE.high = HL.low;
}

/* LD D,(HL) */
static void op0x56(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	DE.high = read_memory(tmp);
}

/* LD D,A */
static void op0x57(void)
{
	DE.high = AF.high;
}

/* LD E,B */
static void op0x58(void)
{
	DE.low = BC.high;
}

/* LD E,C */
static void op0x59(void)
{
	DE.low = BC.low;
}

/* LD E,D */
static void op0x5A(void)
{
	DE.low = DE.high;
}

/* LD E,E */
static void op0x5B(void)
{
	DE.low = DE.low;
}

/* LD E,H */
static void op0x5C(void)
{
	DE.low = HL.high;
}

/* LD E,L */
static void op0x5D(void)
{
	DE.low = HL.low;
}

/* LD E,(HL) */
static void op0x5E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	DE.low = read_memory(tmp);
}

/* LD E,A */
static void op0x5F(void)
{
	DE.low = AF.high;
}

/* LD H,B */
static void op0x60(void)
{
	HL.high = BC.high;
}

/* LD H,C */
static void op0x61(void)
{
	HL.high = BC.low;
}

/* LD H,D */
static void op0x62(void)
{
	HL.high = DE.high;
}

/* LD H,E */
static void op0x63(void)
{
	HL.high = DE.low;
}

/* LD H,H */
static void op0x64(void)
{
	HL.high = HL.high;
}

/* LD H,L */
static void op0x65(void)
{
	HL.high = HL.low;
}

/* LD H,(HL) */
static void op0x66(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	HL.high = read_memory(tmp);
}

/* LD H,A */
static void op0x67(void)
{
	HL.high = AF.high;
}

/* LD L,B */
static void op0x68(void)
{
	HL.low = BC.high;
}

/* LD L,C */
static void op0x69(void)
{
	HL.low = BC.low;
}

/* LD L,D */
static void op0x6A(void)
{
	HL.low = DE.high;
}

/* LD L,E */
static void op0x6B(void)
{
	HL.low = DE.low;
}

/* LD L,H */
static void op0x6C(void)
{
	HL.low = HL.high;
}

/* LD L,L */
static void op0x6D(void)
{
	HL.low = HL.low;
}

/* LD L,(HL) */
static void op0x6E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	HL.low = read_memory(tmp);
}

/* LD L,A */
static void op0x6F(void)
{
	HL.low = AF.high;
}

/* LD (HL),B */
static void op0x70(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, BC.high);
}

/* LD (HL),C */
static void op0x71(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, BC.low);
}

/* LD (HL),D */
static void op0x72(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, DE.high);
}

/* LD (HL),E */
static void op0x73(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, DE.low);
}

/* LD (HL),H */
static void op0x74(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, HL.high);
}

/* LD (HL),L */
static void op0x75(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, HL.low);
}

/* HALT */
static void op0x76(void)
{

}

/* LD (HL),A */
static void op0x77(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	write_memory(tmp, AF.high);
}

/* LD A,B */
static void op0x78(void)
{
	AF.high = BC.high;
}

/* LD A,C */
static void op0x79(void)
{
	AF.high = BC.low;
}

/* LD A,D */
static void op0x7A(void)
{
	AF.high = DE.high;
}

/* LD A,E */
static void op0x7B(void)
{
	AF.high = DE.low;
}

/* LD A,H */
static void op0x7C(void)
{
	AF.high = HL.high;
}

/* LD A,L */
static void op0x7D(void)
{
	AF.high = HL.low;
}

/* LD A,(HL) */
static void op0x7E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	AF.high = read_memory(tmp);
}

/* LD A,A */
static void op0x7F(void)
{
	AF.high = AF.high;
}

/* ADD A,B */
static void op0x80(void)
{
	add(BC.high, 0);
}

/* ADD A,C */
static void op0x81(void)
{
	add(BC.low, 0);
}

/* ADD A,D */
static void op0x82(void)
{
	add(DE.high, 0);
}

/* ADD A,E */
static void op0x83(void)
{
	add(DE.low, 0);
}

/* ADD A,H */
static void op0x84(void)
{
	add(HL.high, 0);
}

/* ADD A,L */
static void op0x85(void)
{
	add(HL.low, 0);
}

/* ADD A,(HL) */
static void op0x86(void)
{
	u16 tmp = (HL.high << 8) + HL.low;
	add(read_memory(tmp), 0);
}

/* ADD A,A */
static void op0x87(void)
{
	add(AF.high, 0);
}

/* ADC A,B */
static void op0x88(void)
{
	add(BC.high, 1);
}

/* ADC A,C */
static void op0x89(void)
{
	add(BC.low, 1);
}

/* ADC A,D */
static void op0x8A(void)
{
	add(DE.high, 1);
}

/* ADC A,E */
static void op0x8B(void)
{
	add(DE.low, 1);
}

/* ADC A,H */
static void op0x8C(void)
{
	add(HL.high, 1);
}

/* ADC A,L */
static void op0x8D(void)
{
	add(HL.low, 1);
}

/* ADC A,(HL) */
static void op0x8E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	add(read_memory(tmp), 1);
}

/* ADC A,A */
static void op0x8F(void)
{
	add(AF.high, 1);
}

/* SUB A,B */
static void op0x90(void)
{
	sub(BC.high, 0);
}

/* SUB A,C */
static void op0x91(void)
{
	sub(BC.low, 0);
}

/* SUB A,D */
static void op0x92(void)
{
	sub(DE.high, 0);
}

/* SUB A,E */
static void op0x93(void)
{
	sub(DE.low, 0);
}

/* SUB A,H */
static void op0x94(void)
{
	sub(HL.high, 0);
}

/* SUB A,L */
static void op0x95(void)
{
	sub(HL.low, 0);
}

/* SUB A,(HL) */
static void op0x96(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	sub(read_memory(tmp), 0);
}

/* SUB A,A */
static void op0x97(void)
{
	sub(AF.high, 0);
}

/* SBC A,B */
static void op0x98(void)
{
	sub(BC.high, 1);
}

/* SBC A,C */
static void op0x99(void)
{
	sub(BC.low, 1);
}

/* SBC A,D */
static void op0x9A(void)
{
	sub(DE.high, 1);
}

/* SBC A,E */
static void op0x9B(void)
{
	sub(DE.low, 1);
}

/* SBC A,H */
static void op0x9C(void)
{
	sub(HL.high, 1);
}

/* SBC A,L */
static void op0x9D(void)
{
	sub(HL.low, 1);
}

/* SBC A,(HL) */
static void op0x9E(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	sub(read_memory(tmp), 1);
}

/* SBC A,A */
static void op0x9F(void)
{
	sub(AF.high, 1);
}

/* AND A,B */
static void op0xA0(void)
{
	and(BC.high);
}

/* AND A,C */
static void op0xA1(void)
{
	and(BC.low);
}

/* AND A,D */
static void op0xA2(void)
{
	and(DE.high);
}

/* AND A,E */
static void op0xA3(void)
{
	and(DE.low);
}

/* AND A,H */
static void op0xA4(void)
{
	and(HL.high);
}

/* AND A,L */
static void op0xA5(void)
{
	and(HL.low);
}

/* AND A,(HL)*/
static void op0xA6(void)
{
	u16 tmp = (HL.high << 8) + HL.low;
	and(read_memory(tmp));
}

/* AND A,A */
static void op0xA7(void)
{
	and(AF.high);
}

/* XOR A,B */
static void op0xA8(void)
{
	xor(BC.high);
}

/* XOR A,C */
static void op0xA9(void)
{
	xor(BC.low);
}

/* XOR A,D */
static void op0xAA(void)
{
	xor(DE.high);
}

/* XOR A,E */
static void op0xAB(void)
{
	xor(DE.low);
}

/* XOR A,H */
static void op0xAC(void)
{
	xor(HL.high);
}

/* XOR A,L */
static void op0xAD(void)
{
	xor(HL.low);
}

/* XOR A,(HL) */
static void op0xAE(void)
{
	u16 tmp = (HL.high << 8) + HL.low;
	xor(read_memory(tmp));
}

/* XOR A,A */
static void op0xAF(void)
{
	AF.high = 0;
}

/* OR A,B  */
static void op0xB0(void)
{
	or(BC.high);
}

/* OR A,C */
static void op0xB1(void)
{
	or(BC.low);
}

/* OR A,D */
static void op0xB2(void)
{
	or(DE.high);
}

/* OR A,E */
static void op0xB3(void)
{
	or(DE.low);
}

/* OR A,H */
static void op0xB4(void)
{
	or(HL.high);
}

/* OR A,L */
static void op0xB5(void)
{
	or(HL.low);
}

/* OR A,(HL) */
static void op0xB6(void)
{
	u8 tmp = (HL.high << 8) + HL.low;
	or(read_memory(tmp));
}

/* OR A,A */
static void op0xB7(void)
{
	or(AF.high);
}

/* CP A,B */
static void op0xB8(void)
{
	cmp(BC.high);
}

/* CP A,C */
static void op0xB9(void)
{
	cmp(BC.low);
}

/* CP A,D */
static void op0xBA(void)
{
	cmp(DE.high);
}

/* CP A,E */
static void op0xBB(void)
{
	cmp(DE.low);
}

/* CP A,H */
static void op0xBC(void)
{
	cmp(HL.high);
}

/* CP A,L */
static void op0xBD(void)
{
	cmp(HL.low);
}

/* CP A,(HL) */
static void op0xBE(void)
{
	u16 tmp = (HL.high << 8) + HL.low;
	cmp(read_memory(tmp));
}

/* CP A,A */
static void op0xBF(void)
{
	cmp(AF.high);
}

/* RET NZ */
static void op0xC0(void)
{
	if (!get_flag(ZFLAG))
		PC = pop_stack();
}

/* POP BC*/
static void op0xC1(void)
{
	u16 tmp = pop_stack();

	BC.high = tmp >> 8;
	BC.low = tmp;
}

/* JP NZ,nn */
static void op0xC2(void)
{
	u16 address;
	if (!get_flag(ZFLAG)) {
		address = fetch_16bit_data();
		PC = address;
	}
}

/* JP nn */
static void op0xC3(void)
{
	u16 address = fetch_16bit_data();
	PC = address;
}

/* CALL NZ,nn */
static void op0xC4(void)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;
	u16 address;

	if (!get_flag(ZFLAG)) {
		push_stack(low, high);
		address = fetch_16bit_data();
		PC = address;
	}
}

/* PUSH BC */
static void op0xC5(void)
{
	push_stack(BC.low, BC.high);
}

/* ADD A,n */
static void op0xC6(void)
{
	u8 tmp = fetch_8bit_data();
	add(tmp, 0);
}

/* RST 0x00 */
static void op0xC7(void)
{
	rst(0x00);
}

/* RET Z */
static void op0xC8(void)
{
	if (get_flag(ZFLAG))
		PC = pop_stack();
}

/* RET */
static void op0xC9(void)
{
	PC = pop_stack();
}

/* JP Z,nn */
static void op0xCA(void)
{
	u16 address;

	if (get_flag(ZFLAG)) {
		address = fetch_16bit_data();
		PC = address;
	}
}

/* CB Prefix */
static void op0xCB(void)
{
	u8 cb_opcode = fetch_8bit_data();

	cb_optable[cb_opcode]();
}

/* CALL Z,nn */
static void op0xCC(void)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;
	u16 address;

	if (get_flag(ZFLAG)) {
		push_stack(low, high);
		address = fetch_16bit_data();
		PC = address;
	}
}

/* CALL nn */
static void op0xCD(void)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;
	u16 address = fetch_16bit_data();

	push_stack(low, high);

	PC = address;
}

/* ADC A,n */
static void op0xCE(void)
{
	u8 tmp = fetch_8bit_data();
	add(tmp, 1);
}

/* RST 0x08 */
static void op0xCF(void)
{
	rst(0x08);
}

/* RET NC */
static void op0xD0(void)
{
	u16 address;

	if (!get_flag(CFLAG)) {
		address = pop_stack();
		PC = address;
	}
}

/* POP DE */
static void op0xD1(void)
{
	u16 tmp = pop_stack();

	DE.high = tmp >> 8;
	DE.low = tmp;
}

/* JP NC,nn */
static void op0xD2(void)
{
	u16 address;

	if (!get_flag(CFLAG)) {
		address = fetch_16bit_data();
		PC = address;
	}
}

/* N/A */
static void op0xD3(void)
{

}

/* CALL NC,nn */
static void op0xD4(void)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;
	u16 address;

	if (!get_flag(CFLAG)) {
		push_stack(low, high);
		address = fetch_16bit_data();
		PC = address;
	}
}

/* PUSH DE */
static void op0xD5(void)
{
	u8 low = DE.low;
	u8 high = DE.high;

	push_stack(low, high);
}

/* SUB A,n */
static void op0xD6(void)
{
	u8 tmp = fetch_8bit_data();

	sub(tmp, 0);
}

/* RST 0x10 */
static void op0xD7(void)
{
	rst(0x10);
}

/* RET C */
static void op0xD8(void)
{
	u16 address;

	if (get_flag(CFLAG)) {
		address = pop_stack();
		PC = address;
	}
}

/* RETI */
static void op0xD9(void)
{
	PC = pop_stack();
	set_ime(1);
}

/* JP C,nn */
static void op0xDA(void)
{
	u16 address;

	if (get_flag(CFLAG)) {
		address = fetch_16bit_data();
		PC = address;
	}
}

/* N/A */
static void op0xDB(void)
{

}

/* CALL C,nn */
static void op0xDC(void)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;
	u16 address;

	if (get_flag(CFLAG)) {
		push_stack(low, high);
		address = fetch_16bit_data();
		PC = address;
	}
}

/* N/A */
static void op0xDD(void)
{

}

/* SBC A,n */
static void op0xDE(void)
{
	u8 tmp = fetch_8bit_data();

	sub(tmp, 1);
}

/* RST 0x18 */
static void op0xDF(void)
{
	rst(0x18);
}

/* LDH (n),A */
static void op0xE0(void)
{
	u16 address = fetch_8bit_data();

	address += 0xFF00;

	write_memory(address, AF.high);
}

/* POP HL */
static void op0xE1(void)
{
	u16 tmp = pop_stack();

	HL.high = tmp >> 8;
	HL.low = tmp;
}

/* LD (C),A */
static void op0xE2(void)
{
	u16 address = 0xFF00 + BC.low;
	write_memory(address, AF.high);
}

/* N/A */
static void op0xE3(void)
{

}

/* N/A */
static void op0xE4(void)
{

}

/* PUSH HL */
static void op0xE5(void)
{
	push_stack(HL.low, HL.high);
}

/* AND A,n */
static void op0xE6(void)
{
	u8 tmp = fetch_8bit_data();
	and(tmp);
}

/* RST 0x20 */
static void op0xE7(void)
{
	rst(0x20);
}

/* ADD SP,n */
static void op0xE8(void)
{
	u8 tmp = fetch_8bit_data();
	int res = tmp + SP;

	reset_flag(ZFLAG);
	reset_flag(NFLAG);

	if ((tmp ^ SP ^ res) & 0x1000)
		set_flag(HFLAG);

	if (res > 0xFFFF)
		set_flag(CFLAG);

	SP = res;
}

/* JP (HL) */
static void op0xE9(void)
{
	PC = (HL.high << 8) + HL.low;
}

/* LD (nn),A */
static void op0xEA(void)
{
	u16 address = fetch_16bit_data();
	write_memory(address, AF.high);
}

/* N/A */
static void op0xEB(void)
{

}

/* N/A */
static void op0xEC(void)
{

}

/* N/A */
static void op0xED(void)
{

}

/* XOR A,n */
static void op0xEE(void)
{
	u8 tmp = fetch_8bit_data();
	xor(tmp);
}

/* RST 0x28 */
static void op0xEF(void)
{
	rst(0x28);
}

/* LDH A,(n) */
static void op0xF0(void)
{
	u8 tmp = fetch_8bit_data();

	AF.high = read_memory(0xFF00 + tmp);
}

/* POP AF */
static void op0xF1(void)
{
	u16 tmp = pop_stack();

	AF.high = tmp >> 8;
	AF.low = tmp & 0xFF;
}

/* LD A,(C) */
static void op0xF2(void)
{
	u8 tmp = read_memory(0xFF00 + BC.low);

	AF.high = tmp;
}

/* DI */
static void op0xF3(void)
{
	disable_interrupt = 1;
}

/* N/A */
static void op0xF4(void)
{

}

/* PUSH AF */
static void op0xF5(void)
{
	push_stack(AF.low, AF.high);
}

/* OR A,n */
static void op0xF6(void)
{
	u8 tmp = fetch_8bit_data();
	or(tmp);
}

/* RST 0x30 */
static void op0xF7(void)
{
	rst(0x30);
}

/* LD HL,SP+n */
static void op0xF8(void)
{
	char value = fetch_8bit_data();
	int result;
	result = SP + value;
	if (result > 0xFFFF)
		set_flag(CFLAG);
	else
		reset_flag(CFLAG);

	HL.low = result & 0x00FF;
	HL.high = result >> 8;
	result = 0;
	result = (SP & 0x0FFF) + (value & 0x0FFF);
	if (result > 0x0FFF)
		set_flag(HFLAG);
	else
		reset_flag(HFLAG);

	reset_flag(ZFLAG);
	reset_flag(NFLAG);
}

/* LD SP,HL */
static void op0xF9(void)
{
	u16 tmp = HL.high;
	tmp <<= 8;
	tmp += HL.low;
	SP = tmp;
}

/* LD A,(nn) */
static void op0xFA(void)
{
	u16 address = fetch_16bit_data();

	AF.high = read_memory(address);
}

/* EI */
static void op0xFB(void)
{
	set_ime(1);
}

/* N/A */
static void op0xFC(void)
{

}

/* N/A */
static void op0xFD(void)
{

}

/* CP A,n */
static void op0xFE(void)
{
	u8 tmp = fetch_8bit_data();
	cmp(tmp);
}

/* RST 0x38 */
static void op0xFF(void)
{
	rst(0x38);
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

static void init_cb_optable(void)
{
}
