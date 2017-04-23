#ifndef GAMEBOY_H
#define GAMEBOY_H

#define BIT_7(reg) (reg & 0x80)
#define BIT_6(reg) (reg & 0x40)
#define BIT_5(reg) (reg & 0x20)
#define BIT_4(reg) (reg & 0x10)
#define BIT_3(reg) (reg & 0x08)
#define BIT_2(reg) (reg & 0x04)
#define BIT_1(reg) (reg & 0x02)
#define BIT_0(reg) (reg & 0x01)

#define RESET_BIT_4(reg) (reg & 0x0F)
#define RESET_BIT_3(reg) (reg & 0x17)
#define RESET_BIT_2(reg) (reg & 0x1B)
#define RESET_BIT_1(reg) (reg & 0x1D)
#define RESET_BIT_0(reg) (reg & 0x1E)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u64;

#ifdef DEBUG
void log_msg(const char *fmt, ...);
#else
void log_msg() {}
#endif
#endif
