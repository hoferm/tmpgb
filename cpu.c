#include "gameboy.h"

#include "interrupt.h"
#include "memory.h"

#define ZFLAG 0x80
#define NFLAG 0x40
#define HFLAG 0x20
#define CFLAG 0x10

static void init_optable(void);
static void init_cb_optable(void);

static void (*optable[256])(void);
static void (*cb_optable[256])(void);

static u8 B;
static u8 C;
static u8 D;
static u8 E;
static u8 H;
static u8 L;
static u8 F;
static u8 A;

static u16 PC;
static u16 SP;

static int clock_count = 0;
static int old_clock_count = 0;

static int ime_scheduled = 0;
static int stopped = 0;

static void tick(int n)
{
	clock_count += 4 * n;
}

int cpu_cycle(void)
{
	return clock_count;
}

int old_cpu_cycle(void)
{
	return old_clock_count;
}

static void reset_clock_count(void)
{
	clock_count = 0;
	old_clock_count = 0;
}

static void cpu_write_mem(u16 addr, u8 val)
{
	write_memory(addr, val);
	tick(1);
}

static u8 cpu_read_mem(u16 addr)
{
	tick(1);
	return read_memory(addr);
}

static void push_stack(u8 low, u8 high)
{
	SP--;
	cpu_write_mem(SP, high);
	SP--;
	cpu_write_mem(SP, low);
	tick(1);
}

static u16 pop_stack(void)
{
	u16 value;
	value = cpu_read_mem(SP);
	value += (cpu_read_mem(SP + 1) << 8);

	SP += 2;
	return value;
}

static u8 fetch_8bit_data(void)
{
	u8 data;

	data = cpu_read_mem(PC);
	PC++;

	return data;
}

static u16 fetch_16bit_data(void)
{
	u16 data;

	data = cpu_read_mem(PC) + (cpu_read_mem(PC + 1) << 8);
	PC = PC + 2;

	return data;
}

static void execute_opcode(u8 opcode)
{
	if (ime_scheduled) {
		set_ime(1);
		ime_scheduled = 0;
	}
	optable[opcode]();
}

void fetch_opcode(void)
{
	u8 opcode;
	int interrupt = execute_interrupt();

	if (clock_count >= 1024)
		reset_clock_count();
	old_clock_count = clock_count;

	if (interrupt) {
		push_stack(PC, PC >> 8);
		PC = interrupt;
	}
	opcode = cpu_read_mem(PC);
	PC++;

	execute_opcode(opcode);
}

static void set_flag(u8 flag)
{
	F |= flag;
}

static void reset_flag(u8 flag)
{
	F &= (0xFF - flag);
}

static u8 get_flag(u8 flag)
{
	return (F & flag) ? 1 : 0;
}

static u8 inc(u8 reg)
{
	reset_flag(ZFLAG);
	reset_flag(HFLAG);
	reset_flag(NFLAG);

	if ((reg & 0x0F) == 15)
		set_flag(HFLAG);

	reg++;
	if (reg == 0)
		set_flag(ZFLAG);

	return reg;
}

static u8 dec(u8 reg)
{
	reset_flag(HFLAG);
	reset_flag(ZFLAG);
	reset_flag(NFLAG);

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
	u16 res = val;

	reset_flag(HFLAG);
	reset_flag(ZFLAG);

	if (with_carry)
		res += get_flag(CFLAG);

	reset_flag(CFLAG);

	res += A;

	if ((res & 0xFF) == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);

	if ((A ^ val ^ res) & 0x10)
		set_flag(HFLAG);

	if (res > 255)
		set_flag(CFLAG);

	A = res;
}

static void add_HL(u16 val)
{
	int tmp = H;

	reset_flag(HFLAG);
	reset_flag(CFLAG);

	tmp <<= 8;
	tmp += L + val;
	if (tmp > 65535)
		set_flag(CFLAG);

	H = tmp >> 8;
	L = tmp;
	tmp = H + (val >> 8);
	tmp &= 0x0F;
	tmp <<= 8;
	tmp += L + (val & 0x00FF);
	if (tmp > 4095)
		set_flag(HFLAG);

	reset_flag(NFLAG);
	tick(1);
}

static void sub(u8 val, int with_carry)
{
	u8 res;

	reset_flag(ZFLAG);
	reset_flag(HFLAG);

	if (with_carry)
		val += get_flag(CFLAG);

	reset_flag(CFLAG);
	res = A - val;

	if (res == 0)
		set_flag(ZFLAG);

	set_flag(NFLAG);

	if ((A & 0xF) < (val & 0xF))
		set_flag(HFLAG);

	if (A < val)
		set_flag(CFLAG);

	A = res;
}

static void and(u8 val)
{
	reset_flag(ZFLAG);
	A &= val;

	if (A == 0)
		set_flag(ZFLAG);

	set_flag(HFLAG);
	reset_flag(NFLAG);
	reset_flag(CFLAG);
}

static void xor(u8 val)
{
	reset_flag(ZFLAG);
	A ^= val;

	if (A == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);
}

static void or(u8 val)
{
	reset_flag(ZFLAG);
	A |= val;

	if (A == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);
}

static void cmp(u8 val)
{
	reset_flag(ZFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);

	if (A == val)
		set_flag(ZFLAG);

	set_flag(NFLAG);

	if ((A & 0xF) < (val & 0xF))
		set_flag(HFLAG);

	if (A < val)
		set_flag(CFLAG);
}

static void rst(u8 offset)
{
	u8 low = PC & 0xFF;
	u8 high = PC >> 8;

	push_stack(low, high);

	PC = 0x0000 + offset;
}

static u8 sla(u8 reg)
{
	u8 res;

	reset_flag(ZFLAG);
	reset_flag(CFLAG);

	if (reg > 127)
		set_flag(CFLAG);

	res = reg << 1;

	if (res == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	return res;
}

static u8 srl(u8 reg)
{
	u8 res;

	if ((reg % 2) != 0)
		set_flag(CFLAG);
	else
		reset_flag(CFLAG);

	res = reg >> 1;

	if (res == 0)
		set_flag(ZFLAG);
	else
		reset_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);

	return res;
}

static u8 sra(u8 reg)
{
	u8 res = srl(reg);
	u8 bit = (res >> 6) & 1;

	res = (res & ~(1U << 7)) | (bit << 7);
	return res;
}

static u8 rl(u8 reg)
{
	u8 res = sla(reg);
	u8 cflag = get_flag(CFLAG);

	res = (res & ~1) | cflag;

	if (res == 0)
		set_flag(ZFLAG);
	else
		reset_flag(ZFLAG);
	tick(1);
	return res;
}

static u8 rr(u8 reg)
{
	u8 res = srl(reg);
	u8 cflag = get_flag(CFLAG);

	res = (res & ~(1U << 7)) | (cflag << 7);

	if (res == 0)
		set_flag(ZFLAG);
	else
		reset_flag(ZFLAG);
	tick(1);
	return res;
}

static u8 rlc(u8 reg)
{
	u8 cflag = get_flag(CFLAG);
	u8 res = sla(reg);

	res = (res & ~1) | cflag;
	tick(1);
	return res;
}

static u8 rrc(u8 reg)
{
	u8 cflag = get_flag(CFLAG);
	u8 res = srl(reg);

	res = (res & ~(1U << 7)) | cflag;

	if (res == 0)
		set_flag(ZFLAG);
	else
		reset_flag(ZFLAG);
	tick(1);
	return res;
}

static u8 swap(u8 reg)
{
	u8 res = 0;

	reset_flag(ZFLAG);

	if (reg == 0)
		set_flag(ZFLAG);
	else
		res = (reg >> 4) + (reg << 4);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
	reset_flag(CFLAG);

	return res;
}

static void check_bit(u8 reg, u8 bit)
{
	if ((reg >> bit) & 1)
		reset_flag(ZFLAG);
	else
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	set_flag(HFLAG);
}

void init_cpu(void)
{
	A = 0x01;
	F = 0xB0;
	B = 0x00;
	C = 0x13;
	D = 0x00;
	E = 0xD8;
	H = 0x01;
	L = 0x4D;
	SP = 0xFFFE;

	PC = 0x100;

	init_optable();
	init_cb_optable();
}

void cpu_debug_info(struct cpu_info *cpu)
{
	cpu->PC = &PC;
	cpu->SP = &SP;

	cpu->B = &B;
	cpu->C = &C;
	cpu->D = &D;
	cpu->E = &E;
	cpu->H = &H;
	cpu->L = &L;
	cpu->A = &A;
	cpu->F = &F;
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
	B = temp >> 8;
	C = temp;
}

/* LD BC,A */
static void op0x02(void)
{
	B = 0;
	C = A;
}

/* INC BC */
static void op0x03(void)
{
	C++;

	if (C == 0)
		B++;
	tick(1);
}

/* INC B */
static void op0x04(void)
{
	B = inc(B);
}

/* DEC B */
static void op0x05(void)
{
	B = dec(B);
}

/* LD B,n */
static void op0x06(void)
{
	B = fetch_8bit_data();
}

/* RLCA */
static void op0x07(void)
{
	reset_flag(CFLAG);
	if (A > 127)
		set_flag(CFLAG);

	A <<= 1;
	A += get_flag(CFLAG);

	if (A == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
}

/* LD nn,SP */
static void op0x08(void)
{
	u16 addr = fetch_16bit_data();
	cpu_write_mem(addr, SP & 0x00FF);
	cpu_write_mem(addr+1, SP >> 8);
}

/* ADD HL,BC */
static void op0x09(void)
{
	u16 tmp = B;
	tmp <<= 8;
	add_HL(tmp + C);
}

/* LD A,(BC) */
static void op0x0A(void)
{
	u16 tmp = B;
	tmp <<= 8;
	tmp += C;
	A = cpu_read_mem(tmp);
}

/* DEC BC */
static void op0x0B(void)
{
	if (C == 0)
		B--;

	C--;
	tick(1);
}

/* INC C */
static void op0x0C(void)
{
	C = inc(C);
}

/* DEC C */
static void op0x0D(void)
{
	C = dec(C);
}

/* LD C,n */
static void op0x0E(void)
{
	C = fetch_8bit_data();
}

/* RRCA */
static void op0x0F(void)
{
	if (A & 1)
		set_flag(CFLAG);

	A >>= 1;
	A += get_flag(CFLAG) * 128;

	if (A == 0)
		set_flag(ZFLAG);

	reset_flag(HFLAG);
	reset_flag(NFLAG);
}

/* STOP */
static void op0x10(void)
{
	stopped = 1;
}

/* LD DE,nn */
static void op0x11(void)
{
	u16 tmp = fetch_16bit_data();
	D = tmp >> 8;
	E = tmp;
}

/* LD DE,A */
static void op0x12(void)
{
	D = 0;
	E = A;
}

/* INC DE */
static void op0x13(void)
{
	E++;
	if (E == 0)
		D++;
	tick(1);
}

/* INC D */
static void op0x14(void)
{
	D = inc(D);
}

/* DEC D */
static void op0x15(void)
{
	D = dec(D);
}

/* LD D,n */
static void op0x16(void)
{
	D = fetch_8bit_data();
}

/* RLA */
static void op0x17(void)
{
	u8 tmp = get_flag(CFLAG);
	if (A > 127){
		set_flag(CFLAG);
	}

	A <<= 1;
	A += tmp;

	if (A == 0)
		set_flag(ZFLAG);

	reset_flag(NFLAG);
	reset_flag(HFLAG);
}

/* JR n */
static void op0x18(void)
{
	char tmp;
	tmp = (char) fetch_8bit_data();
	PC += tmp;
	tick(1);
}

/* ADD HL,DE */
static void op0x19(void)
{
	u16 tmp = D;
	tmp <<= 8;
	add_HL(tmp + E);
}

/* LD A,(DE) */
static void op0x1A(void)
{
	u16 tmp = D;
	tmp <<= 8;
	tmp += E;
	A = cpu_read_mem(tmp);
}

/* DEC DE */
static void op0x1B(void)
{
	if (E == 0)
		D--;

	E--;
	tick(1);
}

/* INC E */
static void op0x1C(void)
{
	E = inc(E);
}

/* DEC E */
static void op0x1D(void)
{
	E = dec(E);
}

/* LD E,n */
static void op0x1E(void)
{
	E = fetch_8bit_data();
}

/* RRA */
static void op0x1F(void)
{
	A = rr(A);
	reset_flag(ZFLAG);
	reset_flag(HFLAG);
	reset_flag(NFLAG);
}

/* JR NZ,n */
static void op0x20(void)
{
	char tmp = (char) fetch_8bit_data();
	if (!get_flag(ZFLAG)) {
		PC += tmp;
		tick(1);
	}
}

/* LD HL,nn */
static void op0x21(void)
{
	u16 tmp = fetch_16bit_data();
	H = tmp >> 8;
	L = tmp;
}

/* LDI (HL),A */
static void op0x22(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, A);
	L++;
	if (L == 0)
		H++;
}

/* INC HL */
static void op0x23(void)
{
	L++;
	if (L == 0)
		H++;
	tick(1);
}

/* INC H */
static void op0x24(void)
{
	H = inc(H);
}

/* DEC H */
static void op0x25(void)
{
	H = dec(H);
}

/* LD H,n */
static void op0x26(void)
{
	H = fetch_8bit_data();
}

/* DAA */
static void op0x27(void)
{

}

/* JR Z,n */
static void op0x28(void)
{
	char tmp = (char) fetch_8bit_data();
	if (get_flag(ZFLAG)) {
		PC += tmp;
		tick(1);
	}
}

/* ADD HL,HL */
static void op0x29(void)
{
	u16 tmp = H;
	tmp <<= 8;
	add_HL(tmp + L);
}

/* LDI A,(HL) */
static void op0x2A(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	A = cpu_read_mem(tmp);
	op0x23();
}

/* DEC HL */
static void op0x2B(void)
{
	if (L == 0)
		H--;

	L--;
	tick(1);
}

/* INC L */
static void op0x2C(void)
{
	L = inc(L);
}

/* DEC L */
static void op0x2D(void)
{
	L = dec(L);
}

/* LD L,n */
static void op0x2E(void)
{
	L = fetch_8bit_data();
}

/* CPL */
static void op0x2F(void)
{
	A = ~A;
	set_flag(NFLAG);
	set_flag(HFLAG);
}

/* JR NC,n */
static void op0x30(void)
{
	char tmp = (char) fetch_8bit_data();
	if (!get_flag(CFLAG)) {
		PC += tmp;
		tick(1);
	}
}

/* LD SP,nn */
static void op0x31(void)
{
	SP = fetch_16bit_data();
}

/* LDD (HL),A */
static void op0x32(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, A);

	if (L == 0)
		H--;

	L--;
}

/* INC SP */
static void op0x33(void)
{
	SP++;
	tick(1);
}

/* INC (HL) */
static void op0x34(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, inc(cpu_read_mem(tmp)));
}

/* DEC (HL) */
static void op0x35(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;

	cpu_write_mem(tmp, dec(cpu_read_mem(tmp)));

}

/* LD (HL),n */
static void op0x36(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, fetch_8bit_data());
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
	char tmp = (char) fetch_8bit_data();
	if (get_flag(CFLAG)) {
		PC += tmp;
		tick(1);
	}
}

/* ADD HL,SP */
static void op0x39(void)
{
	add_HL(SP);
}

/* LDD A,(HL) */
static void op0x3A(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	A = cpu_read_mem(tmp);
	op0x2B();
}

/* DEC SP */
static void op0x3B(void)
{
	SP--;
	tick(1);
}

/* INC A */
static void op0x3C(void)
{
	A = inc(A);
}

/* DEC A */
static void op0x3D(void)
{
	A = dec(A);
}

/* LD A,n */
static void op0x3E(void)
{
	A = fetch_8bit_data();
}

/* CCF */
static void op0x3F(void)
{
	reset_flag(NFLAG);
	reset_flag(HFLAG);
	if (get_flag(CFLAG))
		reset_flag(CFLAG);
	else
		set_flag(CFLAG);
}

/* LD B,B */
static void op0x40(void)
{
	B = B;
}

/* LD B,C */
static void op0x41(void)
{
	B = C;
}

/* LD B,D */
static void op0x42(void)
{
	B = D;
}

/* LD B,E */
static void op0x43(void)
{
	B = E;
}

/* LD B,H */
static void op0x44(void)
{
	B = H;
}

/* LD B,L */
static void op0x45(void)
{
	B = L;
}

/* LD B,(HL)*/
static void op0x46(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	B = cpu_read_mem(tmp);
}

/* LD B,A */
static void op0x47(void)
{
	B = A;
}

/* LD C,B */
static void op0x48(void)
{
	C = B;
}

/* LD C,C */
static void op0x49(void)
{
	C = C;
}

/* LD C,D */
static void op0x4A(void)
{
	C = D;
}

/* LD C,E */
static void op0x4B(void)
{
	C = E;
}

/* LD C,H */
static void op0x4C(void)
{
	C = H;
}

/* LD C,L */
static void op0x4D(void)
{
	C = L;
}

/* LD C,(HL) */
static void op0x4E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	C = cpu_read_mem(tmp);
}

/* LD C,A */
static void op0x4F(void)
{
	C = A;
}

/* LD D,B */
static void op0x50(void)
{
	D = B;
}

/* LD D,C */
static void op0x51(void)
{
	D = C;
}

/* LD D,D */
static void op0x52(void)
{
	D = D;
}

/* LD D,E */
static void op0x53(void)
{
	D = E;
}

/* LD D,H */
static void op0x54(void)
{
	D = H;
}

/* LD D,L */
static void op0x55(void)
{
	D = L;
}

/* LD D,(HL) */
static void op0x56(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	D = cpu_read_mem(tmp);
}

/* LD D,A */
static void op0x57(void)
{
	D = A;
}

/* LD E,B */
static void op0x58(void)
{
	E = B;
}

/* LD E,C */
static void op0x59(void)
{
	E = C;
}

/* LD E,D */
static void op0x5A(void)
{
	E = D;
}

/* LD E,E */
static void op0x5B(void)
{
	E = E;
}

/* LD E,H */
static void op0x5C(void)
{
	E = H;
}

/* LD E,L */
static void op0x5D(void)
{
	E = L;
}

/* LD E,(HL) */
static void op0x5E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	E = cpu_read_mem(tmp);
}

/* LD E,A */
static void op0x5F(void)
{
	E = A;
}

/* LD H,B */
static void op0x60(void)
{
	H = B;
}

/* LD H,C */
static void op0x61(void)
{
	H = C;
}

/* LD H,D */
static void op0x62(void)
{
	H = D;
}

/* LD H,E */
static void op0x63(void)
{
	H = E;
}

/* LD H,H */
static void op0x64(void)
{
	H = H;
}

/* LD H,L */
static void op0x65(void)
{
	H = L;
}

/* LD H,(HL) */
static void op0x66(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	H = cpu_read_mem(tmp);
}

/* LD H,A */
static void op0x67(void)
{
	H = A;
}

/* LD L,B */
static void op0x68(void)
{
	L = B;
}

/* LD L,C */
static void op0x69(void)
{
	L = C;
}

/* LD L,D */
static void op0x6A(void)
{
	L = D;
}

/* LD L,E */
static void op0x6B(void)
{
	L = E;
}

/* LD L,H */
static void op0x6C(void)
{
	L = H;
}

/* LD L,L */
static void op0x6D(void)
{
	L = L;
}

/* LD L,(HL) */
static void op0x6E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	L = cpu_read_mem(tmp);
}

/* LD L,A */
static void op0x6F(void)
{
	L = A;
}

/* LD (HL),B */
static void op0x70(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, B);
}

/* LD (HL),C */
static void op0x71(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, C);
}

/* LD (HL),D */
static void op0x72(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, D);
}

/* LD (HL),E */
static void op0x73(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, E);
}

/* LD (HL),H */
static void op0x74(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, H);
}

/* LD (HL),L */
static void op0x75(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, L);
}

/* HALT */
static void op0x76(void)
{

}

/* LD (HL),A */
static void op0x77(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	cpu_write_mem(tmp, A);
}

/* LD A,B */
static void op0x78(void)
{
	A = B;
}

/* LD A,C */
static void op0x79(void)
{
	A = C;
}

/* LD A,D */
static void op0x7A(void)
{
	A = D;
}

/* LD A,E */
static void op0x7B(void)
{
	A = E;
}

/* LD A,H */
static void op0x7C(void)
{
	A = H;
}

/* LD A,L */
static void op0x7D(void)
{
	A = L;
}

/* LD A,(HL) */
static void op0x7E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	A = cpu_read_mem(tmp);
}

/* LD A,A */
static void op0x7F(void)
{
	A = A;
}

/* ADD A,B */
static void op0x80(void)
{
	add(B, 0);
}

/* ADD A,C */
static void op0x81(void)
{
	add(C, 0);
}

/* ADD A,D */
static void op0x82(void)
{
	add(D, 0);
}

/* ADD A,E */
static void op0x83(void)
{
	add(E, 0);
}

/* ADD A,H */
static void op0x84(void)
{
	add(H, 0);
}

/* ADD A,L */
static void op0x85(void)
{
	add(L, 0);
}

/* ADD A,(HL) */
static void op0x86(void)
{
	u16 tmp = (H << 8) + L;
	add(cpu_read_mem(tmp), 0);
}

/* ADD A,A */
static void op0x87(void)
{
	add(A, 0);
}

/* ADC A,B */
static void op0x88(void)
{
	add(B, 1);
}

/* ADC A,C */
static void op0x89(void)
{
	add(C, 1);
}

/* ADC A,D */
static void op0x8A(void)
{
	add(D, 1);
}

/* ADC A,E */
static void op0x8B(void)
{
	add(E, 1);
}

/* ADC A,H */
static void op0x8C(void)
{
	add(H, 1);
}

/* ADC A,L */
static void op0x8D(void)
{
	add(L, 1);
}

/* ADC A,(HL) */
static void op0x8E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	add(cpu_read_mem(tmp), 1);
}

/* ADC A,A */
static void op0x8F(void)
{
	add(A, 1);
}

/* SUB A,B */
static void op0x90(void)
{
	sub(B, 0);
}

/* SUB A,C */
static void op0x91(void)
{
	sub(C, 0);
}

/* SUB A,D */
static void op0x92(void)
{
	sub(D, 0);
}

/* SUB A,E */
static void op0x93(void)
{
	sub(E, 0);
}

/* SUB A,H */
static void op0x94(void)
{
	sub(H, 0);
}

/* SUB A,L */
static void op0x95(void)
{
	sub(L, 0);
}

/* SUB A,(HL) */
static void op0x96(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	sub(cpu_read_mem(tmp), 0);
}

/* SUB A,A */
static void op0x97(void)
{
	sub(A, 0);
}

/* SBC A,B */
static void op0x98(void)
{
	sub(B, 1);
}

/* SBC A,C */
static void op0x99(void)
{
	sub(C, 1);
}

/* SBC A,D */
static void op0x9A(void)
{
	sub(D, 1);
}

/* SBC A,E */
static void op0x9B(void)
{
	sub(E, 1);
}

/* SBC A,H */
static void op0x9C(void)
{
	sub(H, 1);
}

/* SBC A,L */
static void op0x9D(void)
{
	sub(L, 1);
}

/* SBC A,(HL) */
static void op0x9E(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	sub(cpu_read_mem(tmp), 1);
}

/* SBC A,A */
static void op0x9F(void)
{
	sub(A, 1);
}

/* AND A,B */
static void op0xA0(void)
{
	and(B);
}

/* AND A,C */
static void op0xA1(void)
{
	and(C);
}

/* AND A,D */
static void op0xA2(void)
{
	and(D);
}

/* AND A,E */
static void op0xA3(void)
{
	and(E);
}

/* AND A,H */
static void op0xA4(void)
{
	and(H);
}

/* AND A,L */
static void op0xA5(void)
{
	and(L);
}

/* AND A,(HL)*/
static void op0xA6(void)
{
	u16 tmp = (H << 8) + L;
	and(cpu_read_mem(tmp));
}

/* AND A,A */
static void op0xA7(void)
{
	and(A);
}

/* XOR A,B */
static void op0xA8(void)
{
	xor(B);
}

/* XOR A,C */
static void op0xA9(void)
{
	xor(C);
}

/* XOR A,D */
static void op0xAA(void)
{
	xor(D);
}

/* XOR A,E */
static void op0xAB(void)
{
	xor(E);
}

/* XOR A,H */
static void op0xAC(void)
{
	xor(H);
}

/* XOR A,L */
static void op0xAD(void)
{
	xor(L);
}

/* XOR A,(HL) */
static void op0xAE(void)
{
	u16 tmp = (H << 8) + L;
	xor(cpu_read_mem(tmp));
}

/* XOR A,A */
static void op0xAF(void)
{
	xor(A);
}

/* OR A,B  */
static void op0xB0(void)
{
	or(B);
}

/* OR A,C */
static void op0xB1(void)
{
	or(C);
}

/* OR A,D */
static void op0xB2(void)
{
	or(D);
}

/* OR A,E */
static void op0xB3(void)
{
	or(E);
}

/* OR A,H */
static void op0xB4(void)
{
	or(H);
}

/* OR A,L */
static void op0xB5(void)
{
	or(L);
}

/* OR A,(HL) */
static void op0xB6(void)
{
	u8 tmp = (H << 8) + L;
	or(cpu_read_mem(tmp));
}

/* OR A,A */
static void op0xB7(void)
{
	or(A);
}

/* CP A,B */
static void op0xB8(void)
{
	cmp(B);
}

/* CP A,C */
static void op0xB9(void)
{
	cmp(C);
}

/* CP A,D */
static void op0xBA(void)
{
	cmp(D);
}

/* CP A,E */
static void op0xBB(void)
{
	cmp(E);
}

/* CP A,H */
static void op0xBC(void)
{
	cmp(H);
}

/* CP A,L */
static void op0xBD(void)
{
	cmp(L);
}

/* CP A,(HL) */
static void op0xBE(void)
{
	u16 tmp = (H << 8) + L;
	cmp(cpu_read_mem(tmp));
}

/* CP A,A */
static void op0xBF(void)
{
	cmp(A);
}

/* RET NZ */
static void op0xC0(void)
{
	if (!get_flag(ZFLAG)) {
		PC = pop_stack();
		tick(2);
	}
	tick(1);
}

/* POP BC*/
static void op0xC1(void)
{
	u16 tmp = pop_stack();

	B = tmp >> 8;
	C = tmp;
}

/* JP NZ,nn */
static void op0xC2(void)
{
	u16 address = fetch_16bit_data();
	if (!get_flag(ZFLAG)) {
		PC = address;
		tick(1);
	}
}

/* JP nn */
static void op0xC3(void)
{
	u16 address = fetch_16bit_data();
	PC = address;
	tick(1);
}

/* CALL NZ,nn */
static void op0xC4(void)
{
	u8 low;
	u8 high;
	u16 address = fetch_16bit_data();

	if (!get_flag(ZFLAG)) {
		low = PC;
		high = PC >> 8;
		push_stack(low, high);
		PC = address;
	}
}

/* PUSH BC */
static void op0xC5(void)
{
	push_stack(C, B);
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
	if (get_flag(ZFLAG)) {
		PC = pop_stack();
		tick(2);
	}
	tick(1);
}

/* RET */
static void op0xC9(void)
{
	PC = pop_stack();
	tick(1);
}

/* JP Z,nn */
static void op0xCA(void)
{
	u16 address = fetch_16bit_data();

	if (get_flag(ZFLAG)) {
		PC = address;
		tick(1);
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
	u8 low;
	u8 high;
	u16 address = fetch_16bit_data();

	if (get_flag(ZFLAG)) {
		low = PC;
		high = PC >> 8;
		push_stack(low, high);
		PC = address;
	}
}

/* CALL nn */
static void op0xCD(void)
{
	u16 address = fetch_16bit_data();
	u8 low = PC;
	u8 high = PC >> 8;

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
	if (!get_flag(CFLAG)) {
		PC = pop_stack();
		tick(2);
	}
	tick(1);
}

/* POP DE */
static void op0xD1(void)
{
	u16 tmp = pop_stack();

	D = tmp >> 8;
	E = tmp;
}

/* JP NC,nn */
static void op0xD2(void)
{
	u16 address = fetch_16bit_data();

	if (!get_flag(CFLAG)) {
		PC = address;
		tick(1);
	}
}

/* N/A */
static void op0xD3(void)
{

}

/* CALL NC,nn */
static void op0xD4(void)
{
	u8 low;
	u8 high;
	u16 address = fetch_16bit_data();

	if (!get_flag(CFLAG)) {
		low = PC;
		high = PC >> 8;
		push_stack(low, high);
		PC = address;
	}
}

/* PUSH DE */
static void op0xD5(void)
{
	u8 low = E;
	u8 high = D;

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
	if (get_flag(CFLAG)) {
		PC = pop_stack();
		tick(2);
	}
	tick(1);
}

/* RETI */
static void op0xD9(void)
{
	PC = pop_stack();
	set_ime(1);
	tick(1);
}

/* JP C,nn */
static void op0xDA(void)
{
	u16 address = fetch_16bit_data();

	if (get_flag(CFLAG)) {
		PC = address;
		tick(1);
	}
}

/* N/A */
static void op0xDB(void)
{

}

/* CALL C,nn */
static void op0xDC(void)
{
	u8 low;
	u8 high;
	u16 address = fetch_16bit_data();

	if (get_flag(CFLAG)) {
		low = PC;
		high = PC >> 8;
		push_stack(low, high);
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

	cpu_write_mem(address, A);
}

/* POP HL */
static void op0xE1(void)
{
	u16 tmp = pop_stack();

	H = tmp >> 8;
	L = tmp;
}

/* LD (C),A */
static void op0xE2(void)
{
	u16 address = 0xFF00 + C;
	cpu_write_mem(address, A);
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
	push_stack(L, H);
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
	char tmp = (char) fetch_8bit_data();
	int res = tmp + SP;

	reset_flag(ZFLAG);
	reset_flag(NFLAG);

	if ((tmp ^ SP ^ res) & 0x1000)
		set_flag(HFLAG);

	if (res > 0xFFFF)
		set_flag(CFLAG);

	SP = res;
	tick(2);
}

/* JP HL */
static void op0xE9(void)
{
	PC = (H << 8) + L;
}

/* LD (nn),A */
static void op0xEA(void)
{
	u16 address = fetch_16bit_data();
	cpu_write_mem(address, A);
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

	A = cpu_read_mem(0xFF00 + tmp);
}

/* POP AF */
static void op0xF1(void)
{
	u16 tmp = pop_stack();

	A = tmp >> 8;
	F = tmp & 0xFF;
}

/* LD A,(C) */
static void op0xF2(void)
{
	u8 tmp = cpu_read_mem(0xFF00 + C);

	A = tmp;
}

/* DI */
static void op0xF3(void)
{
	set_ime(0);
	if (ime_scheduled)
		ime_scheduled = 0;
}

/* N/A */
static void op0xF4(void)
{

}

/* PUSH AF */
static void op0xF5(void)
{
	push_stack(F, A);
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
	char value = (char) fetch_8bit_data();
	int result;
	result = SP + value;
	if (result > 0xFFFF)
		set_flag(CFLAG);
	else
		reset_flag(CFLAG);

	L = result & 0x00FF;
	H = result >> 8;
	result = 0;
	result = (SP & 0x0FFF) + (value & 0x0FFF);
	if (result > 0x0FFF)
		set_flag(HFLAG);
	else
		reset_flag(HFLAG);

	reset_flag(ZFLAG);
	reset_flag(NFLAG);
	tick(1);
}

/* LD SP,HL */
static void op0xF9(void)
{
	u16 tmp = H;
	tmp <<= 8;
	tmp += L;
	SP = tmp;
	tick(1);
}

/* LD A,(nn) */
static void op0xFA(void)
{
	u16 address = fetch_16bit_data();

	A = cpu_read_mem(address);
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

/* RLC B */
static void CB_op0x00(void)
{
	B = rlc(B);
}

/* RLC C */
static void CB_op0x01(void)
{
	C = rlc(C);
}

/* RLC D */
static void CB_op0x02(void)
{
	D = rlc(D);
}

/* RLC E */
static void CB_op0x03(void)
{
	E = rlc(E);
}

/* RLC H */
static void CB_op0x04(void)
{
	H = rlc(H);
}

/* RLC L */
static void CB_op0x05(void)
{
	L = rlc(L);
}

/* RLC (HL) */
static void CB_op0x06(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = rlc(reg);
	cpu_write_mem(address, reg);
}

/* RLC A */
static void CB_op0x07(void)
{
	A = rlc(A);
}

/* RRC B */
static void CB_op0x08(void)
{
	B = rrc(B);
}

/* RRC C */
static void CB_op0x09(void)
{
	C = rrc(C);
}

/* RRC D */
static void CB_op0x0A(void)
{
	D = rrc(D);
}

/* RRC E */
static void CB_op0x0B(void)
{
	E = rrc(D);
}

/* RRC H */
static void CB_op0x0C(void)
{
	H = rrc(H);
}

/* RRC L */
static void CB_op0x0D(void)
{
	L = rrc(L);
}

/* RRC (HL) */
static void CB_op0x0E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = rrc(reg);
	cpu_write_mem(address, reg);
}

/* RRC A */
static void CB_op0x0F(void)
{
	A = rrc(A);
}

/* RL B */
static void CB_op0x10(void)
{
	B = rl(B);
}

/* RL C */
static void CB_op0x11(void)
{
	C = rl(C);
}

/* RL D */
static void CB_op0x12(void)
{
	D = rl(D);
}

/* RL E */
static void CB_op0x13(void)
{
	E = rl(E);
}

/* RL H */
static void CB_op0x14(void)
{
	H = rl(H);
}

/* RL L */
static void CB_op0x15(void)
{
	L = rl(L);
}

/* RL (HL) */
static void CB_op0x16(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = rl(reg);
	cpu_write_mem(address, reg);
}

/* RL A */
static void CB_op0x17(void)
{
	A = rl(A);
}

/* RR B */
static void CB_op0x18(void)
{
	B = rr(B);
}

/* RR C */
static void CB_op0x19(void)
{
	C = rr(C);
}

/* RR D */
static void CB_op0x1A(void)
{
	D = rr(D);
}

/* RR E */
static void CB_op0x1B(void)
{
	E = rr(E);
}

/* RR H */
static void CB_op0x1C(void)
{
	H = rr(H);
}

/* RR L */
static void CB_op0x1D(void)
{
	L = rr(L);
}

/* RR (HL) */
static void CB_op0x1E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = rr(reg);
	cpu_write_mem(address, reg);
}

/* RR A */
static void CB_op0x1F(void)
{
	A = rr(A);
}

/* SLA B */
static void CB_op0x20(void)
{
	B = sla(B);
}

/* SLA C */
static void CB_op0x21(void)
{
	C = sla(C);
}

/* SLA D */
static void CB_op0x22(void)
{
	D = sla(D);
}

/* SLA E */
static void CB_op0x23(void)
{
	E = sla(E);
}

/* SLA H */
static void CB_op0x24(void)
{
	H = sla(H);
}

/* SLA L */
static void CB_op0x25(void)
{
	L = sla(L);
}

/* SLA (HL) */
static void CB_op0x26(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = sla(reg);
	cpu_write_mem(address, reg);
}

/* SLA A */
static void CB_op0x27(void)
{
	A = sla(A);
}

/* SRA B */
static void CB_op0x28(void)
{
	B = sra(B);
}

/* SRA C */
static void CB_op0x29(void)
{
	C = sra(C);
}

/* SRA D */
static void CB_op0x2A(void)
{
	D = sra(D);
}

/* SRA E */
static void CB_op0x2B(void)
{
	E = sra(E);
}

/* SRA H */
static void CB_op0x2C(void)
{
	H = sra(H);
}

/* SRA L */
static void CB_op0x2D(void)
{
	L = sra(L);
}

/* SRA (HL) */
static void CB_op0x2E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = sra(reg);
	cpu_write_mem(address, reg);
}

/* SRA A */
static void CB_op0x2F(void)
{
	A = sra(A);
}

/* SWAP B */
static void CB_op0x30(void)
{
	B = swap(B);
}

/* SWAP C */
static void CB_op0x31(void)
{
	C = swap(C);
}

/* SWAP D */
static void CB_op0x32(void)
{
	D = swap(D);
}

/* SWAP E */
static void CB_op0x33(void)
{
	E = swap(E);
}

/* SWAP H */
static void CB_op0x34(void)
{
	H = swap(H);
}

/* SWAP L */
static void CB_op0x35(void)
{
	L = swap(L);
}

/* SWAP (HL) */
static void CB_op0x36(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = swap(reg);
	cpu_write_mem(address, reg);
}

/* SWAP A */
static void CB_op0x37(void)
{
	A = swap(A);
}

/* SRL B */
static void CB_op0x38(void)
{
	B = srl(B);
}

/* SRL C */
static void CB_op0x39(void)
{
	C = srl(C);
}

/* SRL D */
static void CB_op0x3A(void)
{
	D = srl(D);
}

/* SRL E */
static void CB_op0x3B(void)
{
	E = srl(E);
}

/* SRL H */
static void CB_op0x3C(void)
{
	H = srl(H);
}

/* SRL L */
static void CB_op0x3D(void)
{
	L = srl(L);
}

/* SRL (HL) */
static void CB_op0x3E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);

	reg = srl(reg);
	cpu_write_mem(address, reg);
}

/* SRL A */
static void CB_op0x3F(void)
{
	A = srl(A);
}

/* BIT 0,B */
static void CB_op0x40(void)
{
	check_bit(B, 0);
}

/* BIT 0,C */
static void CB_op0x41(void)
{
	check_bit(C, 0);
}

/* BIT 0,D */
static void CB_op0x42(void)
{
	check_bit(D, 0);
}

/* BIT 0,E */
static void CB_op0x43(void)
{
	check_bit(E, 0);
}

/* BIT 0,H */
static void CB_op0x44(void)
{
	check_bit(H, 0);
}

/* BIT 0,L */
static void CB_op0x45(void)
{
	check_bit(L, 0);
}

/* BIT 0,(HL) */
static void CB_op0x46(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 0);
}

/* BIT 0,A */
static void CB_op0x47(void)
{
	check_bit(A, 0);
}

/* BIT 1,B */
static void CB_op0x48(void)
{
	check_bit(B, 1);
}

/* BIT 1,C */
static void CB_op0x49(void)
{
	check_bit(C, 1);
}

/* BIT 1,D */
static void CB_op0x4A(void)
{
	check_bit(D, 1);
}

/* BIT 1,E */
static void CB_op0x4B(void)
{
	check_bit(E, 1);
}

/* BIT 1,H */
static void CB_op0x4C(void)
{
	check_bit(H, 1);
}

/* BIT 1,L */
static void CB_op0x4D(void)
{
	check_bit(L, 1);
}

/* BIT 1,(HL) */
static void CB_op0x4E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 1);
}

/* BIT 1,A */
static void CB_op0x4F(void)
{
	check_bit(A, 1);
}

/* BIT 2,B */
static void CB_op0x50(void)
{
	check_bit(B, 2);
}

/* BIT 2,C */
static void CB_op0x51(void)
{
	check_bit(C, 2);
}

/* BIT 2,D */
static void CB_op0x52(void)
{
	check_bit(D, 2);
}

/* BIT 2,E */
static void CB_op0x53(void)
{
	check_bit(E, 2);
}

/* BIT 2,H */
static void CB_op0x54(void)
{
	check_bit(H, 2);
}

/* BIT 2,L */
static void CB_op0x55(void)
{
	check_bit(L, 2);
}

/* BIT 2,(HL) */
static void CB_op0x56(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 2);
}

/* BIT 2,A */
static void CB_op0x57(void)
{
	check_bit(A, 2);
}

/* BIT 3,B */
static void CB_op0x58(void)
{
	check_bit(B, 3);
}

/* BIT 3,C */
static void CB_op0x59(void)
{
	check_bit(C, 3);
}

/* BIT 3,D */
static void CB_op0x5A(void)
{
	check_bit(D, 3);
}

/* BIT 3,E */
static void CB_op0x5B(void)
{
	check_bit(E, 3);
}

/* BIT 3,H */
static void CB_op0x5C(void)
{
	check_bit(H, 3);
}

/* BIT 3,L */
static void CB_op0x5D(void)
{
	check_bit(L, 3);
}

/* BIT 3,(HL) */
static void CB_op0x5E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 3);
}

/* BIT 3,A */
static void CB_op0x5F(void)
{
	check_bit(A, 3);
}

/* BIT 4,B */
static void CB_op0x60(void)
{
	check_bit(B, 4);
}

/* BIT 4,C */
static void CB_op0x61(void)
{
	check_bit(C, 4);
}

/* BIT 4,D */
static void CB_op0x62(void)
{
	check_bit(D, 4);
}

/* BIT 4,E */
static void CB_op0x63(void)
{
	check_bit(E, 4);
}

/* BIT 4,H */
static void CB_op0x64(void)
{
	check_bit(H, 4);
}

/* BIT 4,L */
static void CB_op0x65(void)
{
	check_bit(L, 4);
}

/* BIT 4,(HL) */
static void CB_op0x66(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 4);
}

/* BIT 4,A */
static void CB_op0x67(void)
{
	check_bit(A, 4);
}

/* BIT 5,B */
static void CB_op0x68(void)
{
	check_bit(B, 5);
}

/* BIT 5,C */
static void CB_op0x69(void)
{
	check_bit(C, 5);
}

/* BIT 5,D */
static void CB_op0x6A(void)
{
	check_bit(D, 5);
}

/* BIT 5,E */
static void CB_op0x6B(void)
{
	check_bit(E, 5);
}

/* BIT 5,H */
static void CB_op0x6C(void)
{
	check_bit(H, 5);
}

/* BIT 5,L */
static void CB_op0x6D(void)
{
	check_bit(L, 5);
}

/* BIT 5,(HL) */
static void CB_op0x6E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 5);
}

/* BIT 5,A */
static void CB_op0x6F(void)
{
	check_bit(A, 5);
}

/* BIT 6,B */
static void CB_op0x70(void)
{
	check_bit(B, 6);
}

/* BIT 6,C */
static void CB_op0x71(void)
{
	check_bit(C, 6);
}

/* BIT 6,D */
static void CB_op0x72(void)
{
	check_bit(D, 6);
}

/* BIT 6,E */
static void CB_op0x73(void)
{
	check_bit(E, 6);
}

/* BIT 6,H */
static void CB_op0x74(void)
{
	check_bit(H, 6);
}

/* BIT 6,L */
static void CB_op0x75(void)
{
	check_bit(L, 6);
}

/* BIT 6,(HL) */
static void CB_op0x76(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 6);
}

/* BIT 6,A */
static void CB_op0x77(void)
{
	check_bit(A, 6);
}

/* BIT 7,B */
static void CB_op0x78(void)
{
	check_bit(B, 7);
}

/* BIT 7,C */
static void CB_op0x79(void)
{
	check_bit(C, 7);
}

/* BIT 7,D */
static void CB_op0x7A(void)
{
	check_bit(D, 7);
}

/* BIT 7,E */
static void CB_op0x7B(void)
{
	check_bit(E, 7);
}

/* BIT 7,H */
static void CB_op0x7C(void)
{
	check_bit(H, 7);
}

/* BIT 7,L */
static void CB_op0x7D(void)
{
	check_bit(L, 7);
}

/* BIT 7,(HL) */
static void CB_op0x7E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	check_bit(reg, 7);
}

/* BIT 7,A */
static void CB_op0x7F(void)
{
	check_bit(A, 7);
}

/* RES 0,B */
static void CB_op0x80(void)
{
	B = reset_bit(B, 0);
}

/* RES 0,C */
static void CB_op0x81(void)
{
	C = reset_bit(C, 0);
}

/* RES 0,D */
static void CB_op0x82(void)
{
	D = reset_bit(D, 0);
}

/* RES 0,E */
static void CB_op0x83(void)
{
	E = reset_bit(E, 0);
}

/* RES 0,H */
static void CB_op0x84(void)
{
	H = reset_bit(H, 0);
}

/* RES 0,L */
static void CB_op0x85(void)
{
	L = reset_bit(L, 0);
}

/* RES 0,(HL) */
static void CB_op0x86(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 0);
	cpu_write_mem(address, reg);
}

/* RES 0,A */
static void CB_op0x87(void)
{
	A = reset_bit(A, 0);
}

/* RES 1,B */
static void CB_op0x88(void)
{
	B = reset_bit(B, 1);
}

/* RES 1,C */
static void CB_op0x89(void)
{
	C = reset_bit(C, 1);
}

/* RES 1,D */
static void CB_op0x8A(void)
{
	D = reset_bit(D, 1);
}

/* RES 1,E */
static void CB_op0x8B(void)
{
	E = reset_bit(E, 1);
}

/* RES 1,H */
static void CB_op0x8C(void)
{
	H = reset_bit(H, 1);
}

/* RES 1,L */
static void CB_op0x8D(void)
{
	L = reset_bit(L, 1);
}

/* RES 1,(HL) */
static void CB_op0x8E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 1);
	cpu_write_mem(address, reg);
}

/* RES 1,A */
static void CB_op0x8F(void)
{
	A = reset_bit(A, 1);
}

/* RES 2,B */
static void CB_op0x90(void)
{
	B = reset_bit(B, 2);
}

/* RES 2,C */
static void CB_op0x91(void)
{
	C = reset_bit(C, 2);
}

/* RES 2,D */
static void CB_op0x92(void)
{
	D = reset_bit(D, 2);
}

/* RES 2,E */
static void CB_op0x93(void)
{
	E = reset_bit(E, 2);
}

/* RES 2,H */
static void CB_op0x94(void)
{
	H = reset_bit(H, 2);
}

/* RES 2,L */
static void CB_op0x95(void)
{
	L = reset_bit(L, 2);
}

/* RES 2,(HL) */
static void CB_op0x96(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 2);
	cpu_write_mem(address, reg);
}

/* RES 2,A */
static void CB_op0x97(void)
{
	A = reset_bit(A, 2);
}

/* RES 3,B */
static void CB_op0x98(void)
{
	B = reset_bit(B, 3);
}

/* RES 3,C */
static void CB_op0x99(void)
{
	C = reset_bit(C, 3);
}

/* RES 3,D */
static void CB_op0x9A(void)
{
	D = reset_bit(D, 3);
}

/* RES 3,E */
static void CB_op0x9B(void)
{
	E = reset_bit(E, 3);
}

/* RES 3,H */
static void CB_op0x9C(void)
{
	H = reset_bit(H, 3);
}

/* RES 3,L */
static void CB_op0x9D(void)
{
	L = reset_bit(L, 3);
}

/* RES 3,(HL) */
static void CB_op0x9E(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 3);
	cpu_write_mem(address, reg);
}

/* RES 3,A */
static void CB_op0x9F(void)
{
	A = reset_bit(A, 3);
}

/* RES 4,B */
static void CB_op0xA0(void)
{
	B = reset_bit(B, 4);
}

/* RES 4,C */
static void CB_op0xA1(void)
{
	C = reset_bit(C, 4);
}

/* RES 4,D */
static void CB_op0xA2(void)
{
	D = reset_bit(D, 4);
}

/* RES 4,E */
static void CB_op0xA3(void)
{
	E = reset_bit(E, 4);
}

/* RES 4,H */
static void CB_op0xA4(void)
{
	H = reset_bit(H, 4);
}

/* RES 4,L */
static void CB_op0xA5(void)
{
	L = reset_bit(L, 4);
}

/* RES 4,(HL) */
static void CB_op0xA6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 4);
	cpu_write_mem(address, reg);
}

/* RES 4,A */
static void CB_op0xA7(void)
{
	A = reset_bit(A, 4);
}

/* RES 5,B */
static void CB_op0xA8(void)
{
	B = reset_bit(B, 5);
}

/* RES 5,C */
static void CB_op0xA9(void)
{
	C = reset_bit(C, 5);
}

/* RES 5,D */
static void CB_op0xAA(void)
{
	D = reset_bit(D, 5);
}

/* RES 5,E */
static void CB_op0xAB(void)
{
	E = reset_bit(E, 5);
}

/* RES 5,H */
static void CB_op0xAC(void)
{
	H = reset_bit(H, 5);
}

/* RES 5,L */
static void CB_op0xAD(void)
{
	L = reset_bit(L, 5);
}

/* RES 5,(HL) */
static void CB_op0xAE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 5);
	cpu_write_mem(address, reg);
}

/* RES 5,A */
static void CB_op0xAF(void)
{
	A = reset_bit(A, 5);
}

/* RES 6,B */
static void CB_op0xB0(void)
{
	B = reset_bit(B, 6);
}

/* RES 6,C */
static void CB_op0xB1(void)
{
	C = reset_bit(C, 6);
}

/* RES 6,D */
static void CB_op0xB2(void)
{
	D = reset_bit(D, 6);
}

/* RES 6,E */
static void CB_op0xB3(void)
{
	E = reset_bit(E, 6);
}

/* RES 6,H */
static void CB_op0xB4(void)
{
	H = reset_bit(H, 6);
}

/* RES 6,L */
static void CB_op0xB5(void)
{
	L = reset_bit(L, 6);
}

/* RES 6,(HL) */
static void CB_op0xB6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 6);
	cpu_write_mem(address, reg);
}

/* RES 6,A */
static void CB_op0xB7(void)
{
	A = reset_bit(A, 6);
}

/* RES 7,B */
static void CB_op0xB8(void)
{
	B = reset_bit(B, 7);
}

/* RES 7,C */
static void CB_op0xB9(void)
{
	C = reset_bit(C, 7);
}

/* RES 7,D */
static void CB_op0xBA(void)
{
	D = reset_bit(D, 7);
}

/* RES 7,E */
static void CB_op0xBB(void)
{
	E = reset_bit(E, 7);
}

/* RES 7,H */
static void CB_op0xBC(void)
{
	H = reset_bit(H, 7);
}

/* RES 7,L */
static void CB_op0xBD(void)
{
	L = reset_bit(L, 7);
}

/* RES 7,(HL) */
static void CB_op0xBE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = reset_bit(reg, 7);
	cpu_write_mem(address, reg);
}

/* RES 7,A */
static void CB_op0xBF(void)
{
	A = reset_bit(A, 7);
}

/* SET 0,B */
static void CB_op0xC0(void)
{
	B = set_bit(B, 0);
}

/* SET 0,C */
static void CB_op0xC1(void)
{
	C = set_bit(C, 0);
}

/* SET 0,D */
static void CB_op0xC2(void)
{
	D = set_bit(D, 0);
}

/* SET 0,E */
static void CB_op0xC3(void)
{
	E = set_bit(E, 0);
}

/* SET 0,H */
static void CB_op0xC4(void)
{
	H = set_bit(H, 0);
}

/* SET 0,L */
static void CB_op0xC5(void)
{
	L = set_bit(L, 0);
}

/* SET 0,(HL) */
static void CB_op0xC6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 0);
	cpu_write_mem(address, reg);
}

/* SET 0,A */
static void CB_op0xC7(void)
{
	A = set_bit(A, 0);
}

/* SET 1,B */
static void CB_op0xC8(void)
{
	B = set_bit(B, 1);
}

/* SET 1,C */
static void CB_op0xC9(void)
{
	C = set_bit(C, 1);
}

/* SET 1,D */
static void CB_op0xCA(void)
{
	D = set_bit(D, 1);
}

/* SET 1,E */
static void CB_op0xCB(void)
{
	E = set_bit(E, 1);
}

/* SET 1,H */
static void CB_op0xCC(void)
{
	H = set_bit(H, 1);
}

/* SET 1,L */
static void CB_op0xCD(void)
{
	L = set_bit(L, 1);
}

/* SET 1,(HL) */
static void CB_op0xCE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 1);
	cpu_write_mem(address, reg);
}

/* SET 1,A */
static void CB_op0xCF(void)
{
	A = set_bit(A, 1);
}

/* SET 2,B */
static void CB_op0xD0(void)
{
	B = set_bit(B, 2);
}

/* SET 2,C */
static void CB_op0xD1(void)
{
	C = set_bit(C, 2);
}

/* SET 2,D */
static void CB_op0xD2(void)
{
	D = set_bit(D, 2);
}

/* SET 2,E */
static void CB_op0xD3(void)
{
	E = set_bit(E, 2);
}

/* SET 2,H */
static void CB_op0xD4(void)
{
	H = set_bit(H, 2);
}

/* SET 2,L */
static void CB_op0xD5(void)
{
	L = set_bit(L, 2);
}

/* SET 2,(HL) */
static void CB_op0xD6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 2);
	cpu_write_mem(address, reg);
}

/* SET 2,A */
static void CB_op0xD7(void)
{
	A = set_bit(A, 2);
}

/* SET 3,B */
static void CB_op0xD8(void)
{
	B = set_bit(B, 3);
}

/* SET 3,C */
static void CB_op0xD9(void)
{
	C = set_bit(C, 3);
}

/* SET 3,D */
static void CB_op0xDA(void)
{
	D = set_bit(D, 3);
}

/* SET 3,E */
static void CB_op0xDB(void)
{
	E = set_bit(E, 3);
}

/* SET 3,H */
static void CB_op0xDC(void)
{
	H = set_bit(H, 3);
}

/* SET 3,L */
static void CB_op0xDD(void)
{
	L = set_bit(L, 3);
}

/* SET 3,(HL) */
static void CB_op0xDE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 3);
	cpu_write_mem(address, reg);
}

/* SET 3,A */
static void CB_op0xDF(void)
{
	A = set_bit(A, 3);
}

/* SET 4,B */
static void CB_op0xE0(void)
{
	B = set_bit(B, 4);
}

/* SET 4,C */
static void CB_op0xE1(void)
{
	C = set_bit(C, 4);
}

/* SET 4,D */
static void CB_op0xE2(void)
{
	D = set_bit(D, 4);
}

/* SET 4,E */
static void CB_op0xE3(void)
{
	E = set_bit(E, 4);
}

/* SET 4,H */
static void CB_op0xE4(void)
{
	H = set_bit(H, 4);
}

/* SET 4,L */
static void CB_op0xE5(void)
{
	L = set_bit(L, 4);
}

/* SET 4,(HL) */
static void CB_op0xE6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 4);
	cpu_write_mem(address, reg);
}

/* SET 4,A */
static void CB_op0xE7(void)
{
	A = set_bit(A, 4);
}

/* SET 5,B */
static void CB_op0xE8(void)
{
	B = set_bit(B, 5);
}

/* SET 5,C */
static void CB_op0xE9(void)
{
	C = set_bit(C, 5);
}

/* SET 5,D */
static void CB_op0xEA(void)
{
	D = set_bit(D, 5);
}

/* SET 5,E */
static void CB_op0xEB(void)
{
	E = set_bit(E, 5);
}

/* SET 5,H */
static void CB_op0xEC(void)
{
	H = set_bit(H, 5);
}

/* SET 5,L */
static void CB_op0xED(void)
{
	L = set_bit(L, 5);
}

/* SET 5,(HL) */
static void CB_op0xEE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 5);
	cpu_write_mem(address, reg);
}

/* SET 5,A */
static void CB_op0xEF(void)
{
	A = set_bit(A, 5);
}

/* SET 6,B */
static void CB_op0xF0(void)
{
	B = set_bit(B, 6);
}

/* SET 6,C */
static void CB_op0xF1(void)
{
	C = set_bit(C, 6);
}

/* SET 6,D */
static void CB_op0xF2(void)
{
	D = set_bit(D, 6);
}

/* SET 6,E */
static void CB_op0xF3(void)
{
	E = set_bit(E, 6);
}

/* SET 6,H */
static void CB_op0xF4(void)
{
	H = set_bit(H, 6);
}

/* SET 6,L */
static void CB_op0xF5(void)
{
	L = set_bit(L, 6);
}

/* SET 6,(HL) */
static void CB_op0xF6(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 6);
	cpu_write_mem(address, reg);
}

/* SET 6,A */
static void CB_op0xF7(void)
{
	A = set_bit(A, 6);
}

/* SET 7,B */
static void CB_op0xF8(void)
{
	B = set_bit(B, 7);
}

/* SET 7,C */
static void CB_op0xF9(void)
{
	C = set_bit(C, 7);
}

/* SET 7,D */
static void CB_op0xFA(void)
{
	D = set_bit(D, 7);
}

/* SET 7,E */
static void CB_op0xFB(void)
{
	E = set_bit(E, 7);
}

/* SET 7,H */
static void CB_op0xFC(void)
{
	H = set_bit(H, 7);
}

/* SET 7,L */
static void CB_op0xFD(void)
{
	L = set_bit(L, 7);
}

/* SET 7,(HL) */
static void CB_op0xFE(void)
{
	u16 address = (H << 8) + L;
	u8 reg = cpu_read_mem(address);
	reg = set_bit(reg, 7);
	cpu_write_mem(address, reg);
}

/* SET 7,A */
static void CB_op0xFF(void)
{
	A = set_bit(A, 7);
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
	cb_optable[0x00] = CB_op0x00;
	cb_optable[0x01] = CB_op0x01;
	cb_optable[0x02] = CB_op0x02;
	cb_optable[0x03] = CB_op0x03;
	cb_optable[0x04] = CB_op0x04;
	cb_optable[0x05] = CB_op0x05;
	cb_optable[0x06] = CB_op0x06;
	cb_optable[0x07] = CB_op0x07;
	cb_optable[0x08] = CB_op0x08;
	cb_optable[0x09] = CB_op0x09;
	cb_optable[0x0A] = CB_op0x0A;
	cb_optable[0x0B] = CB_op0x0B;
	cb_optable[0x0C] = CB_op0x0C;
	cb_optable[0x0D] = CB_op0x0D;
	cb_optable[0x0E] = CB_op0x0E;
	cb_optable[0x0F] = CB_op0x0F;
	cb_optable[0x10] = CB_op0x10;
	cb_optable[0x11] = CB_op0x11;
	cb_optable[0x12] = CB_op0x12;
	cb_optable[0x13] = CB_op0x13;
	cb_optable[0x14] = CB_op0x14;
	cb_optable[0x15] = CB_op0x15;
	cb_optable[0x16] = CB_op0x16;
	cb_optable[0x17] = CB_op0x17;
	cb_optable[0x18] = CB_op0x18;
	cb_optable[0x19] = CB_op0x19;
	cb_optable[0x1A] = CB_op0x1A;
	cb_optable[0x1B] = CB_op0x1B;
	cb_optable[0x1C] = CB_op0x1C;
	cb_optable[0x1D] = CB_op0x1D;
	cb_optable[0x1E] = CB_op0x1E;
	cb_optable[0x1F] = CB_op0x1F;
	cb_optable[0x20] = CB_op0x20;
	cb_optable[0x21] = CB_op0x21;
	cb_optable[0x22] = CB_op0x22;
	cb_optable[0x23] = CB_op0x23;
	cb_optable[0x24] = CB_op0x24;
	cb_optable[0x25] = CB_op0x25;
	cb_optable[0x26] = CB_op0x26;
	cb_optable[0x27] = CB_op0x27;
	cb_optable[0x28] = CB_op0x28;
	cb_optable[0x29] = CB_op0x29;
	cb_optable[0x2A] = CB_op0x2A;
	cb_optable[0x2B] = CB_op0x2B;
	cb_optable[0x2C] = CB_op0x2C;
	cb_optable[0x2D] = CB_op0x2D;
	cb_optable[0x2E] = CB_op0x2E;
	cb_optable[0x2F] = CB_op0x2F;
	cb_optable[0x30] = CB_op0x30;
	cb_optable[0x31] = CB_op0x31;
	cb_optable[0x32] = CB_op0x32;
	cb_optable[0x33] = CB_op0x33;
	cb_optable[0x34] = CB_op0x34;
	cb_optable[0x35] = CB_op0x35;
	cb_optable[0x36] = CB_op0x36;
	cb_optable[0x37] = CB_op0x37;
	cb_optable[0x38] = CB_op0x38;
	cb_optable[0x39] = CB_op0x39;
	cb_optable[0x3A] = CB_op0x3A;
	cb_optable[0x3B] = CB_op0x3B;
	cb_optable[0x3C] = CB_op0x3C;
	cb_optable[0x3D] = CB_op0x3D;
	cb_optable[0x3E] = CB_op0x3E;
	cb_optable[0x3F] = CB_op0x3F;
	cb_optable[0x40] = CB_op0x40;
	cb_optable[0x41] = CB_op0x41;
	cb_optable[0x42] = CB_op0x42;
	cb_optable[0x43] = CB_op0x43;
	cb_optable[0x44] = CB_op0x44;
	cb_optable[0x45] = CB_op0x45;
	cb_optable[0x46] = CB_op0x46;
	cb_optable[0x47] = CB_op0x47;
	cb_optable[0x48] = CB_op0x48;
	cb_optable[0x49] = CB_op0x49;
	cb_optable[0x4A] = CB_op0x4A;
	cb_optable[0x4B] = CB_op0x4B;
	cb_optable[0x4C] = CB_op0x4C;
	cb_optable[0x4D] = CB_op0x4D;
	cb_optable[0x4E] = CB_op0x4E;
	cb_optable[0x4F] = CB_op0x4F;
	cb_optable[0x50] = CB_op0x50;
	cb_optable[0x51] = CB_op0x51;
	cb_optable[0x52] = CB_op0x52;
	cb_optable[0x53] = CB_op0x53;
	cb_optable[0x54] = CB_op0x54;
	cb_optable[0x55] = CB_op0x55;
	cb_optable[0x56] = CB_op0x56;
	cb_optable[0x57] = CB_op0x57;
	cb_optable[0x58] = CB_op0x58;
	cb_optable[0x59] = CB_op0x59;
	cb_optable[0x5A] = CB_op0x5A;
	cb_optable[0x5B] = CB_op0x5B;
	cb_optable[0x5C] = CB_op0x5C;
	cb_optable[0x5D] = CB_op0x5D;
	cb_optable[0x5E] = CB_op0x5E;
	cb_optable[0x5F] = CB_op0x5F;
	cb_optable[0x60] = CB_op0x60;
	cb_optable[0x61] = CB_op0x61;
	cb_optable[0x62] = CB_op0x62;
	cb_optable[0x63] = CB_op0x63;
	cb_optable[0x64] = CB_op0x64;
	cb_optable[0x65] = CB_op0x65;
	cb_optable[0x66] = CB_op0x66;
	cb_optable[0x67] = CB_op0x67;
	cb_optable[0x68] = CB_op0x68;
	cb_optable[0x69] = CB_op0x69;
	cb_optable[0x6A] = CB_op0x6A;
	cb_optable[0x6B] = CB_op0x6B;
	cb_optable[0x6C] = CB_op0x6C;
	cb_optable[0x6D] = CB_op0x6D;
	cb_optable[0x6E] = CB_op0x6E;
	cb_optable[0x6F] = CB_op0x6F;
	cb_optable[0x70] = CB_op0x70;
	cb_optable[0x71] = CB_op0x71;
	cb_optable[0x72] = CB_op0x72;
	cb_optable[0x73] = CB_op0x73;
	cb_optable[0x74] = CB_op0x74;
	cb_optable[0x75] = CB_op0x75;
	cb_optable[0x76] = CB_op0x76;
	cb_optable[0x77] = CB_op0x77;
	cb_optable[0x78] = CB_op0x78;
	cb_optable[0x79] = CB_op0x79;
	cb_optable[0x7A] = CB_op0x7A;
	cb_optable[0x7B] = CB_op0x7B;
	cb_optable[0x7C] = CB_op0x7C;
	cb_optable[0x7D] = CB_op0x7D;
	cb_optable[0x7E] = CB_op0x7E;
	cb_optable[0x7F] = CB_op0x7F;
	cb_optable[0x80] = CB_op0x80;
	cb_optable[0x81] = CB_op0x81;
	cb_optable[0x82] = CB_op0x82;
	cb_optable[0x83] = CB_op0x83;
	cb_optable[0x84] = CB_op0x84;
	cb_optable[0x85] = CB_op0x85;
	cb_optable[0x86] = CB_op0x86;
	cb_optable[0x87] = CB_op0x87;
	cb_optable[0x88] = CB_op0x88;
	cb_optable[0x89] = CB_op0x89;
	cb_optable[0x8A] = CB_op0x8A;
	cb_optable[0x8B] = CB_op0x8B;
	cb_optable[0x8C] = CB_op0x8C;
	cb_optable[0x8D] = CB_op0x8D;
	cb_optable[0x8E] = CB_op0x8E;
	cb_optable[0x8F] = CB_op0x8F;
	cb_optable[0x90] = CB_op0x90;
	cb_optable[0x91] = CB_op0x91;
	cb_optable[0x92] = CB_op0x92;
	cb_optable[0x93] = CB_op0x93;
	cb_optable[0x94] = CB_op0x94;
	cb_optable[0x95] = CB_op0x95;
	cb_optable[0x96] = CB_op0x96;
	cb_optable[0x97] = CB_op0x97;
	cb_optable[0x98] = CB_op0x98;
	cb_optable[0x99] = CB_op0x99;
	cb_optable[0x9A] = CB_op0x9A;
	cb_optable[0x9B] = CB_op0x9B;
	cb_optable[0x9C] = CB_op0x9C;
	cb_optable[0x9D] = CB_op0x9D;
	cb_optable[0x9E] = CB_op0x9E;
	cb_optable[0x9F] = CB_op0x9F;
	cb_optable[0xA0] = CB_op0xA0;
	cb_optable[0xA1] = CB_op0xA1;
	cb_optable[0xA2] = CB_op0xA2;
	cb_optable[0xA3] = CB_op0xA3;
	cb_optable[0xA4] = CB_op0xA4;
	cb_optable[0xA5] = CB_op0xA5;
	cb_optable[0xA6] = CB_op0xA6;
	cb_optable[0xA7] = CB_op0xA7;
	cb_optable[0xA8] = CB_op0xA8;
	cb_optable[0xA9] = CB_op0xA9;
	cb_optable[0xAA] = CB_op0xAA;
	cb_optable[0xAB] = CB_op0xAB;
	cb_optable[0xAC] = CB_op0xAC;
	cb_optable[0xAD] = CB_op0xAD;
	cb_optable[0xAE] = CB_op0xAE;
	cb_optable[0xAF] = CB_op0xAF;
	cb_optable[0xB0] = CB_op0xB0;
	cb_optable[0xB1] = CB_op0xB1;
	cb_optable[0xB2] = CB_op0xB2;
	cb_optable[0xB3] = CB_op0xB3;
	cb_optable[0xB4] = CB_op0xB4;
	cb_optable[0xB5] = CB_op0xB5;
	cb_optable[0xB6] = CB_op0xB6;
	cb_optable[0xB7] = CB_op0xB7;
	cb_optable[0xB8] = CB_op0xB8;
	cb_optable[0xB9] = CB_op0xB9;
	cb_optable[0xBA] = CB_op0xBA;
	cb_optable[0xBB] = CB_op0xBB;
	cb_optable[0xBC] = CB_op0xBC;
	cb_optable[0xBD] = CB_op0xBD;
	cb_optable[0xBE] = CB_op0xBE;
	cb_optable[0xBF] = CB_op0xBF;
	cb_optable[0xC0] = CB_op0xC0;
	cb_optable[0xC1] = CB_op0xC1;
	cb_optable[0xC2] = CB_op0xC2;
	cb_optable[0xC3] = CB_op0xC3;
	cb_optable[0xC4] = CB_op0xC4;
	cb_optable[0xC5] = CB_op0xC5;
	cb_optable[0xC6] = CB_op0xC6;
	cb_optable[0xC7] = CB_op0xC7;
	cb_optable[0xC8] = CB_op0xC8;
	cb_optable[0xC9] = CB_op0xC9;
	cb_optable[0xCA] = CB_op0xCA;
	cb_optable[0xCB] = CB_op0xCB;
	cb_optable[0xCC] = CB_op0xCC;
	cb_optable[0xCD] = CB_op0xCD;
	cb_optable[0xCE] = CB_op0xCE;
	cb_optable[0xCF] = CB_op0xCF;
	cb_optable[0xD0] = CB_op0xD0;
	cb_optable[0xD1] = CB_op0xD1;
	cb_optable[0xD2] = CB_op0xD2;
	cb_optable[0xD3] = CB_op0xD3;
	cb_optable[0xD4] = CB_op0xD4;
	cb_optable[0xD5] = CB_op0xD5;
	cb_optable[0xD6] = CB_op0xD6;
	cb_optable[0xD7] = CB_op0xD7;
	cb_optable[0xD8] = CB_op0xD8;
	cb_optable[0xD9] = CB_op0xD9;
	cb_optable[0xDA] = CB_op0xDA;
	cb_optable[0xDB] = CB_op0xDB;
	cb_optable[0xDC] = CB_op0xDC;
	cb_optable[0xDD] = CB_op0xDD;
	cb_optable[0xDE] = CB_op0xDE;
	cb_optable[0xDF] = CB_op0xDF;
	cb_optable[0xE0] = CB_op0xE0;
	cb_optable[0xE1] = CB_op0xE1;
	cb_optable[0xE2] = CB_op0xE2;
	cb_optable[0xE3] = CB_op0xE3;
	cb_optable[0xE4] = CB_op0xE4;
	cb_optable[0xE5] = CB_op0xE5;
	cb_optable[0xE6] = CB_op0xE6;
	cb_optable[0xE7] = CB_op0xE7;
	cb_optable[0xE8] = CB_op0xE8;
	cb_optable[0xE9] = CB_op0xE9;
	cb_optable[0xEA] = CB_op0xEA;
	cb_optable[0xEB] = CB_op0xEB;
	cb_optable[0xEC] = CB_op0xEC;
	cb_optable[0xED] = CB_op0xED;
	cb_optable[0xEE] = CB_op0xEE;
	cb_optable[0xEF] = CB_op0xEF;
	cb_optable[0xF0] = CB_op0xF0;
	cb_optable[0xF1] = CB_op0xF1;
	cb_optable[0xF2] = CB_op0xF2;
	cb_optable[0xF3] = CB_op0xF3;
	cb_optable[0xF4] = CB_op0xF4;
	cb_optable[0xF5] = CB_op0xF5;
	cb_optable[0xF6] = CB_op0xF6;
	cb_optable[0xF7] = CB_op0xF7;
	cb_optable[0xF8] = CB_op0xF8;
	cb_optable[0xF9] = CB_op0xF9;
	cb_optable[0xFA] = CB_op0xFA;
	cb_optable[0xFB] = CB_op0xFB;
	cb_optable[0xFC] = CB_op0xFC;
	cb_optable[0xFD] = CB_op0xFD;
	cb_optable[0xFE] = CB_op0xFE;
	cb_optable[0xFF] = CB_op0xFF;
}
