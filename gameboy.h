#ifndef GAMEBOY_H
#define GAMEBOY_H
typedef unsigned char u8;
typedef unsigned short u16;

#ifdef DEBUG
void log_msg(const char *fmt, ...);
#else
void log_msg()
{}
#endif
#endif
