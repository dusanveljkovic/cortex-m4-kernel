#include "../include/utils.h"
#include "stdint.h"

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return (uint8_t)*s1 - (uint8_t)*s2;
}

int strlen(const char *str) {
  int count = 0;
  while (*str++ != '\0')
    count++;
  return count;
}
