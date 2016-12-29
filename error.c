#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void exit_error(void)
{
	switch(errnr) {
	case VALIDATION_ERR:
		fprintf(stderr, "Invalid ROM");
		exit(VALIDATION_ERR);
		break;
	default:
		fprintf(stderr, "Error with switch statement");
	}
}
