#include "utils.h"
#include <stdio.h>   // for snprintf
#include <stdlib.h>  // for strtol
#include <errno.h>
#include <limits.h>

void int_to_str(int num, char *buf, size_t buf_size) {
    // snprintf ensures no buffer overflow
    snprintf(buf, buf_size, "%d", num);
}

int str_to_int(const char *str, int *out) {
    char *endptr;
    errno = 0;

    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        return -1; // overflow/underflow
    }
    if (*endptr != '\0') {
        return -2; // invalid characters in string
    }

    *out = (int)val;
    return 0;
}
