#ifndef GAMEBOY_H
#define GAMEBOY_H

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

void usagef(const char *err, ...);
void die_errno(const char *err, ...);
void die(const char *err, ...);
void errorf(const char *err, ...);

u8 set_bit(u8 val, int bit);
u8 reset_bit(u8 val, int bit);
int get_bit(u8 val, int bit);

struct cpu_info {
	u16 *PC;
	u16 *SP;

	u8 *B;
	u8 *C;
	u8 *D;
	u8 *E;
	u8 *H;
	u8 *L;
	u8 *A;
	u8 *F;
	u64 *instr_count;
};

void cpu_debug_info(struct cpu_info *cpu);
#endif
