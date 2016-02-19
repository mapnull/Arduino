#ifndef ARDUINO

#include <stdio.h>
#include "dtostrf.h"

char *dtostrf(float f, int width, int decimals, char *result)
{
	sprintf(result,"%*.*f", width, decimals, f);
	return result;
}

#endif
