#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <iostream>
#include <string>

/* Print information about a system error and quits. */
[[noreturn]] void syserr(const char* fmt, ...);

/* Print information about an error and quits. */
[[noreturn]] void fatal(const char* fmt, ...);

// Print information about an error and return.
void error(const char* fmt, ...);

#endif
