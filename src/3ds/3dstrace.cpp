#ifdef TRACE3DS

#include <stdarg.h>
#include <stdio.h>
#include <3ds.h>

#include "3dsemu.h"
#include "3dsexit.h"
#include "3dssound.h"
#include "3dstrace.h"

#define TRACE_FILE      "sdmc:/virtuanes_trace.txt"
#define TRACE_MAX_LINES 2000

int trace3dsDetail = 0;
volatile int trace3dsProgress = 0;
static int traceLines = 0;
static u64 traceStartTick = 0;
static LightLock traceLock;

void trace3ds(const char *format, ...)
{
    if (traceLines == 0)
        LightLock_Init(&traceLock);     // the first line comes from the main thread at start-up
    LightLock_Lock(&traceLock);

    FILE *fp = traceLines < TRACE_MAX_LINES ? fopen(TRACE_FILE, "a") : NULL;
    if (fp)
    {
        u64 tick = svcGetSystemTick();
        if (traceLines == 0)
        {
            traceStartTick = tick;
            fputs("=== VirtuaNES started ===\n", fp);
        }
        fprintf(fp, "%8.3f ", (double)(tick - traceStartTick) / SYSCLOCK_ARM11);

        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);

        fputc('\n', fp);
        fclose(fp);
        traceLines++;
    }

    LightLock_Unlock(&traceLock);
}

static void traceWatchdog(void *arg)
{
    int lastProgress = -1;
    while (true)
    {
        svcSleepThread(2000000000LL);
        if (!aptIsActive() || aptShouldClose() || appExiting)
        {
            int progress = trace3dsProgress;
            trace3ds("watchdog: active=%d shouldClose=%d appExiting=%d state=%d main %s, isPlaying=%d mixed=%d",
                aptIsActive(), aptShouldClose(), appExiting, emulator.emulatorState,
                progress != lastProgress ? "running" : "not running a frame",
                snd3DS.isPlaying, snd3dsMixCount);
            lastProgress = progress;
        }
        else
            lastProgress = trace3dsProgress;
    }
}

void trace3dsStartWatchdog()
{
    // Lowest priority, on the app core: it only runs when the main thread waits.
    threadCreate(traceWatchdog, NULL, 0x2000, 0x3F, -2, true);
}

#endif
