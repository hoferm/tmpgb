#ifndef GAMEBOY_H
#define GAMEBOY_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u64;

void usagef(const char *err, ...);
void die_errno(const char *err, ...);
void die(const char *err, ...);
void error(const char *err, ...);

u8 set_bit(u8 val, int bit);
u8 reset_bit(u8 val, int bit);
int get_bit(u8 val, int bit);

#ifdef DEBUG
void log_close(void);
void log_msg(const char *fmt, ...);
int log_init(const char *file);
#else
void log_close() {}
void log_msg() {}
int log_init() {}
#endif
#endif
