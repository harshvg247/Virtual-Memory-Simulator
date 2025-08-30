#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>  // for size_t

// Convert int → string (buffer must be provided by caller)
void int_to_str(int num, char *buf, size_t buf_size);

// Convert string → int
// returns 0 on success
//        -1 on overflow/underflow
//        -2 if string contains invalid chars
int str_to_int(const char *str, int *out);

#endif // UTILS_H
