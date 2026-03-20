#include "esp_log.h"

#include <stdarg.h>
#include <stdio.h>

void simulator_log_write(const char* level, const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (!tag)
    {
        tag = "sim";
    }
    fprintf(stdout, "[%s] %s: ", level ? level : "?", tag);
    vfprintf(stdout, fmt, args);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(args);
}
