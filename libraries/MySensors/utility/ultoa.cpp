#ifndef ARDUINO

#include "ultoa.h"

char *ultoa(long num, char *str, int radix)
{
  unsigned long value;
  char *sp = str;
  char *sp2;
  
  value = num;

  /* Store sign at start of buffer for negative base-10 values */
  if (10 == radix && 0 > num) {
    *sp++ = '-';
    value = -num;
  }

  sp2 = sp;

  do {
    char rem = value % radix;
    value /= radix;
    if (10 > rem) {
      *sp++ = '0' + rem;
    } else {
      *sp++ = 'A' + rem - 10;
    }
  } while (0 < value);

  /* Mark end of string */
  *sp-- = 0;

  /* Reverse string contents (excluding sign) in place */
  while (sp2 < sp) {
    char tmp = *sp2;
    *sp2++ = *sp;
    *sp-- = tmp;
  }

  return str;
}

#endif
