#include <stdint.h>

struct reg {
	uint8_t high;
	uint8_t low;
} AF, BC, DE, HL;

uint16_t PC;
uint16_t SP;

uint16_t fetch_16bit_data(void);

uint8_t fetch_8bit_data(void);

int rotate_left(uint8_t reg);

void set_zflag(void);

void set_nflag(void);

void set_hflag(void);

void set_cflag(void);

void reset_zflag(void);

void reset_nflag(void);

void reset_hflag(void);

void reset_cflag(void);
