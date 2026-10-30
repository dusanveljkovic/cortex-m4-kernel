#ifndef _UPRINTF_H
#define _UPRINTF_H

#include "stdarg.h"
#include "stdint.h"

// unprivileged printf: format locally then write via sys_console_write()
int uprintf(const char *fmt, ...);

#endif
