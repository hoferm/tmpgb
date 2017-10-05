enum {
	INT_VBLANK = 0x40,
	INT_LCD = 0x48,
	INT_TIMER = 0x50,
	INT_SERIAL = 0x58,
	INT_JOYPAD = 0x60
};

void set_ime(int enabled);

int get_ime(void);

int execute_interrupt(void);

void request_interrupt(int);
