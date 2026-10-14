#ifdef TRACE3DS

#include <stdarg.h>
#include <stdio.h>
#include <3ds.h>

#include "3dstrace.h"

#define TRACE_FILE      "sdmc:/virtuanes_trace.txt"
#define TRACE_MAX_LINES 2000

int trace3dsDetail = 0;
static int traceLines = 0;
static u64 traceStartTick = 0;

void trace3ds(const char *format, ...)
{
    if (traceLines >= TRACE_MAX_LINES)
        return;

    FILE *fp = fopen(TRACE_FILE, traceLines == 0 ? "w" : "a");
    if (!fp)
        return;

    u64 tick = svcGetSystemTick();
    if (traceLines == 0)
        traceStartTick = tick;
    fprintf(fp, "%8.3f ", (double)(tick - traceStartTick) / SYSCLOCK_ARM11);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);

    fputc('\n', fp);
    fclose(fp);
    traceLines++;
}

#endif
