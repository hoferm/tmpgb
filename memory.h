enum {
	ROM,
	MBC1
} mode;

void read_rom(const unsigned char *buffer, int count);

void write_memory(unsigned short addr, unsigned char value);

unsigned char read_memory(unsigned short addr);

unsigned char *get_vram(void);

unsigned char *get_div(void);
unsigned char *get_tima(void);
unsigned char *get_tma(void);
unsigned char *get_tac(void);

int init_memory(void);
