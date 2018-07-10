#ifndef MEMORY_H
#define MEMORY_H
enum {
	ROM,
	MBC1
} mode;

void read_rom(const unsigned char *buffer, int count);
void write_memory(unsigned short addr, unsigned char value);
unsigned char read_memory(unsigned short addr);
void write_ly(u8 v);
void write_joypad(u8 v);
void write_stat(u8 v);
int init_memory(void);
#endif
