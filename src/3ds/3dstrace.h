#ifndef _3DSTRACE_H_
#define _3DSTRACE_H_

//---------------------------------------------------------
// A `make TRACE=1` build writes a step-by-step log of start-up,
// ROM loading and the first frames to sdmc:/virtuanes_trace.txt,
// for debugging on real hardware, where there's no emulator log.
// The file is reopened for every line, so the log survives a
// freeze. Call it from the main thread only: the sound mixing
// thread isn't set up for stdio.
//---------------------------------------------------------
#ifdef TRACE3DS
void trace3ds(const char *format, ...);
extern int trace3dsDetail;      // nonzero: also log the per-frame steps
extern volatile int trace3dsProgress;   // bumped by the main thread every frame

// Starts a low-priority thread that logs the app's state every 2 seconds
// while it's in the background or closing, to show where it stops.
void trace3dsStartWatchdog();
#define TRACE3DS_LOG(...)       trace3ds(__VA_ARGS__)
#define TRACE3DS_DETAIL(...)    do { if (trace3dsDetail) trace3ds(__VA_ARGS__); } while (0)
#else
#define TRACE3DS_LOG(...)       ((void)0)
#define TRACE3DS_DETAIL(...)    ((void)0)
#endif

#endif
