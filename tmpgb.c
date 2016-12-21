#include <stdio.h>
#include <stdlib.h>

static void usage(void)
{
	fprintf(stderr, "usage: tmpgb <file>");
	exit(EXIT_FAILURE);
}

static void load_rom(const char *rom)
{
	FILE *fp;
	unsigned char c;
	fp = fopen(rom, "rb");
	
	if (fp == NULL) {
		fprintf(stderr, "Could not open %s", rom);
		exit(EXIT_FAILURE);
	}
	
	while ((c = fgetc(fp)) != EOF) {
		printf("%x\n", c);
	}
}

int main(int argc, char **argv)
{
	char *rom;
	
	if (argc != 2)
		usage();
	
	rom = argv[1];
	load_rom(rom);
}